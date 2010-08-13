
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "api.h"
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include "pbs_cache.h"
#include "client.h"

#define NUM_CLIENT 2
#define NUM_REP 4

#define PORT 9019
#define BUF_SIZE 1024
#define DEBUG 0
#define NUM 300
#define IP "147.251.3.17"
#define WAY -1
char ip[NUM][16] = {"147.251.3.17", "147.228.67.30", "147.251.3.130", "147.228.43.85"};

int listener; /* listening socket descriptor*/

/*Message for node with ID = i*/
int message(char * msg) {
    struct sockaddr_in remote_addr; /* client address*/
    int my_socket;
    int size;
    char buf[BUF_SIZE];

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(PORT);

    if ((my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Distributed message() socket:");
        return -1;
    }

    //if (DEBUG > 1) printf("Message: %s for %s\n", msg, ip[i]);

    remote_addr.sin_addr.s_addr = inet_addr(IP);

    if (sendto(my_socket, msg, BUF_SIZE - 1, 0,
            (struct sockaddr *) & remote_addr, sizeof (remote_addr)) == -1) {
        perror("Distributed message() sendto:");
        return -1;
    }

    if ((size = recv(listener, buf, 2, 0)) == -1) {
        perror("Probl�m s p�ijet�m dat");
        return -1;
    }
    //printf("P�ijato %s\n", buf);


    close(my_socket);
    return 0;
}

struct metric_rec {
    char host[3][1024], value[3][1024], test[1024];
    long ts[3];
};

typedef struct metric_rec mtr;
mtr mtc[3];

/*char * reffer_get(char * ref, int id, int i) {
    sprintf(ref, "%s\t%ld\t%s\n", mtc[id].host[i], mtc[id].ts[i], mtc[id].value[i]);
    return ref;
}*/

char * reffer_get(char * ref, int id, int i) {
    sprintf(ref, "%s\t%s\n", mtc[id].host[i], mtc[id].value[i]);
    return ref;
}

int host_id(char * host) {
    if (strcmp(host, "host1") == 0) {
        return 0;
    } else if (strcmp(host, "host2") == 0) {
        return 1;
    } else if (strcmp(host, "host3") == 0) {
        return 2;
    }
    return -15;
}

int metric_id(char * metric) {
    if (strcmp(metric, "test1") == 0) {
        return 0;
    } else if (strcmp(metric, "test2") == 0) {
        return 1;
    } else if (strcmp(metric, "test3") == 0) {
        return 2;
    }
    printf("Not found\n");
    return -15;
}

int compare(char * mes, int i) {
    char * msg, *host, *value, *ts, inter[512], *cookie;
    int j;

    if ((host = strtok_r(mes, "\t", &cookie)) == NULL) {
        perror("Receiver: Command not found:");
        return 1;
    }
    if ((ts = strtok_r(cookie, "\t", &value)) == NULL) {
        perror("Receiver: Command not found:");
        return 1;
    }
    sprintf(inter, "%s\t%s", host, value);

    for (j = 0; j < 3; j++) {
        msg = (char *) malloc(1024);
        if (msg == NULL) exit(-15);
        reffer_get(msg, i, j);
        _log(WAY, LOG_DEBUG, "comparing '%s' and '%s'\n", inter, msg);
        if (strcmp(inter, msg) == 0) return 0;
        free(msg);
    }
    return 1;
}

void list() {
    FILE * stream;
    int i, ret;
    for (i = 0; i < 4; i++) {
        stream = cache_connect_net(ip[i], PBS_CACHE_PORT);
        ret = cache_list(stream, "test1", stdout);
        if (ret)
            _log(WAY, LOG_DEBUG, "cache_list failed\n");
        printf("\n\n-----------------------------\n\n");
        ret = cache_list(stream, "test2", stdout);
        if (ret)
            _log(WAY, LOG_DEBUG, "cache_list failed\n");
        printf("\n\n-----------------------------\n\n");
        ret = cache_list(stream, "test3", stdout);
        if (ret)
            _log(WAY, LOG_DEBUG, "cache_list failed\n");

        cache_close(stream);
    }
}


int flush_and_check() {
    char buf1[NUM_REP][1024];
    /*char * msg;*/
    int i = 0;
    //FILE * stream;
    FILE * stream[4];
    int j, retr = 0, k = 0;
    char metric[3][6] = {"test1", "test2", "test3"};
    int term;

    list();


    for (i = 0; i < 3; i++) {
again:
        _log(WAY, LOG_DEBUG, "flush main loop\n");
        list();
        term = 0;
        if (retr > 45) {
            fprintf(stderr, "too many retries, exiting\n");
            exit(-15);
        }
        for (j = 0; j < 4; j++) {
            _log(WAY, LOG_DEBUG, "Openning %d\n", j);
            stream[j] = cache_connect_net(ip[j], PBS_CACHE_PORT);
            if (stream[j] == NULL) {
                _log(WAY, LOG_DEBUG, "Null stream\n");
                for (k = 0; k < j; k++) {
                    _log(WAY, LOG_DEBUG, "%d/%d\n", k, j);
                    if (stream[k]) cache_close(stream[k]);
                }
                i = 0;
                goto again;
            }
            fprintf(stream[j], " list %s\n", metric[i]);
            fgets(buf1[j], 1024, stream[j]);
            if (strncmp(buf1[j], "205 list ok", 4) != 0) {
                _log(WAY, LOG_DEBUG, "Chybny obsah '%s'\n", buf1[j]);
                for (k = 0; k <= j; k++) {
                    _log(WAY, LOG_DEBUG, "%d\n", k);
                    if (stream[k]) cache_close(stream[k]);
                }
                i = 0;
                goto again;
            }
        }
        for (j = 0; j < 3; j++) {
            fgets(buf1[0], 1024, stream[0]);
            _log(WAY, LOG_DEBUG, "1. got '%s'\n", buf1[0]);
            if (strcmp(buf1[0], "201 OK list\n") != 0) {
                _log(WAY, LOG_DEBUG, "Comparing.\n");
                if (compare(buf1[0], i) != 0) {
                    _log(WAY, LOG_DEBUG, "Comparisson failed.\n");
                    sleep(2);
                    i = 0;
                    retr++;
                    break;
                }
            } else {
                _log(WAY, LOG_DEBUG, "Probably the end.\n");
                term++;

            }
            fgets(buf1[1], 1024, stream[1]);
            _log(WAY, LOG_DEBUG, "2. got '%s'\n", buf1[1]);
            if (strcmp(buf1[1], "201 OK list\n") != 0) {
                _log(WAY, LOG_DEBUG, "Comparing.\n");
                if (compare(buf1[1], i) != 0) {
                    _log(WAY, LOG_DEBUG, "Comparisson failed.\n");
                    sleep(2);
                    i = 0;
                    retr++;
                    break;
                }
            } else {
                _log(WAY, LOG_DEBUG, "Probably the end.\n");
                term++;
            }
            fgets(buf1[2], 1024, stream[2]);
            _log(WAY, LOG_DEBUG, "3. got '%s'\n", buf1[2]);
            if (strcmp(buf1[2], "201 OK list\n") != 0) {
                _log(WAY, LOG_DEBUG, "Comparing.\n");
                if (compare(buf1[2], i) != 0) {
                    _log(WAY, LOG_DEBUG, "Comparisson failed.\n");
                    sleep(2);
                    i = 0;
                    retr++;
                    break;
                }
            } else {
                _log(WAY, LOG_DEBUG, "Probably the end.\n");
                term++;
            }

            fgets(buf1[3], 1024, stream[3]);
            _log(WAY, LOG_DEBUG, "4. got '%s'\n", buf1[3]);
            if (strcmp(buf1[3], "201 OK list\n") != 0) {
                _log(WAY, LOG_DEBUG, "Comparing.\n");
                if (compare(buf1[3], i) != 0) {
                    _log(WAY, LOG_DEBUG, "Comparisson failed.\n");
                    sleep(2);
                    i = 0;
                    retr++;
                    break;
                }
            } else {
                _log(WAY, LOG_DEBUG, "Probably the end.\n");
                term++;
            }
            if (term == 4) break;
            if (term > 0) {

            }

        }
        _log(WAY, LOG_DEBUG, "Closing all\n");
        cache_close(stream[0]);
        cache_close(stream[1]);
        cache_close(stream[2]);
        cache_close(stream[3]);
    }

    /*
        msg = (char *)malloc(1024);
        if (msg==NULL) exit(-15);
        reffer_get(msg, 2);
        if (strcmp(buf3[0], msg) == 0);
        free(msg);

        msg = (char *)malloc(1024);
        if (msg==NULL) exit(-15);
        reffer_get(msg, 1);
        if (strcmp(buf2[0], msg) == 0);
        free(msg);

        msg = (char *)malloc(1024);
        if (msg==NULL) exit(-15);
        reffer_get(msg, 0);
        if (strcmp(buf1[0], msg) == 0);
        free(msg);*/
    return 0;
}

void save_val(char * host, char * metric, char * value, long stamp) {
    //if (mtc[metric_id(metric)].ts[host_id(host)] <= stamp) {
        strcpy(mtc[metric_id(metric)].host[host_id(host)], host);
        strcpy(mtc[metric_id(metric)].value[host_id(host)], value);
        mtc[metric_id(metric)].ts[host_id(host)] = stamp;
    //}
}

int main(int argc, char **argv) {
    FILE *stream, *inf = NULL;
    int ret, i = 0, ind;
    char buf[LINE_MAX + 1], * host, *metric, *cookie, *value, *retn, *c1, *t;
    int count = 0;
    srand(time(NULL));


    report_init("client.log");

    /* is it correct?*/
    signal(SIGPIPE, SIG_IGN);


    if ((inf = fopen("soubor", "r")) == NULL) {
        perror("opening file");
        exit(-1);
    }
    while (fgets(buf, LINE_MAX, inf)) {
        if ((host = strtok_r(buf, ";", &cookie)) == NULL) {
            fprintf(stderr, "wrong  format\n");
            exit(-1);
        }
        if ((metric = strtok_r(cookie, ";", &cookie)) == NULL) {
            fprintf(stderr, "wrong  format\n");
            exit(-1);
        }
        cookie[strlen(cookie) - 1] = '\0';
        if (DEBUG) {
            printf("ip %s,  host '%s', metric '%s', val '%s'\n", ip[rand() % 3], host, metric, cookie);
        } else {
            save_val(host, metric, cookie, 0);
back:
            ind = rand() % 3;
            _log(WAY, _LOG_DEBUG, "connecting to %s\n", ip[ind]);
            stream = cache_connect_net(ip[ind], PBS_CACHE_PORT);
            _log(WAY, _LOG_DEBUG, "Connected\n");
            if (stream != NULL) {
                ret = cache_store(stream, host, metric, cookie);
                if (ret) {
                    cache_close(stream);
                    fprintf(stderr, "cache_store failed\n");
                    sleep(1);
                    goto back;
                }
                _log(WAY, _LOG_DEBUG, "%d '%s' '%s' '%s'\n", i, host, metric, cookie);
                //sleep(2);
                value = cache_get(stream, host, metric);
                if (value == NULL) {
                    _log(WAY, _LOG_DEBUG, "cache_store failed, value not successfully saved\n");
                    cache_close(stream);
                    sleep(2);
                    goto back;
                }
                _log(WAY, _LOG_DEBUG, "Returned %s\n", value);
                if ((t = strtok_r(value, "\t", &retn)) == NULL) {
                    _log(WAY, _LOG_DEBUG, "wrong  format\n");
                    cache_close(stream);
                    sleep(2);
                    goto back;
                }
                if ((t = strtok_r(retn, "\n", &c1)) == NULL) {
                    _log(WAY, _LOG_DEBUG, "wrong  format\n");
                    cache_close(stream);
                    sleep(2);
                    goto back;
                }

                _log(WAY, _LOG_DEBUG, "value '%s' cookie '%s'\n", t, cookie);
                if (strcmp(cookie, t) != 0) {
                    _log(WAY, _LOG_DEBUG, "cache_store failed, value not successfully saved\n");
                    cache_close(stream);
                    sleep(2);
                    goto back;
                }
                cache_close(stream);
                count++;
                if (count == 25) {
                    sleep(5);
                    if (flush_and_check() == 0) {
                    } else {
                        /*error*/

                    }
                    count = 0;
                }

            } else {
                _log(WAY, _LOG_DEBUG, "cache_connect failed: %s\n", strerror(errno));
                sleep(2);
                goto back;
            }
        }
        i++;
    }
    fclose(inf);
    for (i = 0; i < 3; i++) {
        _log(WAY, _LOG_DEBUG, "%d\n", i);
        stream = cache_connect_net(ip[i], PBS_CACHE_PORT);
        ret = cache_list(stream, "test1", stdout);
        if (ret)
            _log(WAY, _LOG_DEBUG, "cache_list failed\n");
        _log(WAY, _LOG_DEBUG, "\n\n\n\n");
        ret = cache_list(stream, "test2", stdout);
        if (ret)
            _log(WAY, _LOG_DEBUG, "cache_list failed\n");
        _log(WAY, _LOG_DEBUG, "\n\n\n\n");
        ret = cache_list(stream, "test3", stdout);
        if (ret)
            _log(WAY, _LOG_DEBUG, "cache_list failed\n");

        cache_close(stream);
    }
    return 0;
}


