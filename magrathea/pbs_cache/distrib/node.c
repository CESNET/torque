/*
 *      node.c
 *      
 *      Copyright 2009 Jaroslav Tomeček <adray@phoenix>
 *      
 * 		Node configuration
 */
#include "main.h"

extern struct node_struct node;

int ip2index(char * ip);
void machtab_print();

/*
 * Initiates node struct as well as the paxos node.
 */
void node_init(char * ip_file, struct node_struct * node) {
    FILE *inf; /*IP adress file desc.*/
    char line[LINE_MAX]; /*line of IP adress file*/
    int i = 0, id = 0;
    char *single_ip/*one IP from file*/;
    int s = socket(PF_INET, SOCK_STREAM, 0); /*socket for own IP recogn.*/
    const char *ip; /*My IP*/
    char *cookie;
    struct hostent *remote_host; /* Name of the remote node*/
    struct in_addr ipv4addr;
    int mutex_ret, pipe_ret;


    mutex_ret = pthread_mutex_init(&node->mtmutex, NULL);
    if (mutex_ret != 0) {
        pax_log(LOG_ERR,"node_init:pthread_mutex:%s\n", strerror(errno));
        exit(EX_OSERR);
    }

    /*implicit adress*/
    ip = "127.0.0.1";
    /*Own IP recognition*/
    for (i = 1;; i++) {
        struct ifreq ifr;
        struct sockaddr_in *sin = (struct sockaddr_in *) & ifr.ifr_addr;

        ifr.ifr_ifindex = i;
        if (ioctl(s, SIOCGIFNAME, &ifr) < 0)
            break;

        /* now ifr.ifr_name is set */
        if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
            continue;

        ip = inet_ntoa(sin->sin_addr);
        if (strcmp(ip, "127.0.0.1")) {
            break;
        }
    }
    close(s);

    if (_PAXOS_DEBUG>2)pax_log(LOG_DEBUG,"My IP is %s\n", ip);

    inet_pton(AF_INET, ip, &ipv4addr);
    remote_host = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
    node->hostname = strdup(ip);

    /*IP file loading*/
    if ((inf = fopen(ip_file, "r")) == NULL) {
        pax_log(LOG_ERR,"cannot open ip configuration file: %s\n", strerror(errno));
        exit(-1);
    }
    i = 0;

    while (fgets(line, LINE_MAX, inf)) {
        id++;
        if (strcmp(line, "\n") == 0) continue;
        if ((single_ip = strtok_r(line, ":", &cookie)) == NULL) {
            pax_log(LOG_ERR, "Format error while processing ip configuration file\n");
            exit(-1);
        }
        if (strlen(single_ip) > 15) {
            pax_log(LOG_ERR,
                    "Format error while processing ip configuration file. String is not IP address: %s\n",
                    single_ip);
            exit(-1);
        }
        if (strcmp(ip, single_ip) == 0) {
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"My IP detected [%d] %s\n", i, single_ip);
            node->id = id;
            continue;
        } else {
            i++;
            if (node->machtab == NULL) {
                node->machtab = (machine_t *) malloc(sizeof (machine_t));
                if (node->machtab == NULL) {
                    pax_log(LOG_ERR,"node_init: malloc:%s\n", strerror(errno));
                    exit(EX_OSERR);
                }
            } else {
                if (!(node->machtab = (machine_t *) realloc(node->machtab, i * sizeof (machine_t)))) {
                    pax_log(LOG_ERR,"node_init: malloc:%s\n", strerror(errno));
                    exit(EX_OSERR);
                }
            }
            if (!(node->machtab[i - 1].ip = (char *) malloc(16 * sizeof (char)))) {
                pax_log(LOG_ERR,"Not enough memory for IP table allocation!%s\n", strerror(errno));
                exit(EX_OSERR);
            }

            node->machtab[i - 1].id = id;
            node->machtab[i - 1].connected = BFALSE;
            node->machtab[i - 1].port = PORT;
            strcpy(node->machtab[i - 1].ip, single_ip);

            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"IP configuration: \n");
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"[%d] %d %s\n", i - 1, id, single_ip);
        }
    }

    fclose(inf);
    node->number_of_nodes = i;
    node->master_id = -1;
    node->current = 0;
    node->non_voting = BFALSE;
    node->updating = BFALSE;
    node->updater_id = -1;

    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"my id: %d\n", node->id);

    /*Pipes init*/
    if (!(wimpipe = (int **) malloc((node->number_of_nodes + 1) * sizeof (int *)))) {
        pax_log(LOG_ERR,"Pipe pool memory allocation: %s\n", strerror(errno));
        exit(2);
    }
    for (i = 0; i < node->number_of_nodes + 1; i++) {
        if (!(wimpipe[i] = (int *) malloc(2 * sizeof (int)))) {
            pax_log(LOG_ERR,"Not enough memory for IP table allocation: %s\n", strerror(errno));
            exit(2);
        }
        pipe_ret = pipe(wimpipe[i]);
        if (pipe_ret == -1) {
            pax_log(LOG_ERR,"Cannot create pipes: %s\n", strerror(errno));
            exit(1);
        }

    }
    if (!(paxpipe = (int **) malloc((node->number_of_nodes + 1) * sizeof (int *)))) {
        pax_log(LOG_ERR,"Pipe pool memory allocation: %s\n", strerror(errno));
        exit(2);
    }

    for (i = 0; i < node->number_of_nodes + 1; i++) {
        if (!(paxpipe[i] = (int *) malloc(2 * sizeof (int)))) {
            pax_log(LOG_ERR,"Not enough memory for pipe allocation: %s\n", strerror(errno));
            exit(2);
        }
        pipe_ret = pipe(paxpipe[i]);
        if (pipe_ret == -1) {
            pax_log(LOG_ERR,"Cannot create pipes: %s\n", strerror(errno));
            exit(1);
        }

    }

    pipe_ret = pipe(metric_pipe);
    if (pipe_ret == -1) {
        pax_log(LOG_ERR,"Cannot create pipes!: %s\n", strerror(errno));
        exit(1);
    }

    pipe_ret = pipe(metric_pipe_resp);
    if (pipe_ret == -1) {
        pax_log(LOG_ERR,"Cannot create pipes!: %s\n", strerror(errno));
        exit(1);
    }

}

/*
 * Prints information about node table
 */
void print_node_info(struct node_struct node) {
    int i;

    for (i = 0; i < node.number_of_nodes; i++) {
        pax_log(LOG_DEBUG,"[%d] %s %d online: %d", i,
                node.machtab[i].ip,
                node.machtab[i].port,
                node.machtab[i].connected);
        if (i == node.id) pax_log(LOG_DEBUG," - local");
        pax_log(LOG_DEBUG,"\n");
    }
}

/*
 * Adds node into the node table.
 * The idp parameter is the unique id after insertion.
 * If the real_change is not set, then the node is added into the table,
 * but the state is set to "disconnected"
 */
int machtab_add(struct node_struct *node, int fd, char *ip,
        int *idp, PAXOS_BOOL real_change) {

    int ret, index, fret = 0;

    ret = 0;

    
    index = ip2index(ip);
    if (index == -1) {
        pax_log(LOG_ERR, "machtab_add:Index out of bound\n");
        return -1;
    }

    if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    if (node->machtab[index].connected != BTRUE) {
        if (real_change) {
            node->machtab[index].connected = BTRUE;
            node->machtab[index].fd = fd;
            node->current++;
        }
    } else {
        /*Already known*/
        fret = 2;
    }
    if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    if (idp != NULL)
        *idp = index;
    if (ret == 0)
        return fret;
    return (ret);

}

/*
 * removes the node from the table.
 * If lock is set then the mtmutex must be locked
 */
int machtab_rem(struct node_struct *node, int eid, int lock) {
    int ret;
    ret = 0;
    if (lock != 0)
        if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
    node->machtab[eid].connected = BFALSE;
    if(node->machtab[eid].fd) close(node->machtab[eid].fd);
    if (node->current > 0)node->current--;
    if (lock != 0)
        if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }

    return (ret);
}

/*
 * Prints the machine table
 */
void machtab_print() {
    int i;
    pax_log(LOG_DEBUG,"IP configuration table:\n");
    for (i = 0; i < node.number_of_nodes; i++) {
        pax_log(LOG_DEBUG,"[%d] %s, port: %d, connected: %d\n", i,
                node.machtab[i].ip,
                node.machtab[i].port,
                node.machtab[i].connected);
    }
}

/*For ip returns index in the list */
int ip2index(char * ip) {
    int i;
    for (i = 0; i < node.number_of_nodes; i++) {
        if (strcmp(node.machtab[i].ip, ip) == 0) return i;
    }
    return -1;
}

/*For index in the list returns id*/
int id2index(int id) {
    int i;
    for (i = 0; i < node.number_of_nodes; i++) {
        if (node.machtab[i].id == id) return i;
    }
    return -1;
}
