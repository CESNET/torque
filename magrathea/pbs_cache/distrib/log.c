/*
 *
 *      log.c
 *      
 *      Copyright 2009 Jaroslav Tomeček
 *      
 * 
 * 		Paxos and database log mechanism
 */

#include "main.h"
#include <sys/stat.h>
#include "../memory.h"

int file, zfile;
char * filename, * zero_fn, * sst_fn;
extern struct node_struct node;
pthread_mutex_t log_mutex;
int n_rounds;
long long instance_proposed_sug = -1, round_sug = -1;

typedef struct metric_ {
    TAILQ_ENTRY(metric_) metricq;
    char * name;
} metric_rec;

typedef struct snt_ {
    long long round, instance;
} snapshot_t;

snapshot_t * snt;

TAILQ_HEAD(lst_m, metric_) lst_m;

int hash_check(char * filename);
int write_hash(char * filename);
static void * updater_thread(void *args);
static void * snapshot_thr(void * args);
int insert_metric_rec(char *msg);
int process_snapshot();
int _init_paxos_log(struct node_struct * node);
int _write_log_record(char * msg, PAXOS_BOOL master, PAXOS_BOOL commit, struct node_struct * node);
int _write_metric_snap(char * mtr_name, char * value, struct node_struct * node);
int _write_log_out_record(char * msg, struct node_struct *node);
int _write_snapshot_mark(char * rec, struct node_struct * node);
static PAXOS_BOOL snap_lock;

void print_db_state() {
    FILE * inf;
    metric_rec * np;

    inf = get_report_desc();
    TAILQ_FOREACH(np, &lst_m, metricq) {
        memory_dump(np->name, inf, 1);
    }
}

/*Tests whether the file 'filename exists*/
int exists(char * filename) {
    FILE * inf;
    if ((inf = fopen(filename, "r")) == NULL) {
        return -1;
    }
    fclose(inf);
    return 0;
}

void lock_snapshotting() {
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if(_PAXOS_DEBUG) pax_log(LOG_DEBUG, "snap locked\n");
    snap_lock = BTRUE;
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
}

void unlock_snapshotting() {
    if (snap_lock == BTRUE) {
        if (pthread_mutex_lock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        if(_PAXOS_DEBUG) pax_log(LOG_DEBUG, "snap unlocked\n");
        snap_lock = BFALSE;
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
    }
}

static PAXOS_BOOL get_snap_lock() {
    PAXOS_BOOL lock;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if(_PAXOS_DEBUG>3) pax_log(LOG_DEBUG, "Get snap %d\n", snap_lock);
    lock = snap_lock;
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return lock;
}

int init_paxos_log() {
    return _init_paxos_log(&node);
};
/*
 * Logs initialization
 * name: _init_paxos_log
 */
int _init_paxos_log(struct node_struct * node) {
    int oflag;
    int stat_ret, write_ret, hash_ret, ret;
    int mutex_ret;
    pthread_t ssht_thr;

    if (_PAXOS_DEBUG) pax_log(LOG_DEBUG, "Init_log\n");

    mutex_ret = pthread_mutex_init(&log_mutex, NULL);
    if (mutex_ret != 0) {
        pax_log(LOG_ERR,"log_init:pthread_mutex: %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    snt = (snapshot_t*) malloc(sizeof (snapshot_t));
    snt->instance = -1;
    snt->round = -1;
    TAILQ_INIT(&lst_m);

    snap_lock = BFALSE;

    filename = PAXOS_LOG_FILE;
    zero_fn = PAXOS_LOG_FILE BASE_LOG_EXT;
    sst_fn = PAXOS_LOG_FILE SNAPSHOT_EXT;
    stat_ret = exists(filename);
    oflag = O_RDWR | O_APPEND;

    if (stat_ret != 0) {
        oflag |= O_CREAT;
    }

    if (!(file = open(filename, oflag, S_IWUSR | S_IRUSR))) {
        pax_log(LOG_ERR,"Opening log file: %s\n", strerror(errno));
        ret = -2;
        goto err;
    }

    /*Log file*/
    if (stat_ret == 0) {
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Base log exists\n");
        if (pthread_mutex_lock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        hash_ret = hash_check(filename);
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        if (hash_ret != 0) {
            pax_log(LOG_ERR, "Hash check failed  - base log\n");
            if (pthread_mutex_lock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            node->non_voting = BTRUE;
            if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "paxos log init  - voting forbidden\n");
            if (pthread_mutex_unlock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
        }
    } else {
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "New hash written - base log\n");
        write_ret = write_hash(filename);
        if (write_ret != 0) {
            ret = -2;
            goto err;
        }
    }

    /*Base log file*/
    stat_ret = exists(zero_fn);
    oflag = O_RDWR | O_APPEND;
    if (stat_ret != 0) {       
        oflag |= O_CREAT;
    }
    if (!(zfile = open(zero_fn, oflag, S_IWUSR | S_IRUSR))) {
        pax_log(LOG_ERR,"Opening zero log file: %s\n", strerror(errno));
        ret = -2;
        goto err;
    }


    if (stat_ret == 0) {
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Zero log exists\n");
        if (pthread_mutex_lock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        hash_ret = hash_check(zero_fn);
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }

        if (hash_ret != 0) {
            pax_log(LOG_ERR, "Zero log hash failed\n");
            if (pthread_mutex_lock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            node->non_voting = BTRUE;
            if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "paxos zero log init  - voting forbidden\n");
            if (pthread_mutex_unlock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
        }
    } else {
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Zero log - new hash written\n");
        write_ret = write_hash(zero_fn);
        if (write_ret != 0) {
            ret = -2;
            goto err;
        }
    }

    if ((ret = pthread_create(&ssht_thr, NULL, snapshot_thr, NULL)) != 0) {
        pax_log(LOG_ERR,"init_log:can't create snapshot thread: %s\n", strerror(errno));
        goto err;
    }
    n_rounds = 0;
    return 0;
err:

    close(file);
    close(zfile);
    return ret;
}

/*
 * Writes one record into the main log.
 * name: write_log_record
 * @param msg Message to be written
 * @param master If the Master is writtig the log then the "MAS:" prefix is used
 * @param commit If the transaction was commited
 *
 * Needs node lock to be aquired
 */
int write_log_record(char * msg, PAXOS_BOOL master, PAXOS_BOOL commit) {
    return _write_log_record(msg, master, commit, &node);
}

int _write_log_record(char * msg, PAXOS_BOOL master, PAXOS_BOOL commit, struct node_struct * node) {
    int write_ret, ret, hash_ret;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }

    /*Prefixes*/
    if (master == BTRUE) write(file, "MAS:", strlen("MAS:"));
    else if (master == 0) write(file, "CLI:", strlen("CLI:"));

    write(file, msg, strlen(msg));

    if (msg[strlen(msg) - 1] != ';')
        write(file, ";", strlen(";"));

    write(file, "\n", strlen("\n"));
    sync();
    write_ret = write_hash(filename);

    if (commit) {
        n_rounds++; /*If the transaction was commited increase the number
                            of records*/
        if(_PAXOS_DEBUG>1)pax_log(LOG_DEBUG,"Number of records in the log: INREASE n_rounds %d\n", n_rounds);
    }

    if (write_ret != 0) {
        pax_log(LOG_ERR, "Write_log_record %s: Hash cannot be written.\n", msg);
        ret = -2;
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        goto err;
    }

    hash_ret = hash_check(filename);
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Wrong hash!\n", filename);
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "write_rec base log checked  - voting forbidden\n");
        node->non_voting = BTRUE;
    }
    return 0;
err:

    return ret;
}

/*
 * Writes snapshot file for given metric
 */
int write_metric_snap(char * mtr_name, char * value) {
    return _write_metric_snap(mtr_name,value, &node);
}

int _write_metric_snap(char * mtr_name, char * value, struct node_struct * node ) {
    int size, hash_ret;
    char * sn_fn;
    FILE * inf;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    size = strlen(mtr_name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 2;
    sn_fn = (char *) malloc(size * sizeof (char));
    snprintf(sn_fn, size, "%s%s%s", PAXOS_DIR, mtr_name, SNAPSHOT_EXT);
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Making snapshot of %s to %s\n", mtr_name, sn_fn);
    if ((inf = fopen(sn_fn, "a")) == NULL) {
        pax_log(LOG_ERR,"cannot open snapshot file for writting: %s\n", strerror(errno));
        free(sn_fn);
        exit(-1);
    }
    fprintf(inf, "%s", value);
    fclose(inf);
    write_hash(sn_fn);
    hash_ret = hash_check(sn_fn);
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Wrong hash!\n", sn_fn);
        if (pthread_mutex_lock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        node->non_voting = BTRUE;
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "write metric snap base log checked  - voting forbidden\n");
        if (pthread_mutex_unlock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
    }    
    free(sn_fn);
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return 0;
}

/*
 * Writes one record into the basis (outdated) log.
 * name: write_log_out_record
 * @return
 */
int write_log_out_record(char * msg) {
    return _write_log_out_record(msg, & node);
}

int _write_log_out_record(char * msg, struct node_struct *node) {
    /*int write_ret, ret, hash_ret;*/

    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    write(zfile, msg, strlen(msg));
    write(zfile, ";", strlen(";"));
    write(zfile, "\n", strlen("\n"));
    sync();
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    return 0;

}

int write_log_out_hash(){
    int write_ret= 0;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    write_ret = write_hash(zero_fn);

    if (write_ret != 0) {
        pax_log(LOG_ERR, "Write_record: Hash cannot be written.\n");
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        return -1;
    }
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    return 0;
}

/*
 * Writes snapshot mark file
 * This file consists of two records - the snapshot instance and round number
 */
int write_snapshot_mark(char * rec) {
    return _write_snapshot_mark(rec, &node);
}

int _write_snapshot_mark(char * rec, struct node_struct * node) {
    FILE * inf;
    int hash_ret;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if ((inf = fopen(sst_fn, "w")) == NULL) {
        pax_log(LOG_ERR,"cannot truncate snapshot log file: %s\n", strerror(errno));
        exit(-1);
    }
    fprintf(inf, "%s", rec);
    fclose(inf);
    hash_ret = write_hash(sst_fn);
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "Cannot write hash for %s\n", sst_fn);
        exit(-1);
    }           
    hash_ret = hash_check(sst_fn);
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Wrong hash!\n", sst_fn);
        if (pthread_mutex_lock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        node->non_voting = BTRUE;
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "snapshot mark checked  - voting forbidden\n");
        if (pthread_mutex_unlock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
    }       
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return 0;
}

/*
 * Truncates all logs
 * name: clear_logs
 */
void clear_logs() {
    int ret, size;
    metric_rec * np;
    char *sn_fn;

    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    n_rounds=0;
    ret = ftruncate(file, 0);
    ret += ftruncate(zfile, 0);
    sync();
    if (ret != 0) {
        pax_log(LOG_ERR, "cannot truncate logs\n");
        exit(EX_OSERR);
    }

    ret += write_hash(filename);
    if (ret != 0) {
        pax_log(LOG_ERR, "cannot truncate logs\n");
        exit(EX_OSERR);
    }
    if (exists(sst_fn) == 0) {
        ret = unlink(sst_fn);
        if (ret != 0) {
            pax_log(LOG_ERR,"Error while snapshot base truncating: %s\n", strerror(errno));
            exit(EX_OSERR);
        }
        TAILQ_FOREACH(np, &lst_m, metricq) {
            size = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 2;
            sn_fn = (char *) malloc(size * sizeof (char));
            snprintf(sn_fn, size, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);
            if (exists(sn_fn) == 0) {
                ret = unlink(sn_fn);
                if (ret != 0) {
                    pax_log(LOG_ERR,"Error while snapshot base truncating: %s\n", strerror(errno));
                    exit(EX_OSERR);
                }
            }
            free(sn_fn);
        }
        if (pthread_mutex_unlock(&log_mutex) != 0) {

            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }

    }else {
        if (pthread_mutex_unlock(&log_mutex) != 0) {

            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }        
    }


}

/*
 * Checks the snaps log hash
 * The log_lock must be locked before entering
 */
int snaps_hash(struct node_struct * node) {
    int hash_ret = 0;
    metric_rec * np;
    char * name;
    int len = 0;

    TAILQ_FOREACH(np, &lst_m, metricq) {
        len = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 1;
        name = malloc(1+len * sizeof (char));
        sprintf(name, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);
        if (exists(name) == 0)
            hash_ret = hash_check(name);
        free(name);
        if (hash_ret != 0) {
            pax_log(LOG_ERR, "Snapshot %s corrupted!\n", np->name);
            break;
        }
    }
    if (hash_ret != 0) {
        node->non_voting = BTRUE;
        return 2;
    }
    return 0;
}

/*
 * Writes the snaps hash
 * name: snaps_write_hash
 * The log_lock must be locked before entering
 */
void snaps_write_hash() {
    int hash_ret = 0;
    metric_rec * np;
    char * name;
    int len = 0;

    TAILQ_FOREACH(np, &lst_m, metricq) {
        len = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 1;
        name = malloc(len * sizeof (char));
        sprintf(name, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);
        hash_ret = write_hash(name);
        if (hash_ret != 0) break;
        hash_ret = hash_check(name);
        if (hash_ret != 0) break;
        free(name);        
    }
    if (hash_ret != 0) {

        pax_log(LOG_ERR, "Can't write hash of snapshots!\n");
        exit(-1);
    }
}

/*
 * Checks the snapshot log hash
 * name: sst_hash
 * The log_lock must be locked before entering
 */
int sst_hash(struct node_struct * node) {
    int hash_ret = 0;

    if (exists(sst_fn) != 0) {
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        return 0;
    }
    hash_ret = hash_check(sst_fn);
    if (hash_ret != 0) {

        pax_log(LOG_ERR, "File %s corrupted!\n", sst_fn);
        node->non_voting = BTRUE;
        return 2;
    }
    return 0;
}

/*
 * Checks the basis log hash
 * name: out_hash
 */
int out_hash() {
    int hash_ret = 0;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    hash_ret = hash_check(zero_fn);
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    if (hash_ret != 0) {

        pax_log(LOG_ERR, "File %s corrupted!\n", zero_fn);
        node.non_voting = BTRUE;
        return 2;
    }
    return 0;
}

/*
 * Writes the md5 hash of the file
 * name: write_hash
 */
int write_hash(char * filename) {
    char comd[512], hash_file_name[512];
    const char * md5 = "md5sum ";
    const char * ext = ".hash";
    const char * post = ">";
    int ret = 0;

    snprintf(hash_file_name, 511, "%s%s", filename, ext);
    snprintf(comd, 511, "%s%s%s%s", md5, filename, post, hash_file_name);
    ret = system(comd);
    if (ret != 0) {
        goto err;
    }

err:
    if (ret != 0) pax_log(LOG_ERR,"Hash write: %s\n", strerror(errno));

    return ret;
}

/*
 * Checks the md5 hash of the file
 * The log must be locked before entering the function
 * 
 */
int hash_check(char * filename) {
    char *comd = NULL, *hash_file_name = NULL;
    const char * md5 = "md5sum -c --status ";
    const char * ext = ".hash";
    int ret = 0, size = 0;
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Hash control started %s\n", filename);

    size = (strlen(filename) + strlen(ext) + 1) * sizeof (char);
    hash_file_name = (char *) malloc(size);
    if (!hash_file_name) {
        pax_log(LOG_ERR,"Opening log hash file: %s\n", strerror(errno));
        goto err;
    }


    memset(hash_file_name, 0, size);
    sprintf(hash_file_name, "%s%s", filename, ext);

    size = (strlen(md5) + strlen(hash_file_name) + 1) * sizeof (char);
    comd = (char *) malloc(size);

    if (comd==NULL) {
        ret = -1;
        goto err;
    }
    memset(comd, 0, size);
    sprintf(comd, "%s%s", md5, hash_file_name);

    ret = system(comd);
    if (ret != 0) {
        goto err;
    }

err:
    if (hash_file_name != NULL) free(hash_file_name);
    if (comd != NULL) free(comd);
    if (ret != 0) pax_log(LOG_ERR, "Hash checked failed.\n");

    return ret;
}

/*
 * Process all the logs
 * name: process_logs
 */
int process_logs(struct node_struct * node) {
    int ret = 0;
    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if ((pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex");
        exit(EX_OSERR);
    }
    instance_proposed_sug = -1;
    round_sug = -1;
    ret = process_snapshot();
    if (ret!=0) goto done;
    ret = process_log(node, zero_fn, BFALSE);
    if (ret!=0) goto done;
    ret = process_log(node, filename, BFALSE);
    if (ret!=0) goto done;
	/*Clear all records which are outdated*/
    if(_PAXOS_DEBUG>2)print_db_state();
    memory_timeout();
done:
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    if ((pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return ret;
}

/*
 * Process log
 * name: process_log
 * @param nd pointer to node structure
 * @param name Log file name
 * @param snapshotting Is log processed or snapshot is beiing taken?
 * 
 * The node->mtmutex and log_mutex must be locked before entering
 */
int process_log(struct node_struct * nd, char * name, PAXOS_BOOL snapshotting) {
    char buf[BUF_SIZE];
    int hash_ret;
    FILE * inf, * outf = NULL;
    char * cmd, * cookie, * metric;
    long long round, instance_proposed, ballot;
    long long instance_proposed_sug = -1, round_sug = -1;
    int i = 0;

    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Processing %s\n", name);

    hash_ret = hash_check(name);
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Hash check failed!\n", name);
        nd->non_voting=BTRUE;
        return 2;
    }

    if ((inf = fopen(name, "r")) == NULL) {
        pax_log(LOG_ERR,"cannot open log file: %s\n", strerror(errno));
        exit(-1);
    }
    if (outf == NULL && snapshotting) {
        if ((outf = fopen(PAXOS_DIR TEMP_LOG, "a+")) == NULL) {
            pax_log(LOG_ERR,"cannot open zero log file: %s\n", strerror(errno));
            exit(-1);
        }
    }
    while (fgets(buf, LINE_MAX, inf)) {
        if (n_rounds > SNAPSHOT_LIMIT/2 || snapshotting == BFALSE) {
            if(_PAXOS_DEBUG>2)pax_log(LOG_DEBUG, "Reducing %s.\n", buf);
            if ((cmd = strtok_r(buf, ";", &cookie)) == NULL) {
                pax_log(LOG_ERR, "Process log: Command not found\n");
                continue;
            }
            /*Master records*/
            if (strncmp(cmd, "MAS", 3) == 0) {                
                if ((cmd = strtok_r(buf, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Process log: Command not found\n");
                    continue;
                }
                if (strncmp(cookie, "BEB", 3) == 0) {
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }

                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    ballot = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    instance_proposed = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    round = atoll(cmd);
                    metric = strdup(cookie);

                    if (!snapshotting) {
                        paxos.last_instance = instance_proposed;
                        paxos.previous_round = round;
                        n_rounds++;
                        if(_PAXOS_DEBUG)pax_log(LOG_DEBUG,"Number of records in the log: INCREASE n_rounds %d\n", n_rounds);
                        if (strncmp(metric, "NA", 2) != 0) {/*If not voting round*/
                            if (strncmp(metric, "REM:", 4) == 0) { /*If metric remove request*/
                                if (memory_remove_local(metric) != 0) {
                                    pax_log(LOG_ERR, "Local value cannot be removed.\n");
                                }
                            } else if (memory_update_local(metric) != 0) {
                                pax_log(LOG_ERR, "Local metric cannot be updated (%s).\n", metric);
                            }
                        }
                        if(_PAXOS_DEBUG>2)print_db_state();
                    } else {
                        if (n_rounds != 0) {
                            snt -> round = round;
                            snt -> instance = instance_proposed;
                            n_rounds--;
                            if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "Number of records in the log: nrounds decreasing %d\n", n_rounds);
                        }
                    }
                }
            }
            /*Client records*/
            if (strncmp(cmd, "CLI", 3) == 0) {
                if ((cmd = strtok_r(buf, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Process log: Command not found\n");
                    continue;
                }
               if (strncmp(cookie, "BEB", 3) == 0) {
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }

                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    ballot = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    instance_proposed_sug = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: BEB: Command not found:\n");
                        continue;
                    }
                    round_sug = atoll(cmd);
                    metric = strdup(cookie);
               }

               if (strncmp(cookie, "SUC", 3) == 0) {
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: SUC: Command not found:\n");
                        continue;
                    }

                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: SUC: Command not found:\n");
                        continue;
                    }
                    round = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Process log: SUC: Command not found:\n");
                        continue;
                    }
                    instance_proposed = atoll(cmd);
                    if (instance_proposed_sug == instance_proposed && round_sug == round) {
                        if (!snapshotting) {
                            paxos.last_instance = instance_proposed;
                            paxos.previous_round = round;
                            n_rounds++;
                            if(_PAXOS_DEBUG)pax_log(LOG_DEBUG,"INCREASE n_rounds %d\n", n_rounds);
                            if (strncmp(metric, "NA", 2) != 0) {/*If not voting round*/
                                if (strncmp(metric, "REM:", 4) == 0) { /*If metric remove request*/
                                    if (memory_remove_local(metric) != 0) {
                                        pax_log(LOG_ERR, "Local value cannot be removed.\n");
                                    }
                                } else if (memory_update_local(metric) != 0) {
                                    pax_log(LOG_ERR, "Local metric cannot be updated (%s).\n", metric);
                                }
                                if(_PAXOS_DEBUG>2)print_db_state();
                            }
                        } else {
                            if (n_rounds != 0) {
                                snt -> round = round;
                                snt -> instance = instance_proposed;
                                n_rounds--;
                                if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "Number of records in the log: nrounds decreasing %d\n", n_rounds);
                            }
                        }
                    }else{
                        if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "Ignoring orphaned SUC\n");
                        return 5;
                    }
                }
            }
        } else if (snapshotting) {
            if(_PAXOS_DEBUG>2)pax_log(LOG_DEBUG, "Copiing into the new log %s.\n", buf);
            fprintf(outf, "%s", buf);
        }
    }


    fclose(inf);
    if (outf != NULL) fclose(outf);
    return 0;
}

/*
 * Sends the log to the remote outdated node
 * name: update_remote_node
 * @param id Id of the node according to the node struct
 */
int update_remote_node(int id, struct node_struct node) {
    pthread_t upd_thr;
    int ret = 0, hash_ret;

    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit (EX_OSERR);
    }
    ret = snaps_hash(&node);
    ret += sst_hash(&node);
    if (ret > 0) {
        node.non_voting = BTRUE;
        return 1;
    }
    hash_ret = hash_check(filename);
    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Hash check failed!\n", filename);
        node.non_voting = BTRUE;
        return 1;
    }    
    hash_ret = hash_check(zero_fn);
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit (EX_OSERR);
    }

    if (hash_ret != 0) {
        pax_log(LOG_ERR, "File %s corrupted: Hash check failed!\n", filename);
        node.non_voting = BTRUE;
        return 1;
    }
    if ((ret = pthread_create(&upd_thr, NULL, updater_thread, (void*) & id)) != 0) {

        pax_log(LOG_ERR,"can't create updater thread: %s\n", strerror(errno));
        exit(EX_OSERR);
    }
    sleep(2);
    
    return 0;
}

/*
 * Sends the log to the outdated node
 * name: updater_thread
 */
static void * updater_thread(void *args) {
    int fd;
    int oflag;
    oflag = O_RDONLY;
    char buf[MTR_BUF], * sn_fn;
    char msg[MTR_BUF + 4];
    int id, size;
    FILE * inf;
    metric_rec * np;


    id = *((int *) args);
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Updater started for %d\n", id);
    fd = node.machtab[id].fd;
    send_mes("OUT:BEG:;", fd);

    if (pthread_mutex_lock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (exists(sst_fn) == 0) {
        if ((inf = fopen(sst_fn, "r")) == NULL) {
            pax_log(LOG_ERR,"cannot open snapshot base file: %s\n", strerror(errno));
            free(sn_fn);
            exit(-1);
        }
        while (fgets(buf, LINE_MAX, inf)) {
            sprintf(msg, "%s%s;", "OUT:SSM:", buf);
            send_mes(msg, fd);
        }
        fclose(inf);

        TAILQ_FOREACH(np, &lst_m, metricq) {
            size = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 2;
            sn_fn = (char *) malloc(size * sizeof (char));
            snprintf(sn_fn, size, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);
            if ((inf = fopen(sn_fn, "r")) == NULL) {
                pax_log(LOG_ERR,"cannot open snapshot file: %s\n", strerror(errno));
                free(sn_fn);
                goto next;
            }
            while (fgets(buf, LINE_MAX, inf)) {
                sprintf(msg, "%s%s:%s;", "OUT:SST:", np->name, buf);
                send_mes(msg, fd);
            }
            fclose(inf);
next:       free(sn_fn);
        }
    }
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    /*Outdated log*/
    if ((inf = fopen(zero_fn, "r")) == NULL) {
        pax_log(LOG_ERR,"cannot open zero log file: %s\n", strerror(errno));
        exit(-1);
    }
    while (1) {

        if (pthread_mutex_lock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }

        if (!fgets(buf, LINE_MAX, inf)) {
            fclose(inf);
            if (pthread_mutex_unlock(&log_mutex) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            break;
        }
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        sprintf(msg, "%s%s", "OUT:", buf);
        send_mes(msg, fd);
    }

    /*Main log*/
    if ((inf = fopen(filename, "r")) == NULL) {
        pax_log(LOG_ERR,"cannot open log file: %s\n", strerror(errno));
        exit(-1);
    }

    while (1) {
        if (pthread_mutex_lock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        if (!fgets(buf, LINE_MAX, inf)) {
            fclose(inf);
            if (pthread_mutex_unlock(&log_mutex) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            break;
        }
        if (pthread_mutex_unlock(&log_mutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        sprintf(msg, "%s%s", "OUT:", buf);
        send_mes(msg, fd);
    }
    send_mes("OUT:FIN:;", fd);
    return (void *) 0;
}

int snapshot() {
    metric_rec * np;
    FILE * inf;
    char * sn_fn;
    int size, ret;

    if (pthread_mutex_lock(&log_mutex) != 0) {
    	pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }    

    TAILQ_FOREACH(np, &lst_m, metricq) {
        size = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 2;
        sn_fn = (char *) malloc(size * sizeof (char));
        snprintf(sn_fn, size, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);
        if (_PAXOS_DEBUG>1)pax_log(LOG_DEBUG,"Making snapshot of %s to %s\n", np->name, sn_fn);
        if ((inf = fopen(sn_fn, "w")) == NULL) {
            pax_log(LOG_ERR,"cannot open snapshot file: %s\n", strerror(errno));
            free(sn_fn);
            exit(-1);
        }
        memory_dump(np->name, inf, 1);
        fclose(inf);
        free(sn_fn);
    }
    instance_proposed_sug = -1;
    round_sug = -1;

    if(exists(zero_fn)==0) {
        if(process_log(&node, zero_fn, BTRUE)!=0) {
            node.updating = BFALSE;
            node.non_voting = BTRUE;
        }            
        close(zfile);
    }
    if(process_log(&node, filename, BTRUE)!=0) {
        node.updating = BFALSE;
        node.non_voting = BTRUE;
    }            
    close(file);
    ret = rename(PAXOS_DIR TEMP_LOG, PAXOS_LOG_FILE);
    if (ret != 0) {
        pax_log(LOG_ERR,"rename: %s\n", strerror(errno));
        exit(-1);
    }

    if (!(file = open(filename, O_RDWR | O_APPEND, S_IWUSR | S_IRUSR))) {
        pax_log(LOG_ERR,"Opening log file: %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    unlink(PAXOS_LOG_FILE BASE_LOG_EXT);
    if (!(zfile = open(zero_fn, O_RDWR | O_APPEND|O_CREAT, S_IWUSR | S_IRUSR))) {
        pax_log(LOG_ERR,"Opening zero log file: %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    write_hash(PAXOS_LOG_FILE);

    write_hash(PAXOS_LOG_FILE BASE_LOG_EXT);
    if ((inf = fopen(sst_fn, "w")) == NULL) {
        pax_log(LOG_ERR,"cannot open snapshot base log file: %s\n", strerror(errno));
        exit(-1);
    }

    if(snt->instance==-1) snt->instance = paxos.last_instance;
    if(snt->round==-1) snt->round = paxos.previous_round;
    fprintf(inf, "%lld:%lld;", snt->instance, snt->round);
    fflush(inf);
    fclose(inf);
    ret += write_hash(sst_fn);
    if (ret != 0) {
        pax_log(LOG_ERR, "cannot write hash of %s\n", sst_fn);
        exit(EX_OSERR);
    }
    snaps_write_hash();
    if (pthread_mutex_unlock(&log_mutex) != 0) {
    	pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    if(_PAXOS_DEBUG>2)pax_log(LOG_DEBUG,"Snapshot done\n");
    return 0;
}

/*
 * Takes the log snapshot
 * name: snapshot_thr
 */
static void * snapshot_thr(void * args) {
    int * ret;
    for (;;) {
        sleep(SNAPSHOT_TO);
        if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "Sst starting\n");
        global_lock();
        if (n_rounds > SNAPSHOT_LIMIT && get_snap_lock() == BFALSE && node.non_voting==BFALSE) {
            snapshot();
            if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "NROUNDS %d\n", n_rounds);
        }
        snt->instance = -1;
        snt->round = -1;
        n_rounds = n_rounds - SNAPSHOT_LIMIT/2;
        global_unlock();
    }

    return (void *) ret;
}

/*
 * Process the snapshot
 * name: process_snapshot
 * @param snapshot Are we updating the snapshot or processing it?
 */
int process_snapshot() {
    FILE * inf;
    char buf[LINE_MAX], *cookie, *inst, *round, *sn_fn;
    metric_rec * np;
    int size, stat_ret, ret = 0;

    ret = sst_hash(&node);
    ret += snaps_hash(&node);

    if (ret > 0) return -1;

    TAILQ_FOREACH(np, &lst_m, metricq) {
        size = strlen(np->name) + strlen(PAXOS_DIR) + strlen(SNAPSHOT_EXT) + 2;
        sn_fn = (char *) malloc(size * sizeof (char));
        snprintf(sn_fn, size, "%s%s%s", PAXOS_DIR, np->name, SNAPSHOT_EXT);

        stat_ret = exists(sn_fn);
        if (stat_ret != 0) {
            free(sn_fn);
            return 0;
        }
        if ((inf = fopen(sn_fn, "r")) == NULL) {
            pax_log(LOG_ERR,"cannot open snapshot file: %s\n", strerror(errno));
            free(sn_fn);
            exit(-1);
        }
        memory_load(np->name, inf, 1);
        fclose(inf);
        free(sn_fn);
    }

    if ((inf = fopen(sst_fn, "r")) == NULL) {
        pax_log(LOG_ERR,"cannot open snapshot base log file: %s\n", strerror(errno));
        return(0); /*Hash processed which means that file is OK*/
    }

    while (fgets(buf, LINE_MAX, inf)) {
        if ((inst = strtok_r(buf, ":", &cookie)) == NULL) {
            pax_log(LOG_ERR, "Snapshot file corrupted\n");
            exit(EX_OSERR);
        }
        paxos.last_instance = atoll(inst);
        if ((round = strtok_r(cookie, ";", &cookie)) == NULL) {
            pax_log(LOG_ERR, "Snapshot file corrupted\n");
            exit(EX_OSERR);
        }
        paxos.previous_round = atoll(round);
     }
    fclose(inf);
    return 0;
}

/*
 * For the snapshot update purposes
 * Inserts new metric record into the metric Queue
 * name: insert_metric_rec
 */
int insert_metric_rec(char *rec) {
    metric_rec * record;
    record = malloc(sizeof (record));
    record ->name = strdup(rec);
    TAILQ_INSERT_TAIL(&lst_m, record, metricq);
    if (_PAXOS_DEBUG>2) {
        metric_rec * np;
        pax_log(LOG_DEBUG, "Distributed metric inserted:\n");
        TAILQ_FOREACH(np, &lst_m, metricq)
            pax_log(LOG_DEBUG,"%s\n", np->name);
    }

    return 0;
}

void clear_dumps(){
    char file[1024];
    metric_rec * np;
    TAILQ_FOREACH(np, &lst_m, metricq) {
    	strcpy(file,PBS_CACHE_ROOT "/pbs_cache.");
	strcat(file,"dump.");
	strcat(file,np->name);
	unlink(file);
    }
}

void close_log() {
    free(snt);
    close(file);
    close(zfile);
}
