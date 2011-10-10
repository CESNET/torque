/*
 *      api.c
 *      
 *      Copyright 2009 Jaroslav Tomeček
 *      
 * 
 * 		Basic API for Paxos interface
 */

#include "main.h"
#include "../api.h"
#include "../memory.h"
#include "../../net.h"

struct node_struct node;

static int initialized = 0;
pthread_mutex_t globmutex;

int master_status(struct node_struct * node);
void * dist_metric_int(void *args);

/*Locking the node to prevent concurrent replicated metric managing*/
void global_lock() {
    if ((pthread_mutex_lock(&globmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (_PAXOS_DEBUG>0)pax_log(LOG_DEBUG, "glob mutex locked\n");
}

void global_unlock() {
    if ((pthread_mutex_unlock(&globmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    if (_PAXOS_DEBUG>0)pax_log(LOG_DEBUG, "glob mutex unlocked\n");
}

/*
 * 
 * name: get_master_status
 * @param
 * @return Whether the local node is Paxos master or paxos slave
 */
int get_master_status() {
    return master_status(&node);
}

/*
 * Internal function returning the local node paxos status
 * name: master_status
 * @param node struct pointer
 * @return MASTER or SLAVE
 */
int master_status(struct node_struct * node) {
    int ret = CLIENT;
    if (pthread_mutex_lock(&node->mtmutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (node->master_id == node->id) {
        ret = MASTER;
    }
    if (pthread_mutex_unlock(&node->mtmutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return ret;
}

/*
 * Transforming the master IP (IPv4) to hostname
 * name: get_host
 * @param node struct pointer
 * @return hostent struct pointer 
 */
char * get_host(struct node_struct *node) {
    struct hostent *hp;
    unsigned long addr_num;

    if (pthread_mutex_lock(&node->mtmutex) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (node->master_id == -1) {
        if (pthread_mutex_unlock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }

        return NULL;
    }

    addr_num = inet_addr(node->machtab[id2index(node->master_id)].ip);
    hp = gethostbyaddr((char *) & addr_num,
            sizeof (addr_num), AF_INET);
    if (pthread_mutex_unlock(&node->mtmutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return strdup(hp->h_name);
}

/*
 * Asks the paxos master for the metric value
 * name: find_replicated
 * @param name metric name
 * @param hostname 
 * @param timestamp
 * @return value
 */
char * find_replicated(char *hostname, char *name, time_t *timestamp) {
    FILE *stream;
    char *value, * tstp, *resp, * cookie;
    char *host, *ret;

    if (_PAXOS_DEBUG) pax_log(LOG_DEBUG, "Find replicated\n");
    net_proto(NPROTO_AUTO);
    host = get_host(&node); /*Find the master*/
    if (host == NULL) return NULL;
    stream = cache_connect_net(host, PBS_CACHE_PORT);
    free(host);
    if (stream != NULL) {
        resp = cache_get(stream, hostname, name);
        if ((tstp = strtok_r(resp, "\t", &cookie)) == NULL) {
            pax_log(LOG_ERR, "Incomplete metric message\n");
            cache_close(stream);
            return NULL;
        }

        if ((value = strtok_r(cookie, "\n", &cookie)) == NULL) {
            pax_log(LOG_ERR, "Incomplete metric message\n");
            cache_close(stream);
            return NULL;
        }
        *timestamp = atol(tstp);
        cache_close(stream);
    } else {
        pax_log(LOG_ERR, "cache_connect failed\n");
        return NULL;
    }
    ret = strdup(value);
    free(resp);
    return ret;
}

/*
 * Manages paxos round for metric remove request
 * 
 * name: remove_replicated
 */
int remove_replicated(char *hostname, char *name) {
    int ret = 0;
    /*fd_set master;  master file descriptor list*/
    /*int number_of_descriptors, */
    int size, size_host, size_name;
    /*struct timeval tv;*/
    char * mes;
    char buf[MTR_BUF];
    const char * pref = "REM";

    pax_log(LOG_ERR, "Removing metrics not tested yet\n");
    size_host = strlen(hostname);
    size_name = strlen(name);
    size = (size_host + 1 + size_name + strlen(pref)) * sizeof (char);
    mes = (char *) malloc(size);
    memset(mes, 0, size);
    memset(buf, 0, 1024 * 5);

    snprintf(mes, size, "%s:%s:%s", pref, hostname, name);
    if (initiate_consensus(&node, mes, BFALSE) == 0) {
        ret = 0;
    }else {
        ret = -1;
    }
    free(mes);

    return ret;
}

/*
 * Manages the API to paxos round for updating the value of the metric
 * name: update_replicated
 */
int update_replicated(char *hostname, char *name, char *value, time_t timestamp) {
    int ret = 0;
    /*fd_set master;  master file descriptor list*/
    /*int number_of_descriptors;*/
    int size;
    /*struct timeval tv;*/
    char * mes;
    char buf[MTR_BUF];

    if (_PAXOS_DEBUG > 1) pax_log(LOG_DEBUG, "Updating %s, %s, %s\n", hostname, name, value);
    if (hostname == NULL || name == NULL || value == NULL) {
        if (_PAXOS_DEBUG) pax_log(LOG_DEBUG, "One or more of the values to be commited are null\n");
        return -1;
    }
    size = (strlen(hostname) + 1 + strlen(name) + strlen(value) +
            11 + 3) * sizeof (char);
    mes = (char *) malloc(size);
    memset(mes, 0, size);
    memset(buf, 0, MTR_BUF);
    snprintf(mes, size, "%s:%s:%s:%ld", hostname, name, value, timestamp);
    if (initiate_consensus(&node, mes, BFALSE) == 0) {
        ret = 0;
    }else {
        ret = -1;
    }
    free(mes);
    return ret;
}

/*
 * Asks the paxos master for removing the metric
 * name: remove_replicated_request
 */
int remove_replicated_request(char *hostname, char *name) {
    int ret = 0;
    FILE *stream;
    char * host;

    net_proto(NPROTO_AUTO);
    host = get_host(&node);
    if (host == NULL) return -1;
    stream = cache_connect_net(host, PBS_CACHE_PORT);
    free(host);
    if (stream != NULL) {
        ret = cache_remove(stream, hostname, name);
        cache_close(stream);
    } else {
        pax_log(LOG_ERR, "cache_connect failed\n");
        return -1;
    }
    return ret;
}

/*
 * Asks the paxos master for change the value of the metric
 * name: update_replicated_request
 */
int update_replicated_request(char *hostname, char *name, char *value) {
    int ret = 0;
    FILE *stream;
    char * host;

    net_proto(NPROTO_AUTO);
    if (_PAXOS_DEBUG > 1) pax_log(LOG_DEBUG, "Proxiing to master %s, %s, %s\n", hostname, name, value);
    host = get_host(&node);
    if (host == NULL) return -1;
    stream = cache_connect_net(host, PBS_CACHE_PORT);
    free(host);
    if (stream != NULL) {
        ret = cache_store(stream, hostname, name, value);
        cache_close(stream);
    } else {
        pax_log(LOG_ERR, "cache_connect failed\n");
        return -1;
    }
    if (_PAXOS_DEBUG > 2) pax_log(LOG_DEBUG, "Proxy done %s, %s, %s\n", hostname, name, value);
    return ret;
}

int preinit_rep() {
    int mutex_ret;

    mutex_ret = pthread_mutex_init(&globmutex, NULL);
    if (mutex_ret != 0) {
        pax_log(LOG_ERR, "pthread_mutex_init: glob_mutex %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    atexit(close_db);
    atexit(close_log);
    atexit(pax_log_close);
    atexit(close_net);
    node_init(IP_CONF, &node);
    paxos_init();
    proxy_stop();
    return 0;
}

int init_rep() {
    pthread_t cnt_thr, lt_thr, i_thr, elect_thr;
    int nsites, ret;
    struct node_struct *nd;

    nd = &node;
    if (!initialized) {
        initialized = 1;
        init_db(&node);
        paxos_state_print();
        if (process_logs(&node) != 0) {
            if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            nd->updating = BFALSE;
            nd->non_voting = BTRUE; /*The node can vote again*/

            if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }

        }
        nsites = node.number_of_nodes;
        clear_dumps();

        if ((ret = pthread_create(&lt_thr, NULL, listen_thread, NULL)) != 0) {
            pax_log(LOG_ERR, "can't create connect thread: %s\n", strerror(errno));
            goto err;
        }


        if ((ret = pthread_create(&cnt_thr, NULL, contact_group, NULL)) != 0) {
            pax_log(LOG_ERR, "can't create connect-all thread: %s\n", strerror(errno));
            goto err;
        }

        if ((ret = pthread_create(&i_thr, NULL, dist_metric_int, NULL)) != 0) {
            pax_log(LOG_ERR, "can't create dist_metric_int thread: %s\n", strerror(errno));
            goto err;
        }
        sleep(1); /*Wait for others to be started*/
        if ((ret = pthread_create(&elect_thr, NULL, election_thread, NULL)) != 0) {
            pax_log(LOG_ERR, "can't create election-thread: %s\n", strerror(errno));
            goto err;
        }
        return 0;
err:
        return ret;
    }
    return 0;
}

/*
 * Request for paxos round listener.
 * name: dist_metric_int
 */
void * dist_metric_int(void *args) {
    fd_set master; /* master file descriptor list*/
    int number_of_descriptors;
    char buf[MTR_BUF];
    for (;;) {
        memset(buf, 0, MTR_BUF);
        FD_ZERO(&master);
        FD_SET(metric_pipe[0], &master);
        number_of_descriptors = select(FD_SETSIZE, &master, NULL, NULL, NULL);

        if (FD_ISSET(metric_pipe[0], &master)) {
            const char * nack = "NACK";
            read(metric_pipe[0], &buf, MTR_BUF);
            if (initiate_consensus(&node, buf, BFALSE) == 0) {
                const char * mes = "ACK";
                if (!write(metric_pipe_resp[1], mes, strlen(mes) * sizeof (char))) {
                    pax_log(LOG_ERR, "Cannot write to the pipe: %s\n", strerror(errno));
                    exit(EX_OSERR);
                }
            } else
                if (!write(metric_pipe_resp[1], nack, strlen(nack) * sizeof (char))) {
                pax_log(LOG_ERR, "Cannot write to the pipe: %s\n", strerror(errno));
                exit(EX_OSERR);
            }
        }
    }
}

/*
 * Manages the local change in the case of the master request
 * name: memory_update_local
 * @param msg message received
 */
int memory_update_local(char * msg) {
    char * hostname, *name, *value, *cookie;
    int ret;
    if ((hostname = strtok_r(msg, ":", &cookie)) == NULL) {
        pax_log(LOG_ERR, "memory update local: hostname: Command not found:\n");
        return -1;
    }
    if ((name = strtok_r(cookie, ":", &cookie)) == NULL) {
        pax_log(LOG_ERR, "memory update local: name: Command not found:\n");
        return -1;
    }
    if ((value = strtok_r(cookie, ":", &cookie)) == NULL) {
        pax_log(LOG_ERR, "memory update local: value: Command not found:\n");
        return -1;
    }

    if (_PAXOS_DEBUG > 2) pax_log(LOG_DEBUG, "Saving: hostname %s name %s value %s timestamp %s\n", hostname, name, value, cookie);
    ret = memory_update(hostname, name, value, atol(cookie), BTRUE);
    if (_PAXOS_DEBUG > 1) pax_log(LOG_DEBUG, "Saved: hostname %s name %s value %s timestamp %s [%d]\n", hostname, name, value, cookie, ret);
    return ret;
}

/*
 * Manages the local change in the case of the master request
 * name: memory_remove_local
 * @param msg message received
 */
int memory_remove_local(char * msg) {
    char * hostname, *cookie;
    int ret;

    if ((msg = strtok_r(msg, ":", &cookie)) == NULL) {
        pax_log(LOG_ERR, "memory remove local: Wrong message format:\n");
        return -1;
    }

    if ((hostname = strtok_r(msg, ":", &cookie)) == NULL) {
        pax_log(LOG_ERR, "memory remove local hostname: Command not found:\n");
        return -1;
    }
    ret = memory_remove(hostname, cookie, BTRUE);
    return ret;
}

/*
 * Inserts metric name into the list
 *
 *
 */
int paxos_insert_metric(char *name) {
    return insert_metric_rec(name);
}

