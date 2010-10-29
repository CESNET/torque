/*
 *      paxos.c
 *      
 *      Copyright 2009 Jaroslav Tomeček <adray@phoenix>
 *      
 * 		The core of the paxos algorithm
 */
#include "main.h"

extern struct node_struct node;

void paxos_init() {
    paxos.lastTried = -1;
    paxos.last_instance = -1;
    paxos.previous_round = -1;
    paxos.nextBallot = -1;
    paxos.previous_vote = -1;
    TAILQ_INIT(&lsthead);
}

/*
 * Asks the other nodes who is the master. If at least quorum of the same
 * values is recevied then the master id is set to that value. Returns 0 in that
 * case -1 otherwise
 * name: ask_for_master
 */
int ask_for_master(struct node_struct * node) {
    int ret = 0, i, max, resp = 0, master_id_prop = -1, temp, arsp = 0;
    fd_set master; /* master file descriptor list*/
    int number_of_descriptors, fail = 0;
    char buf[BUF_SIZE], *cookie, *cmd;
    struct timeval tv;

    broadcast_send("WIM;");
    if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    node->paxos_state = 1;
    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"State set to 1.\n");
    if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
    FD_ZERO(&master);
    for (i = 0; i < node->number_of_nodes; i++) {
        FD_SET(wimpipe[i][0], &master);
        max = wimpipe[i][0];
    }
    for (resp = 0; resp < node->number_of_nodes;) {
        if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"ask for master loop %d of %d\n", resp, node->number_of_nodes);
        FD_ZERO(&master);
        for (i = 0; i < node->number_of_nodes; i++) {
            FD_SET(wimpipe[i][0], &master);
            max = wimpipe[i][0];
        }

        tv.tv_sec = RESPONSE_TIMEOUT_SEC;
        tv.tv_usec = RESPONSE_TIMEOUT_USEC;
        number_of_descriptors = select(FD_SETSIZE, &master, NULL, NULL, &tv);
        if (number_of_descriptors == 0) {
            if (resp >= QUORUM(node)) {
                if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
                   pax_log(LOG_ERR, "can't lock mutex\n");
                   exit(EX_OSERR);
                }
                node->paxos_state = 0;
                if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"State set to 0.\n");
                if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                   pax_log(LOG_ERR, "can't unlock mutex\n");
                   exit(EX_OSERR);
                }

                break;
            }
            broadcast_send("WIM;");
            if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            node->paxos_state = 1;
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"State set to 1.\n");
            if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            resp = 0;
            continue;
        }
        for (i = 0; i < node->number_of_nodes; i++) {
            if (FD_ISSET(wimpipe[i][0], &master)) {
                read(wimpipe[i][0], &buf, sizeof (buf));
                if (buf == NULL) {
                    pax_log(LOG_ERR, "Thread communication %s\n", strerror(errno));
                    exit(1);
                }
            } else continue;
            if ((cmd = strtok_r(buf, ";", &cookie)) == NULL) {
                pax_log(LOG_ERR, "Receiver: Command not found:\n");
                continue;
            }
            /*Who is the master?*/
            if (strncmp(cmd, "ID", 2) == 0) {
                if ((cmd = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: MIS: Command not found:\n");
                    continue;
                }
                resp++;
                temp = atol(cookie);
                if (temp != -1) {
                    arsp++;
                    if (master_id_prop == -1) {
                        master_id_prop = temp;
                    } else {
                        if (master_id_prop != temp) {
                            fail = 1;
                        }
                    }
                }
            }
        }
    }
    if (fail || arsp < QUORUM(node)) {
        ret = -1;
        if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"!Master id not chosen. %d!\n", master_id_prop);
        goto err;
    }
    if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    node->master_id = master_id_prop;
    //if (_PAXOS_DEBUG)printf("Non-voting %d updating %d \n", node->non_voting,  node->updating);
    if (node->non_voting &&  !node->updating) {
        send_mes("LOG;", node->machtab[id2index(master_id_prop)].fd);
        node->updater_id = id2index(master_id_prop);
        node->updating = BTRUE;
    }

    node->paxos_state = 0;
    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"State set to 0.\n");
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Master id chosen %d\n", master_id_prop);
    if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }
err:
    return ret;
}

/*
 * Paxos round master site function. Executes the round of paxos. If the 
 * 'voting' is non-zero then full round of paxos is processed. Returns zero 
 * in the case the quorum agrees.
 */
int initiate_consensus(struct node_struct *node, char * metric, PAXOS_BOOL voting) {
    int ret = 0;
    long long instance_proposed, round_prop;
    long ballot_rem, instance_rem, round_rem;
    char msg[MTR_BUF];
    fd_set master; /* master file descriptor list*/
    int number_of_descriptors, max;
    struct timeval tv;
    int i, n, arsp = 0, nrsp, id_rem;
    char buf[BUF_SIZE], *cmd, *cookie, *comd;

    if (_PAXOS_DEBUG)pax_log(LOG_DEBUG,"initiate_consensus %s\n", metric);
    paxos.lastTried = time(NULL) + node->id;
    round_prop = paxos.previous_round + 1;
    if (voting) {
        instance_proposed = paxos.last_instance + 1;
        snprintf(msg, BUF_SIZE - 1, "NXB:%lld:%lld:%lld;", paxos.lastTried, instance_proposed, round_prop);
        if (broadcast_send(msg) == 0) return -3;
        if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        node->paxos_state = 2;
        if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        tv.tv_sec = RESPONSE_TIMEOUT_SEC;
        for (n = 0; n < node->number_of_nodes;) {
            FD_ZERO(&master);
            for (i = 0; i < node->number_of_nodes; i++) {
                FD_SET(paxpipe[i][0], &master);
                max = paxpipe[i][0];
            }
            number_of_descriptors = select(FD_SETSIZE, &master, NULL, NULL, &tv);
            if (number_of_descriptors == 0) {
                if (n >= QUORUM(node)) break;
            }
            for (i = 0; i < node->number_of_nodes; i++) {
                if (FD_ISSET(paxpipe[i][0], &master)) {
                    read(paxpipe[i][0], &buf, sizeof (buf));
                    if (buf == NULL) {
                        pax_log(LOG_ERR, "Initiate consensus: Thread communication%s\n", strerror(errno));
                        exit(1);
                    }
                } else continue;
                if ((cmd = strtok_r(buf, ";", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: Command not found:\n");
                    continue;
                }

                /*ACK?*/
                /*N_A:%lld:%lld;*/
                /*ballot, paxos.last_instance*/
                /*NAK?*/
                /*N_N:%lld:%lld;*/
                /*ballot, paxos.last_instance*/
                if (strncmp(cmd, "N_A", 3) == 0 || strncmp(cmd, "N_N", 3) == 0) {
                    if ((comd = strtok_r(cmd, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: N_A: Command not found:\n");
                        continue;
                    }
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: N_A: Command not found:\n");
                        continue;
                    }
                    ballot_rem = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: N_A: Command not found:\n");
                        continue;
                    }
                    n++;
                    instance_rem = atoll(cmd);
                    id_rem = atoi(cookie);
                    if (strncmp(comd, "N_N", 3) == 0) {
                        /*Local node is outdated*/
                        if (instance_rem >= instance_proposed) {//INST
                            if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't lock mutex\n");
                                exit(EX_OSERR);
                            }
                            if (instance_rem > paxos.last_instance) {
                            if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "N:N - voting forbidden\n");
                                /*Forbid voting*/
                                node->non_voting = BTRUE;
                                clear_logs();
                            }
                            if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't unlock mutex\n");
                                exit(EX_OSERR);
                            }
                            return -3;
                        }
                        nrsp++;
                    } else {
                        /*if ((instance_rem + 1) < instance_proposed) {
                            update_remote_node(id_rem);
                        }*/
                        arsp++;
                    }

                }

            }
        }

        if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        if (node->master_id != -1) {
            if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            return -1;
        }
        if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG>2,"Initiate consensus - Starting second phase %d of %d\n", arsp, QUORUM(node));
    } else {
        instance_proposed = paxos.last_instance; //INST
        if (_PAXOS_DEBUG>2) {
            pax_log(LOG_DEBUG,"Initiate consensus - Without first phase\n");
        }
    }

    if (arsp >= QUORUM(node) || !voting) {
        arsp = 0;
        nrsp = 0;
        if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        if(voting) write_log_record(msg, BTRUE, BFALSE);

        round_prop = paxos.previous_round + 1;

        for (i = 0; i < node->number_of_nodes; i++) {
            if (node->machtab[i].connected == BTRUE) {
                memset(msg, 0, MTR_BUF);
                sprintf(msg, "BEB:%lld:%lld:%lld:%s;", paxos.lastTried, instance_proposed, round_prop, metric);
                send_mes(msg, node->machtab[i].fd);
            }
        }
        node->paxos_state = 4;
        if (pthread_mutex_unlock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        tv.tv_sec = RESPONSE_TIMEOUT_SEC;
        for (n = 0; n < node->number_of_nodes;) {
            FD_ZERO(&master);
            for (i = 0; i < node->number_of_nodes; i++) {
                FD_SET(paxpipe[i][0], &master);
                max = paxpipe[i][0];
            }

            number_of_descriptors = select(FD_SETSIZE, &master, NULL, NULL, &tv);
            if (number_of_descriptors == 0) {
                if (n >= QUORUM(node)) break;
            }
            for (i = 0; i < node->number_of_nodes; i++) {
                if (FD_ISSET(paxpipe[i][0], &master)) {
                    read(paxpipe[i][0], &buf, sizeof (buf));
                    if (buf == NULL) {
                        pax_log(LOG_ERR, "Initiate consensus: Thread communication%s\n", strerror(errno));
                        exit(1);
                    }
                } else continue;
                if ((cmd = strtok_r(buf, ";", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: Command not found:\n");
                    continue;
                }

                /*ACK?*/
                /*"B_A:%lld:%lld:%d:", instance_proposed, round, ballot, my_id;*/
                /*NACK?*/
                /*"B_N:%lld:%lld:%d:", instance_proposed, prevRound_remote, nextBallot, my_id;*/
                if (strncmp(cmd, "B_A", 3) == 0 || strncmp(cmd, "B_N", 3) == 0) {
                    if ((comd = strtok_r(cmd, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: B_A: Command not found:\n");
                        continue;
                    }
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: B_A: Command not found:\n");
                        continue;
                    }
                    instance_rem = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: B_A: Command not found:\n");
                        continue;
                    }
                    round_rem = atoll(cmd);
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: B_A: Command not found:\n");
                        continue;
                    }
                    ballot_rem = atoll(cmd);
                    id_rem = atoi(cookie);
                    n++;
                    if (strncmp(comd, "B_N", 3) == 0) {
                        if (instance_rem >= instance_proposed || (round_rem >= round_prop)) {
                            /*local node outdated*/
                            if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't lock mutex\n");
                                exit(EX_OSERR);
                            }
                            if (instance_rem > paxos.last_instance || (round_rem > paxos.previous_round)) {
                                if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "B:N - voting forbidden\n");
                                node->non_voting = BTRUE;
                                clear_logs();
                            }
                            if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't unlock mutex\n");
                                exit(EX_OSERR);
                            }
                            return -3;
                        }
                        nrsp++;
                    } else {
                        arsp++;
                    }
                }

            }
        }
        if (pthread_mutex_lock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't lock mutex\n");
            exit(EX_OSERR);
        }
        if (node->master_id != -1 && voting) {
            if (pthread_mutex_unlock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            return 0;
        }
        if (pthread_mutex_unlock(&node->mtmutex) != 0) {
            pax_log(LOG_ERR, "can't unlock mutex\n");
            exit(EX_OSERR);
        }
        if (arsp >= QUORUM(node)) {
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Initiate consensus - Second phase phase arsp %d/%d\n", arsp, QUORUM(node));
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Initiate consensus - Starting third phase\n");
            if (pthread_mutex_lock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            write_log_record(msg, BTRUE, BTRUE);
            paxos.last_instance = instance_proposed;
            paxos.previous_round = round_prop;
            for (i = 0; i < node->number_of_nodes; i++) {
                if (node->machtab[i].connected == BTRUE) {
                    memset(msg, 0, MTR_BUF);
                    sprintf(msg, "SUC:%lld:%lld;", round_prop, instance_proposed);
                    send_mes(msg, node->machtab[i].fd);
                }
            }
            node->paxos_state = 8;
            node->master_id = node->id;
            if (_PAXOS_DEBUG)pax_log(LOG_DEBUG,"The local node is new master %d\n", node->id);
            if (pthread_mutex_unlock(&node->mtmutex) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            return 0;
        }
        return -2;

    }
    return -3;
}

/*
 * Starts the election of the master. First of all asks for the master.
 * If the master is known, then returns 0. Otherwise checks if local node
 * can vote (is not outdated) and if so it starts the full round of paxos.
 * The loop is repeted until the master is locally known.
 */
int election(struct node_struct * node) {
    int mast_ret, iret = -1, ret;
    PAXOS_BOOL can = 0;
    if (_PAXOS_DEBUG) pax_log(LOG_DEBUG,"Election\n");
    if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't lock mutex\n");
        exit(EX_OSERR);
    }
    node->master_id = -1; /*Invalidate master id*/
    if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
        pax_log(LOG_ERR, "can't unlock mutex\n");
        exit(EX_OSERR);
    }

    do {/*Repeat until the master id is known*/
        mast_ret = ask_for_master(node);
        if (mast_ret == 0) {
            return 0;
        } else {
            if ((ret = pthread_mutex_lock(&node->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            if (node->non_voting != BTRUE) can = BTRUE;
            else can = BFALSE;
            if ((ret = pthread_mutex_unlock(&node->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            if (can) iret = initiate_consensus(node, "NA", BTRUE);
        }
        if (iret != 0) sleep(RESPONSE_TIMEOUT_SEC);
    } while (iret != 0);
    if (_PAXOS_DEBUG) pax_log(LOG_DEBUG,"New master is %d\n", node->master_id);
    return 0;
}

void * election_thread(){
    election(& node);
    return (void * ) NULL;
}

/*
 * Prints the paxos values 
 */
void paxos_state_print() {
    pax_log(LOG_DEBUG,"\nPaxos state:\n");
    pax_log(LOG_DEBUG,"Last ballot %lld\n", paxos.lastTried);
    pax_log(LOG_DEBUG,"Next ballot %lld\n", paxos.nextBallot);
    pax_log(LOG_DEBUG,"Next instance %lld\n", paxos.last_instance);
    pax_log(LOG_DEBUG,"Prev. round %lld\n\n", paxos.previous_round);
}
