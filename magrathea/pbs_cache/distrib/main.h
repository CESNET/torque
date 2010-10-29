#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <limits.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <syslog.h>
#include <sys/queue.h>
#include "queue.h"
#include "../api.h"
#include "../log.h"

#ifndef _PAXOS_H_
#define	_PAXOS_H_

#define PORT 9035   /* port we're listening on*/
#define	MAX_THREADS	25
#define	SLEEPTIME	3
#define	SOCKET_CREATION_FAILURE -1
#define RESPONSE_TIMEOUT_USEC 500000 /*timeout for the response*/
#define RESPONSE_TIMEOUT_SEC 3 /*timeout for the response*/
#define PING_TIMEOUT_USEC 500000 /*ping timeout*/
#define PING_TIMEOUT_SEC 30 /*ping timeout*/
#define SNAPSHOT_TO 20 /*snapshotting timeout - 120*/
#define SNAPSHOT_LIMIT 20 /*Number of records needed to snapshot - 100*/
#define BTRUE 1
#define BFALSE 0
#define MASTER 1
#define CLIENT 0
#define PAXOS_DIR PBS_CACHE_ROOT "/"
#define PAXOS_LOG_FILE PAXOS_DIR "paxos.log"
#define BASE_LOG_EXT ".zero"
#define SNAPSHOT_EXT ".snap"
#define TEMP_LOG "pax.temp"
#define CON_FAILED -1
#define NRETR 3
#define IP_CONF "./distrib/ip.conf"
#define _PAXOS_DEBUG 9
#define BUF_SIZE 2048  /* size of message buffers */
#define QUORUM(node) ((node)->number_of_nodes+1)/2 /*Number of nodes I need to commit the request. The last node is this node*/
#define MTR_BUF 1024*5 /*metric buffer size*/
#define PAXOS_PBS_LOG -1 /*If greater than 0 and lower than 8 then the paxos reporting log is printed to STDOUT
                           if greater than 8 then  the paxos reporting log is printed to PAXOS_LOG*/
#define PAXOS_LOG PAXOS_DIR "paxos-report.log"

typedef int PAXOS_BOOL;

typedef struct pxs_struct paxos_t;
TAILQ_HEAD(lsthead, voted_struct) lsthead;

struct __machine {
    int id; /*Remote Id*/
    int port;
    char * ip;
    int fd;
    pthread_t hm_thr;
    PAXOS_BOOL connected;
};

typedef struct __machine machine_t;

struct node_struct {
    unsigned int id; /*Local id*/
    unsigned int number_of_nodes;
    char * hostname;
    char ** ip;
    int master_id;
    machine_t *machtab;
    pthread_mutex_t mtmutex;
    int current;
    PAXOS_BOOL non_voting;
    PAXOS_BOOL updating;
    int updater_id;
    char paxos_state;
};


struct pxs_struct {
    long long lastTried, /*ballot*/
            last_instance,
            previous_round,
            nextBallot, /*Promised ballot*/
            previous_vote; /*previous vote*/
};

paxos_t paxos;

typedef struct {
    char *host; /* Host name. */
    unsigned int port; /* Port on which to connect to this site. */
} repsite_t;


int ** wimpipe, **paxpipe, metric_pipe[2], metric_pipe_resp[2]; /* Pipes */

/*Communication functions*/
void proxy_start();
void proxy_stop();
char get_proxy();
void * listen_thread(void *args);
void * contact_group(void *args);
int get_connected_socket(char *remotehost, int port, int *is_open, int *eidp);
int get_next_message(int fd, char * buffer, PAXOS_BOOL to);
int listen_socket_init(int port);
int listen_socket_accept(int s, int *eidp, int * id);
int broadcast_send(char * msg);
int send_mes(char * msg, int fd);
void close_net();

/*Local node fnts*/
void node_init(char * ip_file, struct node_struct * node);
int machtab_rem(struct node_struct *node, int eid, int lock);
int machtab_add(struct node_struct *node, int fd, char * ip,
                 int *idp, PAXOS_BOOL real_change);
void machtab_print();
int ip2index(char* ip);
int id2index(int id);

/*Paxos initialization fnts*/
void paxos_state_print();
void paxos_init();
int election(struct node_struct * node);
int initiate_consensus(struct node_struct *node, char * metric, PAXOS_BOOL voting);
void * election_thread();

/*paxos log fns*/
void print_db_state();
int init_paxos_log();
void close_log();
int write_metric_snap (char * mtr_name, char * value);
int write_snapshot_mark(char * rec);
int write_log_out_record(char * msg);
int write_log_record(char * msg, PAXOS_BOOL master, PAXOS_BOOL commit);
int write_log_out_hash();
int process_logs(struct node_struct * node);
int process_log(struct node_struct * nd, char * name, PAXOS_BOOL snapshotting);
void clear_logs();
int out_hash();
int update_remote_node(int id, struct node_struct node);
int insert_metric_rec(char *rec);
void lock_snapshotting();
void unlock_snapshotting();
void clear_dumps();

/*Database functions*/
void init_db(struct node_struct * node);
int txn_begin(int id, long long inst, long long round, char * metric);
char * txn_commit(int id, long long inst, long long round);
void close_db();

/*API functions*/
int preinit_rep();
int init_rep();
int get_master_status();
int update_replicated(char *hostname, char *name, char *value, time_t timestamp);
int update_replicated_request(char *hostname, char *name, char *value);
char * find_host_replicated(char * metric_name, char *hostname, time_t *timestamp);
char * find_replicated(char *hostname, char *name, time_t *timestamp);
int remove_replicated(char *hostname, char *name);
int remove_replicated_request(char *hostname, char *name);
int memory_update_local(char * msg);
int memory_remove_local(char * msg);
int paxos_insert_metric(char *name);
void global_unlock();
void global_lock();

/*reporting*/
FILE * get_report_desc();
int pax_report_init();
void pax_log(int level, char *fmt, ...);
void pax_log_close();
#endif /* _PAXOS_H_ */
