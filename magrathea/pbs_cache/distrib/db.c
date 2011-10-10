/*
 *      db.c
 *      
 *      Copyright 2009 Jaroslav Tomeček
 *      
 * 
 * 		Paxos transaction API
 */

#include "main.h"

typedef struct transaction_struct {
    long long round, instance;
    char metric[MTR_BUF];
} txn_t;

txn_t * txns;

pthread_mutex_t dbmutex;

extern struct node_struct node;

/*
 * db api initialization
 * name: init_db
 * @param
 * @return
 */
void init_db(struct node_struct * node) {
    int i, mutex_ret;
    if ((pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    txns = (txn_t *) malloc((node->number_of_nodes + 1) * sizeof (txn_t));
    for (i = 0; i < node->number_of_nodes + 1; i++) {
        txns[0].instance = -2;
        txns[0].round = -1;
    }
    if ((pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    mutex_ret = pthread_mutex_init(&dbmutex, NULL);
    if (mutex_ret != 0) {
        pax_log(LOG_ERR, "init_db:pthread_mutex: %s\n", strerror(errno));
        exit(EX_OSERR);
    }
    
}

/*
 * Starts the transaction. One transaction for each node is held.
 * name: txn_begin
 * @param id id of the node which sent the message
 * @param inst paxos instance number 
 * @param round paxos round number 
 * @param value value to be processed
  */
int txn_begin(int id, long long inst, long long round, char * metric) {
    if(id < 0 || id > node.number_of_nodes) {
        pax_log(LOG_ERR, "txn_begin:index out of bound.\n");
        return -1;
    }
    if ((pthread_mutex_lock(&dbmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    txns[id].instance = inst;
    txns[id].round = round;
    memset(txns[id].metric, 0, MTR_BUF);
    sscanf(metric, "%s", txns[id].metric);
    if ((pthread_mutex_unlock(&dbmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return 0;
}

/*
 * Commits the transaction. One transaction for each node is held.
 * name: txn_begin
 * @param id id of the node which sent the message
 * @param inst paxos instance number 
 * @param round paxos round number 
 */
char * txn_commit(int id, long long inst, long long round) {
     if(id < 0 || id > node.number_of_nodes) {
        pax_log(LOG_ERR, "txn_commit: index out of bound.\n");
        return NULL;
    }
    if ((pthread_mutex_lock(&dbmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (txns[id].instance == inst && txns[id].round == round) {
        if ((pthread_mutex_unlock(&dbmutex)) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        return txns[id].metric;

    }
    if ((pthread_mutex_unlock(&dbmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    return NULL;
}

void close_db() {
    free(txns);
}
