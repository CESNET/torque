/*
 *      net.c
 *      
 *      Copyright 2009 Jaroslav Tomeček <adray@phoenix>
 *      
 * 		Network management
 */
#include "main.h"

static int broadcast_message(struct node_struct * node, char * msg);
static int message(char * msg, int fd);
ssize_t readn(int fd, void *vptr, size_t n);

extern struct node_struct node;

/*
 *	Initialize a socket. Returns
 *	a file descriptor for the socket.
 */
int listen_socket_init(int port) {
    int s;
    int sockopt;
    struct sockaddr_in si;

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        pax_log(LOG_ERR,"can't create listen socket: %s\n", strerror(errno));
        return (-1);
    }

    memset(&si, 0, sizeof (si));
    si.sin_family = AF_INET;
    si.sin_addr.s_addr = htonl(INADDR_ANY);
    si.sin_port = htons((unsigned short) port);

    sockopt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
            (const char *) & sockopt, sizeof (sockopt));

    if (bind(s, (struct sockaddr *) & si, sizeof (si)) != 0) {
        pax_log(LOG_ERR,"can't bind listen socket: %s\n", strerror(errno));
        goto err;
    }

    if (listen(s, 5) != 0) {
        pax_log(LOG_ERR,"can't establish listen queue: %s\n", strerror(errno));
        goto err;
    }

    return (s);

err:
    close(s);
    return (-1);
}

/*
  *	Accept a connection on a given socket. 
 */
int listen_socket_accept(int s, int *eidp, int * id) {
    struct sockaddr_in si;
    socklen_t si_len;
    int host, ret;
    int ns;    
    char str[INET_ADDRSTRLEN];
	u_int16_t port, hs;

accept_wait:
    memset(&si, 0, sizeof (si));
    si_len = sizeof (si);
    ns = accept(s, (struct sockaddr *) & si, &si_len);
    if (ns == SOCKET_CREATION_FAILURE) {
        pax_log(LOG_ERR, "can't accept incoming connection: %s\n", strerror(errno));
        return ns;
    }
    host = ntohl(si.sin_addr.s_addr);

    if (read(ns, &hs, 2) <0) {
        close(ns);
        goto accept_wait;
	}
    port = ntohs(port);
    ret = machtab_add(&node, ns, inet_ntoa(si.sin_addr), eidp, BTRUE);
    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Machine added\n");
    if (ret == 2) {
        close(ns);
        goto accept_wait;
    } else if (ret != 0)
        goto err;

    inet_ntop(AF_INET, &(host), str, INET_ADDRSTRLEN);
    * id = ip2index(inet_ntoa(si.sin_addr));
    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Connected to host %d, eid = %d\n", host, *eidp);
    return (ns);

err:
    close(ns);
    return SOCKET_CREATION_FAILURE;
}

/*
 *	Connects to the remote machine
 *	and returns a file descriptor if succesful.
 * 
 *  If the connection was established already then the eid f the connection
 *	is set and set is_open to 1.
 */
int get_connected_socket(char *remotehost, int port, int *is_open, int *eidp) {
    int ret;
    int s;
    struct sockaddr_in si;
    unsigned int addr;
    u_int16_t nport;

    *is_open = 0;

    si.sin_addr.s_addr = inet_addr(remotehost);

    ret = machtab_add(&node, s, remotehost, eidp, BFALSE);
    if (ret == 2) {
        *is_open = 1;
        return (0);
    } else if (ret != 0) {
        return (-1);
    }

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        pax_log(LOG_ERR,"can't create outgoing socket: %s\n", strerror(errno));
        return (-1);
    }
    
    addr = ntohl(si.sin_addr.s_addr);


    si.sin_family = AF_INET;
    si.sin_port = htons((unsigned short) port);
    if (connect(s, (struct sockaddr *) & si, sizeof (si)) < 0) {
        pax_log(LOG_ERR, "connection failed: %s\n",
                strerror(errno));
        close(s);
        return (-1);
    } else {
        ret = machtab_add(&node, s, remotehost, eidp, BTRUE);
        if (ret == 2) {
            *is_open = 1;
            close(s);
            return (0);
        } else if (ret != 0) {
            close(s);
            return (-1);
        }    
    }


    nport = htons(PORT);
    write(s, &nport, 2);
    return (s);
}

/*
 * Gets new message from the file descriptor and saves it into
 * the buffer. If the timeout is needed then 'to' is set.
 */
int get_next_message(int fd, char * buffer, PAXOS_BOOL to) {
    size_t nr;
    int retval;
    struct timeval tv;
    fd_set read_fd_set;
    unsigned int size;

    tv.tv_sec = PING_TIMEOUT_SEC;
    tv.tv_usec = PING_TIMEOUT_USEC;

    /* Initialize the set of active sockets. */
    FD_ZERO(&read_fd_set); /* Empty set */
    FD_SET(fd, &read_fd_set); /* Add the socket to the set */
    retval = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &tv);
    if (retval == -1) {
        pax_log(LOG_ERR,"get_next_message: select(): %s\n", strerror(errno));
        exit(1);
    }
    if (retval == 0) {
        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "get_next_message: Timeout!\n");
        return -2;
    } else if (retval >= 0) {

        if (FD_ISSET(fd, &read_fd_set)) {
            nr = recv(fd, &size, 4, 0);/*The size of the message*/
            if (nr == 0) {
                pax_log(LOG_ERR, "Connection probably lost\n");
                return -1;
            }
            if (nr == -1) {
                pax_log(LOG_ERR, "Abnormal socket state\n");
                pax_log(LOG_ERR,"get_next_message: recv: %s\n", strerror(errno));
                return (-1);
            }
            if (nr != 4) {
                pax_log(LOG_ERR, "Header message not 4 bytes long (%d B)!\n", nr);
                return (-1);
            }
            if(size > MTR_BUF) {
                pax_log(LOG_ERR, "get_next_message: Message too long!\n");
                return -1;
            }
            nr = recv(fd, buffer, size, 0);
            if (nr == -1) {
                pax_log(LOG_ERR, "Abnormal socket state\n");
                pax_log(LOG_ERR,"get_next_message: recv: %s\n", strerror(errno));
                return (-1);
            }
            if (!nr) {
                pax_log(LOG_ERR, "Some receiver problem %d!\n", nr);
                return (-1);
            }
        }
    }

    if (buffer == NULL) {
        pax_log(LOG_ERR, "Fatal memory error in get_next_message in net.c\n");
        exit(EX_OSERR);
    }
    return (0);
}


/*
 * Sends broadcast message 
 */
int broadcast_send(char * msg) {
    int ret;
    broadcast_message(&node, msg);
    return (ret);
}

/*
* Sends broadcast message - internal function
* returns the number of correctly sent messages
*/
static int broadcast_message(struct node_struct * node, char * msg) {
    int ret, sent, i;

    if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }

    sent = 0;
    for (i = 0; i < node->number_of_nodes; i++) {/*For each connected site do*/
        if (node->machtab[i].connected == BTRUE) {
            if ((ret = message(msg, node->machtab[i].fd)) != 0) {
                pax_log(LOG_ERR, "socket write error in broadcast\n");
                (void) machtab_rem(node, i, 0);
            } else
                sent++;
        }
    }

    if (pthread_mutex_unlock(&node->mtmutex) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    return (sent);
}


/*
 * Sends one message the filed descriptor 'fd' 
 * 
 */
int send_mes(char * msg, int fd) {
    return message(msg, fd);
}

/*
 * Sends one message the filed descriptor 'fd' - internal function
 */
static int message(char * msg, int fd) {
    ssize_t nw;
    unsigned int size;

    size = strlen(msg);
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Sending message %s\n", msg);
    if ((nw = send(fd, (const char *) & size, 4, 0)) == -1) {/*Send the size of the message*/
        pax_log(LOG_ERR,"Sending message - header: %s\n", strerror(errno));
        return -1;
    }
    if ((nw = send(fd, msg, strlen(msg), 0)) == -1) {
        pax_log(LOG_ERR,"Sending message - body: %s\n", strerror(errno));
        return -1;
    }

    return (0);
}


void close_net(){
	int i;
	for(i=0;i<node.number_of_nodes;i++) {
		close(node.machtab[i].fd);
	}
}
