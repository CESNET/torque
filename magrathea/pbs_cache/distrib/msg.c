/*
 *      msg.c
 *      
 *      Copyright 2009 Jaroslav Tomeček <adray@phoenix>
 *      
 * 		Message management and group members management
 */
#include "main.h"

static int connect_site(char *site, int *is_open,
        pthread_t *hm_thrp);
static void * msg_mng_loop(void *args);


extern struct node_struct node;
extern paxos_t paxos;

int cont_id;

typedef struct {
    int fd;
    unsigned int eid;
    int m_index;
} msg_mng_loop_args;

char proxy;

void proxy_start(){
    proxy = 1;
}

void proxy_stop(){
    proxy = 0;
}

char get_proxy(){
    return proxy;
}

/*
 * This is a generic message handling loop that is used both by the
 * master to accept messages from a client and vice versa.
 */
static void * msg_mng_loop(void *args) {
    msg_mng_loop_args *mma;
    int eid;
    int ret = 0,
            retr = 0; /*Number of connection retries*/
    int fd,
            m_index; /*Index in machtab list*/

    char *buffer, * cmd, * cookie, * metric, *cmd_t;
    struct node_struct * nd;
    long long ballot = -1, instance_proposed = -1, round = -1;
    char mes[BUF_SIZE], log_rec[BUF_SIZE];
    char id[5];
    pthread_t cnt_thr;

    nd = &node;
    mma = (msg_mng_loop_args *) args;


    fd = mma->fd;
    eid = mma->eid;
    m_index = mma ->m_index;

    buffer = (char *) malloc(sizeof (char) * MTR_BUF);
    if (buffer == NULL) {
        pax_log(LOG_ERR,"msg_mng_loop: malloc: %s\n", strerror(errno));
        exit(-1);
    }
    if (_PAXOS_DEBUG>2)pax_log(LOG_DEBUG,"Message loop for %d\n", m_index);
    for (ret = 0; ret == 0;) {
        memset(buffer, 0, MTR_BUF);
        if (_PAXOS_DEBUG)pax_log(LOG_DEBUG,"Ready for next mesg\n");
        ret = get_next_message(fd, buffer, BTRUE);
        if (_PAXOS_DEBUG>2)pax_log(LOG_DEBUG,"Get next message ret %d\n", ret);
        if (ret == -1 || retr > NRETR) {/*Connection lost*/
            if(fd)close(fd);
            if (_PAXOS_DEBUG) pax_log(LOG_DEBUG,"Connection with %d lost.\n", nd->machtab[eid].id);

            if ((ret = machtab_rem(&node, eid, 1)) != 0)
                break;
            if (_PAXOS_DEBUG>2) machtab_print();
            sleep(rand()%3);
            /*Triing to reestablish the connection*/
            if ((ret = pthread_create(&cnt_thr, NULL, contact_group, NULL)) != 0) {
                pax_log(LOG_ERR,"can't create connection thread: %s\n", strerror(errno));
                goto err;
            }
            if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't lock mutex\n");
                exit(EX_OSERR);
            }
            if(m_index == nd->updater_id){
                nd->updating = BFALSE;
            }
            if (nd->machtab[m_index].id == nd->master_id
                    || ((nd->current)) < QUORUM(nd)) {
                pax_log(LOG_ERR, "Connection with master or majority lost.\n");
                nd->master_id = -1;
                if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                sleep(1); /*Sleep some time to wait for others to notice the same event*/
                election(nd);
            } else if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                pax_log(LOG_ERR, "can't unlock mutex\n");
                exit(EX_OSERR);
            }
            break;
        }
        if (ret == -2) {/*Connection timeouted*/
            if (retr <= NRETR) ret = send_mes("PNG;", fd); /*Ping message*/
            else {
                pax_log(LOG_ERR, "Ping retried %d times. Node probably dead, closing connection.\n", NRETR);
                goto err;
            }
            retr++;
            continue;
        }
        if (buffer != NULL || strlen(buffer) != 0) {
            strncpy(log_rec, buffer, BUF_SIZE);
            if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Message %s received %d\n", buffer,  nd->machtab[eid].id);
            if ((cmd = strtok_r(buffer, ";", &cookie)) == NULL) {
                pax_log(LOG_ERR, "Receiver: Command not found\n");
                continue;
            }

            if (strncmp(cmd, "SHU", 3) == 0) {
                if(_PAXOS_DEBUG)pax_log(LOG_DEBUG, "Shutting down\n");
                exit(0);
            }

            /*Upon log - the remote node asked for log*/
            if (strncmp(cmd, "LOG", 3) == 0) {
                if ((cmd_t = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: LOG: Command not found:\n");
                    continue;
                }
                if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't lock mutex\n");
                    exit(EX_OSERR);
                }
                if(nd->non_voting==BTRUE){
                    continue;
                }
                if (update_remote_node(m_index, node)==-1) {
                    nd->master_id = -1;
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                    pax_log(LOG_DEBUG>2, "Updating remote node failed, remote node connection lost, election starting\n");
                    election(nd);
                }else if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                pax_log(LOG_DEBUG>2, "Updating started\n");
                continue;
            }

            /*Upon outdated message*/
            if (strncmp(cmd, "OUT", 3) == 0) {
                if ((cmd_t = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: OUT: Command not found:\n");
                    continue;
                }
                if (strncmp(cookie, "BEG", 3) == 0) {/*The transfer started*/
                    nd->updater_id = m_index;
                    clear_logs();
                    lock_snapshotting();
                    pax_log(LOG_ERR, "Logs cleared:\n");
                } else if (strncmp(cookie, "SSM", 3) == 0) {
                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: OUT: Command not found:\n");
                        continue;
                    }
                    write_snapshot_mark(cookie);
                } else
                    if (strncmp(cookie, "SST", 3) == 0) {/*Snapshot*/
                    char * met_name;

                    if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: OUT: Command not found:\n");
                        continue;
                    }
                    if ((met_name = strtok_r(cookie, ":", &cookie)) == NULL) {
                        pax_log(LOG_ERR, "Receiver: OUT: Command not found:\n");
                        continue;
                    }
                    write_metric_snap(met_name, cookie);
                }else
                if (strncmp(cookie, "FIN", 3) == 0) { /*The transfer finnished*/
                    int rt = 0;
                    if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't lock mutex\n");
                        exit(EX_OSERR);
                    }
                    rt = write_log_out_hash();
                    rt+= out_hash();
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                    rt += process_logs(&node);
                    unlock_snapshotting();
                    if (rt==0) {
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        if(_PAXOS_DEBUG) pax_log(LOG_DEBUG, "Voting allowed\n");
                        nd->updating = BFALSE;
                        nd->non_voting = BFALSE; /*The node can vote again*/
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                    }else {
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        if(_PAXOS_DEBUG) pax_log(LOG_DEBUG, "Wrong update\n");
                        send_mes("LOG;", fd);
                        nd->updater_id = m_index;
                        nd->updating = BTRUE;
                        nd->non_voting = BTRUE; /*The node can vote again*/

                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                    }
                } else write_log_out_record(cookie);
                continue;
            }
            /*UPON NextBallot(ballot, instance proposed)*/
            /*NXB:%lld:%lld;*/
            if (strncmp(cmd, "NXB", 3) == 0) {
                if ((cmd_t = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: NXB: Command not found:\n");
                    continue;
                }

                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: NXB: Command not found:\n");
                    continue;
                }
                ballot = atoll(cmd_t);
                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: NXB: Command not found:\n");
                    continue;
                }
                instance_proposed = atoll(cmd_t);
                round = atoll(cookie);

                sprintf(mes, "%lld:%lld", ballot, paxos.last_instance);
                if (ballot >= paxos.nextBallot) {
                    paxos.nextBallot = ballot;                    
                    if (instance_proposed == (paxos.last_instance + 1) && round == (paxos.previous_round+1)) {
                        char * ack_mes;
                        ack_mes = malloc((4 + sizeof (char)) * strlen(mes));
                        if (!ack_mes) {
                            pax_log(LOG_ERR,"NXB response message creation: %s\n", strerror(errno));
                            exit(EX_OSERR);
                        }

                        sprintf(ack_mes, "N_A:%s;", mes); /*Acknowledge the message*/
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        if(!nd->non_voting) {
                            send_mes(ack_mes, fd);
                        }
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                        free(ack_mes);

                    } else if (instance_proposed <= paxos.last_instance) {
                        /*Remote node is outdated*/
                        char * nack_mes;
                        nack_mes = malloc((4 + sizeof (char)) * strlen(mes));
                        if (!nack_mes) {
                            pax_log(LOG_ERR,"NXB response message creation: %s\n", strerror(errno));
                            exit(EX_OSERR);
                        }
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        sprintf(nack_mes, "N_N:%s;", mes); /*Nack the message*/
                        if(!nd->non_voting) {
                            send_mes(nack_mes, fd);
                        }
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                        free(nack_mes);
                    } else if(instance_proposed > (paxos.last_instance + 1) || round > (paxos.previous_round+1)){
                        /*Local node is outdated*/
                        /*char * ack_mes;*/
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        nd->non_voting = BTRUE; /*The local node cannot vote*/
                        if (!nd->updating) {
                            if (_PAXOS_DEBUG) pax_log(LOG_DEBUG, "NXB\n");
                            send_mes("LOG;", fd);
                            nd->updater_id = m_index;
                            nd->updating = BTRUE;
                        }
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                    }

                } else {
                    char * nack_mes;
                    nack_mes = malloc((4 + sizeof (char)) * strlen(mes));
                    if (!nack_mes) {
                        pax_log(LOG_ERR,"NXB response message creation: %s\n", strerror(errno));
                        exit(EX_OSERR);
                    }
                    if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't lock mutex\n");
                        exit(EX_OSERR);
                    }
                    sprintf(nack_mes, "N_N:%s;", mes); /*Nack the message*/
                    if(!nd->non_voting) {
                        send_mes(nack_mes, fd);
                    }
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                    }
                    
                    free(nack_mes);
                }
                continue;
            }

            /*UPON BeginBallot(ballot, instance,round, previous_instances, metric)*/
            if (strncmp(cmd, "BEB", 3) == 0) {
                if ((cmd_t = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: BEB: Command not found:\n");
                    continue;
                }

                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: BEB: Command not found:\n");
                    continue;
                }
                ballot = atoll(cmd_t);
                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: BEB: Command not found:\n");
                    continue;
                }
                instance_proposed = atoll(cmd_t);
                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: BEB: Command not found:\n");
                    continue;
                }
                round = atoll(cmd_t);
                metric = strdup(cookie);

                sprintf(mes, "%lld:%lld:%d:", instance_proposed, ballot, m_index);
                if (ballot >= paxos.nextBallot) {
                    if ((instance_proposed >= paxos.last_instance && round > paxos.previous_round)) {
                        if (instance_proposed > (paxos.last_instance + 1) || round > (paxos.previous_round + 1)) {
                            /*Local node is out-dated*/
                            if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't lock mutex\n");
                                exit(EX_OSERR);
                            }
                            nd->non_voting = BTRUE; /*Local node cannot vote*/
                            if(_PAXOS_DEBUG) pax_log(LOG_DEBUG, "BEB received - voting forbidden\n");
                            if (!nd->updating) {
                                send_mes("LOG;", fd);
                                nd->updater_id = m_index;
                                nd->updating = BTRUE;
                            }
                            if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't unlock mutex\n");
                                exit(EX_OSERR);
                            }
                        }

                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        write_log_record(log_rec, BFALSE, BFALSE);
                        /*Begin transaction*/
                        if (txn_begin(nd->machtab[m_index].id - 1, instance_proposed, round, metric) != 0) {
                            /*In the case of failure*/
                            sprintf(mes, "B_N:%lld:%lld:%lld:%d;", paxos.last_instance,
                                    paxos.previous_round, paxos.nextBallot, m_index); /*Nack the message*/
                            send_mes(mes, fd);
                            if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                                pax_log(LOG_ERR, "can't unlock mutex\n");
                                exit(EX_OSERR);
                            }
                            continue;
                        }
                        pax_log(LOG_DEBUG>2, "BEB txn done\n");
                        if(!nd->non_voting) {
                            sprintf(mes, "B_A:%lld:0:%lld:%d;", instance_proposed, ballot, m_index); /*Acknowledge the message*/
                            send_mes(mes, fd);
			}    
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                             exit(EX_OSERR);
                        }
                    }
                } else {
                    /*remote node is outdated*/
                    /*Nack the message*/
                    if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't lock mutex\n");
                        exit(EX_OSERR);
                    }
                    if(!nd->non_voting) {
                        sprintf(mes, "B_N:%lld:%lld:%lld:%d;", paxos.last_instance, paxos.previous_round, paxos.nextBallot, m_index);
                        send_mes(mes, fd);
                    }
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                       pax_log(LOG_ERR, "can't unlock mutex\n");
                       exit(EX_OSERR);
                    }

                }
                continue;
            }

            /*Ping message*/
            if (strncmp(cmd, "PNG", 3) == 0) {
                retr = 0;
            }


            //UPON Success(round , instance, previous_instances)
            //SUC:%lld:%lld:%s;
            if (strncmp(cmd, "SUC", 3) == 0) {

                if ((cmd_t = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: SUC: Command not found:\n");
                    continue;
                }

                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: SUC: Command not found:\n");
                    continue;
                }
                round = atoll(cmd_t);
                if ((cmd_t = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: SUC: Command not found:\n");
                    continue;
                }
                instance_proposed = atoll(cmd_t);
                if (instance_proposed >= paxos.last_instance && round > paxos.previous_round) {
                    char mtc[MTR_BUF];
                    if (instance_proposed > (paxos.last_instance + 1) || round > (paxos.previous_round + 1)) {
                        /*Local node is out-dated*/
                        if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't lock mutex\n");
                            exit(EX_OSERR);
                        }
                        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "SUC received - voting forbidden\n");
                        nd->non_voting = BTRUE; /*Local node cannot vote*/
                        if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                            pax_log(LOG_ERR, "can't unlock mutex\n");
                            exit(EX_OSERR);
                        }
                    }
                    if (pthread_mutex_lock(&nd->mtmutex) != 0) {
                        pax_log(LOG_ERR, "can't lock mutex\n");
                        exit(EX_OSERR);
                    }
                    paxos.last_instance = instance_proposed;
                    paxos.previous_round = round;
                    nd->master_id = nd->machtab[m_index].id;
                    nd->paxos_state = 0;

                    write_log_record(log_rec, BFALSE, BTRUE);
                    if (txn_commit(
                            nd->machtab[m_index].id - 1,
                            instance_proposed, round) == NULL) {
                        pax_log(LOG_ERR, "Local value cannot be changed to requested value. (null) value commited.\n");
                        pax_log(LOG_ERR, "Maybe something lost, voting forbidden.\n");
                        if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "SUC received - voting forbidden\n");
                        nd->non_voting = BTRUE; /*Local node cannot vote*/
                    } else {
                        strcpy(mtc, txn_commit(
                                nd->machtab[m_index].id - 1,
                                instance_proposed, round));

                        if (strncmp(mtc, "NA", 2) != 0) {/*If not voting round*/
                            if (strncmp(mtc, "REM:", 4) == 0) {/*If not removing*/
                                if (memory_remove_local(mtc) != 0) {
                                    pax_log(LOG_ERR, "Local value cannot be changed to requested value.\n");
                                }
                            } else if (memory_update_local(mtc) != 0) {
                                pax_log(LOG_ERR, "Local metric cannot be removed.\n");
                                if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"Value updated %s\n", mtc);
                            }
                        }
                        if(_PAXOS_DEBUG>2)print_db_state();
                        if (nd->non_voting)
                            if (nd->master_id == nd->machtab[m_index].id && !nd->updating) {
                                send_mes("LOG;", fd);
                                nd->updater_id = m_index;
                                nd->updating = BTRUE;
                            }

                        if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Commited %s\n", txn_commit(nd->machtab[m_index].id - 1, instance_proposed, round));
                    }
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                }else{
                    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"SUC ignoring. Should be inst: %lld >= %lld, round: %lld > %lld\n", instance_proposed, paxos.last_instance, round,  paxos.previous_round);
                }
                if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"New master is %d\n", nd->master_id);

            }

            /*B_A, B_N, N_A, N_N*/
            /*BEB acknowlegdes and NACKs*/
            if ((strncmp(cmd, "B_A", 3) == 0) || (strncmp(cmd, "B_N", 3) == 0)){
                if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't lock mutex\n");
                    exit(EX_OSERR);
                }
                if(nd->paxos_state != 4) {
                    if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Not allowed BA/BN\n");
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                    continue;
                }
                if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                sprintf(mes, "%s:%d;", cmd, m_index);
                /*Inform the initiate consensus thread*/
                write(paxpipe[m_index][1], mes, strlen(mes) * sizeof (char));

            }
            if ((strncmp(cmd, "N_A", 3) == 0) || (strncmp(cmd, "N_N", 3) == 0)) {
                if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't lock mutex\n");
                    exit(EX_OSERR);
                }

                if(nd->paxos_state != 2) {
                    if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Not allowed NA/NN, state %d\n", nd->paxos_state);
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                    continue;
                }
                if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                sprintf(mes, "%s:%d;", cmd, m_index);
                /*Inform the initiate consensus thread*/
                write(paxpipe[m_index][1], mes, strlen(mes) * sizeof (char));

            }


            /*Who is the master?*/
            if (strncmp(cmd, "WIM", 3) == 0) {
                if (pthread_mutex_lock(&nd->mtmutex) != 0) {
                    pax_log(LOG_ERR, "can't lock mutex\n");
                    exit(EX_OSERR);
                }
                if (nd->machtab[m_index].id == nd->master_id) {
                    nd->master_id = -1;
                    nd->updating = BFALSE;
                }
                sprintf(id, "%d", node.master_id);
                if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                sprintf(mes, "MIS:%s:%lld:%lld;", id, paxos.last_instance, paxos.previous_round);
                send_mes(mes, fd);
            }

            /*master is id*/
            /*MIS:id:inst:round;*/
            if (strncmp(cmd, "MIS", 3) == 0) {
                long long rd, inst;
                if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't lock mutex\n");
                    exit(EX_OSERR);
                }
                if(nd->paxos_state != 1) {
                    if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "Not allowed MIS\n");
                    if (_PAXOS_DEBUG>2) pax_log(LOG_DEBUG,"State is %d.\n", (int) nd->paxos_state);
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                    continue;
                }
                if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                    pax_log(LOG_ERR, "can't unlock mutex\n");
                    exit(EX_OSERR);
                }
                if ((cmd = strtok_r(cmd, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: MIS: Command not found:\n");
                    continue;
                }
                if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: MIS: Command not found:\n");
                    continue;
                }
                sprintf(mes, "ID:%d;", atoi(cmd));
                if ((cmd = strtok_r(cookie, ":", &cookie)) == NULL) {
                    pax_log(LOG_ERR, "Receiver: MIS: Command not found:\n");
                    continue;
                }
                inst = atoll(cmd);
                rd = atoll(cookie);
                if ((inst > (paxos.last_instance) && rd > (paxos.previous_round))
                        || (inst == (paxos.last_instance) && rd > (paxos.previous_round))) {
                    /*Local node is out-dated*/
                    if ((ret = pthread_mutex_lock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't lock mutex\n");
                        exit(EX_OSERR);
                    }
                    if(_PAXOS_DEBUG>2) pax_log(LOG_DEBUG, "MIS - voting forbidden\n");
                    nd->non_voting = BTRUE; /*Local node cannot vote*/
                    if (!nd->updating) {
                            send_mes("LOG;", fd);
                            nd->updater_id = m_index;
                            nd->updating = BTRUE;
                    }
                    if ((ret = pthread_mutex_unlock(&nd->mtmutex)) != 0) {
                        pax_log(LOG_ERR, "can't unlock mutex\n");
                        exit(EX_OSERR);
                    }
                }
                /*Inform ask_for_master*/
                write(wimpipe[m_index][1], mes, strlen(mes) * sizeof (char));
            }
        }

    }
err:
    if (_PAXOS_DEBUG>1) pax_log(LOG_DEBUG,"Connection thread exiting %d\n", m_index);
    if (buffer != NULL) free(buffer);
    free(mma);
    return ((void *) (uintptr_t) ret);
}

/*
 * Listen on the specific port for the incomming connections
 */
void * listen_thread(void *args) {
    msg_mng_loop_args *mma;
    pthread_t *hm_thrs;
    void *status;
    int i, eid, port, ret, id;
    int fd, ns, mult = 1;

    mma = NULL;
    port = PORT;

    if ((fd = listen_socket_init(port)) < 0) {
        ret = errno;
        goto err;
    }
    for (;;) {
        for (i = 0; i < MAX_THREADS * mult; i++) {
            if(_PAXOS_DEBUG>2)pax_log(LOG_DEBUG,"Pasive contacting: %d\n", i);
            if ((ns = listen_socket_accept(fd, &eid, &id)) == SOCKET_CREATION_FAILURE) {
                ret = errno;
                goto err;
            }
            if ((mma = malloc(sizeof (msg_mng_loop_args))) == NULL) {
                pax_log(LOG_ERR,"listen_thread: can't allocate memory: %s\n", strerror(errno));
                ret = errno;
                goto err;
            }
            mma->fd = ns;
            mma->eid = eid;
            mma->m_index = id;
            if ((ret = pthread_create(&(node.machtab[eid].hm_thr), NULL,
                    msg_mng_loop, (void *) mma)) != 0) {
                pax_log(LOG_ERR,"can't create thread for site: %s\n", strerror(errno));
                free(mma);
                goto err;
            }
        }
    }

    /* If we fell out, we ended up with too many threads. */
    ret = ENOMEM;

    /* Do not return until all threads exited. */
    while (--i >= 0)
        if (pthread_join(hm_thrs[i], &status) != 0)
            pax_log(LOG_ERR,"can't join site thread: %s\n", strerror(errno));

err:
    return (ret == 0 ? (void *) EXIT_SUCCESS : (void *) EXIT_FAILURE);
}

/*
 * Tries to contact all the nodes we know about 
 */
void * contact_group(void *args) {
    msg_mng_loop_args *mma;
    int failed, i, nsites, open, ret, *success;
    pthread_t *hm_thr;

    mma = NULL;

    nsites = node.number_of_nodes;
    ret = 0;
    hm_thr = NULL;
    success = NULL;

    cont_id ++;

    if ((success = calloc(nsites > 0 ? nsites : 1, sizeof (int))) == NULL) {
        pax_log(LOG_ERR,"contact_group: connect all memory allocation: %s\n", strerror(errno));
        ret = 1;
        goto err;
    }

    for (failed = nsites; failed > 0;) {
        for (i = 0; i < nsites; i++) {
            if (node.machtab[i].connected) {
                if(success[i] == 0) {
                    failed --;
                    success[i] = 1;
                }
                continue;
	    }
            ret = connect_site(node.machtab[i].ip, &open, &(node.machtab[i].hm_thr));

            if (ret == CON_FAILED)
                continue;

            if (ret != 0)
                goto err;

            failed--;
            success[i] = 1;
            /* If the connection is already open, we're done. */
            if (ret == 0 && open == 1)
                continue;

        }
        sleep(1);
    }
    if(_PAXOS_DEBUG>2)machtab_print();
err:
    if (success != NULL)
        free(success);
    if (hm_thr != NULL)
        free(hm_thr);

    return (ret ? (void *) EXIT_FAILURE : (void *) EXIT_SUCCESS);
}

/*
 * Tries to connect site with ip 'site'. If the connection is already
 * established then is_open is set to '1'. New message management thread
 * is started.
 */
static int connect_site(char *site, int *is_open,
        pthread_t * hm_thrp) {
    int eid, ret;
    int s;
    msg_mng_loop_args *mma;
    if ((s = get_connected_socket(site, PORT, is_open, &eid)) < 0)
        return (CON_FAILED);

    if (*is_open) {
        return (0);
    }
    if ((mma = calloc(sizeof (msg_mng_loop_args), 1)) == NULL) {
        pax_log(LOG_ERR,"connect site: msg_mng_loop struct allocation: %s\n", strerror(errno));
        ret = errno;
        goto err;
    }


    mma->fd = s;
    mma->eid = eid;
    mma->m_index = ip2index(site);
    if ((ret = pthread_create(hm_thrp, NULL,
            msg_mng_loop, (void *) mma)) != 0) {
        pax_log(LOG_ERR,"connect site:msg_mng_loop creation %s\n", strerror(errno));
        goto err1;
    }
    return (0);

err1:
    free(mma);
err:
    return (ret);
}
