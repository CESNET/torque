
#include "main.h"

struct node_struct node;

int main(int argc, char *argv[]) {

    pthread_t cnt_thr, lt_thr;
    void *aret, *cret;
    int nsites, ret;
    char buf;

    //if (init_log(PAXOS_LOG_FILE) < 0) exit(-1);
    node_init(IP_CONF, &node);
    //machtab_print();
    //(void) paxos_init();
    //init_db(&node);
    //paxos_state_print();
    //process_logs();

    //if (write_log_record("Ahoj") != 0) exit(-2);
    if (_PAXOS_DEBUG) printf("Number of nodes: %d\n", node.number_of_nodes + 1);
    nsites = node.number_of_nodes;


    if ((ret = pthread_create(&lt_thr, NULL, listen_thread, NULL)) != 0) {
        perror("can't create connect thread");
        goto err;
    }

    if ((ret = pthread_create(&cnt_thr, NULL, contact_group, NULL)) != 0) {
        perror("can't create connect-all thread");
        goto err;
    }
    election(& node);
    for (;;) {
        scanf("%c", &buf);
        //if (strcmp(buf, "quit") == 0) break;
        election(&node);
        printf("%c", buf);
    }
    printf("quit\n");
    /* Wait on the connection threads. */
    if (pthread_join(cnt_thr, &aret) || pthread_join(lt_thr, &cret)) {
        ret = -1;
        goto err;
    }
    if ((uintptr_t) aret != EXIT_SUCCESS ||
            (uintptr_t) cret != EXIT_SUCCESS) {
        ret = -1;
        goto err;
    }

err:

    return (ret);
}
/**TODO
 * at_exit
 *
 */
