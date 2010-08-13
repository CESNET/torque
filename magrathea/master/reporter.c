/* $Id: reporter.c,v 1.18 2008/09/01 09:49:37 xdenemar Exp $ */

/** @file
 * Status reporter for pbs_cache.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.18 $ $Date: 2008/09/01 09:49:37 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#ifndef __USE_XOPEN2K
# define __USE_XOPEN2K
#endif
#include <semaphore.h>

#include "../utils.h"
#include "../pbs_cache/api.h"
#include "reporter.h"


#define WAIT 10
#define CACHE_KEEPOPEN  30

char *metric_name[] = {
    /* M_MAGRATHEA */   "magrathea",
    /* M_CLUSTER */     "machine_cluster",
    /* M_HOST */        "host"
};


struct cache_entry {
    pthread_mutex_t ref_mtx;
    int ref_count;
    enum metric metric;
    char *name;
    char *value;
};

struct cache_list_item {
    struct cache_list_item *prev;
    struct cache_list_item *next;
    struct cache_entry *entry;
};

struct cache {
    char *host;
    int port;
    FILE *stream;
    time_t last_sent;
    pthread_mutex_t list_mtx;
    struct cache_list_item *first;
    struct cache_list_item *last;
};

static struct cache *caches = NULL;
static int cache_count = 0;

static pthread_t thread;
static int running = 0;
static int propagate = 0;

static sem_t wakeup;

static void *reporter_main(void *arg);

static void list_put_tail(struct cache *cache, struct cache_list_item *item);
static void list_put_head(struct cache *cache, struct cache_list_item *item);
static struct cache_list_item *list_get_head(struct cache *cache);
static void cache_entry_ref(struct cache_entry *entry);
static void cache_entry_free(struct cache_entry *entry);


int reporter_add(char *host, int port)
{/*{{{*/
    struct cache *new;
    char *host_copy;

    if (running || host == NULL || port <= 0 || port > 65535)
        return -1;

    new = realloc(caches, sizeof(struct cache) * (cache_count + 1));
    host_copy = strdup(host);

    if (new != NULL)
        caches = new;

    if (new == NULL || host_copy == NULL) {
        if (host_copy != NULL)
            free(host_copy);

        log_msg(LOG_ERR, errno, "Cannot add status cache %s port %d",
                host, port);
        return -1;
    }

    caches[cache_count].host = host_copy;
    caches[cache_count].port = port;
    caches[cache_count].stream = NULL;
    caches[cache_count].last_sent = 0;
    caches[cache_count].first = NULL;
    caches[cache_count].last = NULL;

    log_msg(LOG_INFO, 0, "Status changes will be reported to %s port %d",
            host, port);

    if (pthread_mutex_init(&caches[cache_count].list_mtx, NULL))
        return -1;

    cache_count++;

    return 0;
}/*}}}*/


int reporter_start(void)
{/*{{{*/
    if (running || cache_count == 0)
        return 0;

    sem_init(&wakeup, 0, 0);

    if (pthread_create(&thread, NULL, reporter_main, NULL)) {
        log_msg(LOG_ERR, errno, "Cannot start cache reporting thread");
        return -1;
    }
    else {
        log_msg(LOG_INFO, 0, "Cache reporting thread started");
        running = 1;
        return 0;
    }
}/*}}}*/


void reporter_change(enum metric metric,
                     const char *name,
                     char *value,
                     int copy_value)
{/*{{{*/
    struct cache_entry *entry;
    int i;

    if (!running || name == NULL || value == NULL)
        return;

    entry = (struct cache_entry *) calloc(1, sizeof(struct cache_entry));
    if (entry == NULL
        || pthread_mutex_init(&entry->ref_mtx, NULL))
        return;

    entry->ref_count = 1;
    entry->metric = metric;
    entry->name = strdup(name);
    if (copy_value)
        entry->value = strdup(value);
    else
        entry->value = value;

    if (entry->name == NULL || entry->value == NULL) {
        cache_entry_free(entry);
        return;
    }

    log_msg(LOG_DEBUG, 0, "New cache data '%s' for '%s' in metric '%s'",
            entry->value, entry->name, metric_name[entry->metric]);

    for (i = 0; i < cache_count; i++) {
        struct cache_list_item *item = malloc(sizeof(struct cache_list_item));

        if (item == NULL)
            continue;

        cache_entry_ref(entry);
        item->prev = NULL;
        item->next = NULL;
        item->entry = entry;

        list_put_tail(caches + i, item);
    }

    sem_post(&wakeup);
    cache_entry_free(entry);
}/*}}}*/


void reporter_propagate(void)
{/*{{{*/
    if (!running)
        return;

    propagate = 1;
    sem_post(&wakeup);
    pthread_join(thread, NULL);
}/*}}}*/


static void *reporter_main(void *arg)
{/*{{{*/
    struct cache_list_item *item;
    struct cache *cache;
    int empty;
    int ok;
    int error;
    int i;

    while (1) {
        ok = 0;     /* something was sent */
        empty = 1;  /* nothing to send at all */
        error = 0;  /* an error occurred */

        for (i = 0; i < cache_count; i++) {/*{{{*/
            cache = caches + i;

            if ((item = list_get_head(cache)) != NULL) {
                empty = 0;

                if (cache->stream == NULL)
                    cache->stream = cache_connect_net(cache->host, cache->port);

                log_msg(LOG_DEBUG, 0,
                        "Reporting cache data '%s' for '%s' in metric '%s' "
                        "to %s, port %d",
                        item->entry->value, item->entry->name,
                        metric_name[item->entry->metric],
                        cache->host, cache->port);

                if (cache->stream == NULL
                    || cache_store(cache->stream, item->entry->name,
                                   metric_name[item->entry->metric],
                                   item->entry->value)) {
                    if (cache->stream != NULL) {
                        cache_close(cache->stream);
                        cache->stream = NULL;
                    }

                    log_msg(LOG_WARNING, 0,
                            "Cannot report cache data to %s, port %d",
                            cache->host, cache->port);

                    list_put_head(cache, item);
                    error = 1;
                }
                else {
                    cache->last_sent = time(NULL);
                    cache_entry_free(item->entry);
                    free(item);
                    ok = 1;
                }
            }

            if (cache->stream != NULL
                && time(NULL) - cache->last_sent > CACHE_KEEPOPEN) {
                cache_close(cache->stream);
                cache->stream = NULL;
            }
        }/*}}}*/

        if (propagate) {
            if (!empty && ok)
                continue;
            else
                break;
        }

        if (empty || (error && !ok)) {
            struct timespec ts;

            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += WAIT;
            sem_timedwait(&wakeup, &ts);
        }
    }

    log_msg(LOG_INFO, 0, "Cache reporting thread terminated");

    return NULL;
}/*}}}*/


static void cache_entry_ref(struct cache_entry *entry)
{/*{{{*/
    pthread_mutex_lock(&entry->ref_mtx);
    entry->ref_count++;
    pthread_mutex_unlock(&entry->ref_mtx);
}/*}}}*/


static void cache_entry_free(struct cache_entry *entry)
{/*{{{*/
    int count;

    pthread_mutex_lock(&entry->ref_mtx);
    count = --entry->ref_count;
    pthread_mutex_unlock(&entry->ref_mtx);

    if (count <= 0) {
        pthread_mutex_destroy(&entry->ref_mtx);
        free(entry->name);
        free(entry->value);
        free(entry);
    }
}/*}}}*/


static void list_put_tail(struct cache *cache, struct cache_list_item *item)
{/*{{{*/
    pthread_mutex_lock(&cache->list_mtx);

    item->prev = cache->last;
    item->next = NULL;
    if (cache->last != NULL)
        cache->last->next = item;
    if (cache->first == NULL)
        cache->first = item;
    cache->last = item;

    pthread_mutex_unlock(&cache->list_mtx);
}/*}}}*/


static void list_put_head(struct cache *cache, struct cache_list_item *item)
{/*{{{*/
    pthread_mutex_lock(&cache->list_mtx);

    item->prev = NULL;
    item->next = cache->first;
    if (cache->first != NULL)
        cache->first->prev = item;
    if (cache->last == NULL)
        cache->last = item;
    cache->first = item;

    pthread_mutex_unlock(&cache->list_mtx);
}/*}}}*/


static struct cache_list_item *list_get_head(struct cache *cache)
{/*{{{*/
    struct cache_list_item *item = NULL;

    pthread_mutex_lock(&cache->list_mtx);

    if (cache->first != NULL) {
        item = cache->first;

        cache->first = cache->first->next;
        if (cache->first != NULL)
            cache->first->prev = NULL;
        if (cache->last == item)
            cache->last = NULL;

        item->next = NULL;
    }

    pthread_mutex_unlock(&cache->list_mtx);

    return item;
}/*}}}*/

