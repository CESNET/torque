/* $Id: storage.c,v 1.11 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * Manipulation with permanent storage of domains.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.11 $ $Date: 2009/06/04 11:12:30 $
 */

#include "../config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils.h"
#include "../net.h"
#include "status.h"
#include "domains.h"
#include "cpus.h"
#include "storage.h"


#define STORE_ID            "vm"
#define STORE_IP            "ip"
#define STORE_FQDN          "fqdn"
#define STORE_NAME          "name"
/* FIXME #define STORE_CLUSTER       "cluster" */
#define STORE_STATUS        "status"
#define STORE_JOBS          "running-jobs"
#define STORE_JOB           "job"
#define STORE_PREEMPTIVE    "preemptive"
#define STORE_REBOOTING     "rebooting"
#define STORE_CLUSTER       "cluster-name"


/** Status code to string conversion array. */
static const char *sc_str[] = {
    /* SCSTO_OK */
    "success",
    /* SCSTO_OUT_OF_MEMORY */
    "not enough memory",
    /* SCSTO_UNINITIALIZED */
    "cannot open uninitialized permanent storage",
    /* SCSTO_REOPEN */
    "trying to open storage file, which has already been opened",
    /* SCSTO_OPEN */
    "cannot open storage file",
    /* SCSTO_CLOSED */
    "cannot access closed storage file",
    /* SCSTO_SYNTAX */
    "syntax error",
    /* SCSTO_EMPTY */
    "ignoring empty line",
    /* SCSTO_END_OF_STATUS_CODE_LIST */
    NULL
};


/** Complete filename including master's IP address and port.
 * When found, this file is read rather then STORAGE_FILE to allow multiple
 * instances of master daemon on a single node.
 */
static char *storage_file = NULL;


/** File stream to the storage_file. */
static FILE *storage = NULL;


const char *storage_status_code_string(int status)
{/*{{{*/
    int code = status & STATUS_CODE_MASK;

    if ((status & ~(STATUS_CODE_MASK | STATUS_TYPE_MASK)) == SMAGIC_STORAGE
        && code < SCSTO_END_OF_STATUS_CODE_LIST)
        return sc_str[code];
    else
        return "unknown status code";
}/*}}}*/


int storage_init(const char *ip, int port)
{/*{{{*/
    int size;

    size = strlen(STORAGE_FILE) + 1 + 5 + 1;

    if (ip != NULL)
        size += strlen(ip) + 1;

    if ((storage_file = (char *) malloc(size)) == NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_OUT_OF_MEMORY;

    if (ip != NULL)
        snprintf(storage_file, size, "%s.%s.%d", STORAGE_FILE, ip, port);
    else
        snprintf(storage_file, size, "%s.%d", STORAGE_FILE, port);

    log_msg(LOG_INFO, 0, "State will be written to %s", storage_file);

    return SMAGIC_STORAGE | STATUS_OK | SCSTO_OK;
}/*}}}*/


int storage_open(int write)
{/*{{{*/
    if (storage_file == NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_UNINITIALIZED;

    if (storage != NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_REOPEN;

    if (write)
        storage = fopen(storage_file, "w+");
    else {
        if ((storage = fopen(storage_file, "r")) != NULL)
            log_msg(LOG_INFO, 0, "Loading state from %s", storage_file);
        else if ((storage = fopen(STORAGE_FILE, "r")) != NULL)
            log_msg(LOG_INFO, 0, "Loading state from %s", STORAGE_FILE);
    }

    if (storage == NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_OPEN;
    else
        return SMAGIC_STORAGE | STATUS_OK | SCSTO_OK;
}/*}}}*/


void storage_close(void)
{/*{{{*/
    if (storage != NULL)
        fclose(storage);

    storage = NULL;
}/*}}}*/


int storage_eof(void)
{/*{{{*/
    return (storage == NULL || feof(storage));
}/*}}}*/


int storage_domain_save(struct domain *dom)
{/*{{{*/
    int i;

    if (storage == NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_CLOSED;

    fprintf(storage, STORE_ID " %lu\n", (unsigned long) dom->id);
    fprintf(storage, STORE_FQDN " %s\n", dom->fqdn);
    if (dom->cluster != NULL)
        fprintf(storage, STORE_CLUSTER " %s\n", dom->cluster);
    fprintf(storage, STORE_NAME " %s\n", dom->name);
    fprintf(storage, STORE_IP " %s %u\n", net_addr_str(dom->ip), dom->port);
    fprintf(storage, STORE_STATUS " %s\n",
            domains_status_string(domains_status_get(dom)));
    if (dom->cluster_name != NULL)
        fprintf(storage, STORE_CLUSTER " %s\n", dom->cluster_name);
    if (dom->rebooting)
        fprintf(storage, STORE_REBOOTING " %d\n", 1);

    fprintf(storage, STORE_JOBS " %d\n", dom->running_jobs);
    for (i = 0; i < dom->running_jobs; i++) {
        if (dom->jobs[i] != NULL) {
            fprintf(storage, STORE_JOB " %s %c %d %ld %ld\n",
                    dom->jobs[i]->id,
                    dom->jobs[i]->preemptible ? 'P' : 'N',
                    dom->jobs[i]->cpus,
                    dom->jobs[i]->preempted_start,
                    dom->jobs[i]->preempted_time);
        }
        else
            fprintf(storage, STORE_JOB " (null)\n");
    }

    fprintf(storage, STORE_PREEMPTIVE " %d\n", dom->can_preempt);
    fprintf(storage, "\n");

    return SMAGIC_STORAGE | STATUS_OK | SCSTO_OK;
}/*}}}*/


int storage_domain_load(struct domain *dom)
{/*{{{*/
    char line[1024];
    struct args *args;
    int lines = 0;
    int job = 0;

    if (storage == NULL)
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_CLOSED;

    memset(dom, '\0', sizeof(struct domain));
    domains_status_set(dom, UNDEFINED);
    dom->ip = net_inaddr_any;

    while (fgets(line, 1024, storage) != NULL && line[0] != '\n') {
        lines++;
        line[strlen(line) - 1] = '\0';

        if ((args = line_parse(line)) == NULL)
            continue;

        if (args->argc < 2) {
            free(args);
            continue;
        }

        if (strcmp(args->argv[0], STORE_ID) == 0)
            dom->id = atol(args->argv[1]);
        else if (strcmp(args->argv[0], STORE_IP) == 0) {
            if (!net_addr_parse(args->argv[1], &dom->ip)) {
                dom->ip = net_inaddr_any;
                log_msg(LOG_WARNING, 0, "Invalid IP address: %s",
                        args->argv[1]);
            }

            if (args->argc <= 2
                || (dom->port = atoi(args->argv[2])) <= 0
                || dom->port > 0xffff)
                dom->port = DEFAULT_PORT;
        }
        else if (strcmp(args->argv[0], STORE_FQDN) == 0)
            dom->fqdn = strdup(args->argv[1]);
        else if (strcmp(args->argv[0], STORE_CLUSTER) == 0) {
            line_parse_undo(args);
            dom->cluster = strdup(line + strlen(STORE_CLUSTER) + 1);
        }
        else if (strcmp(args->argv[0], STORE_NAME) == 0)
            dom->name = strdup(args->argv[1]);
        else if (strcmp(args->argv[0], STORE_STATUS) == 0) {
            domains_status_set(dom, domains_status_decode(args->argv[1]));
            if (domains_status_get(dom) == UNDEFINED) {
                log_msg(LOG_WARNING, 0, "Invalid domain status: %s",
                        args->argv[1]);
            }
        }
        else if (strcmp(args->argv[0], STORE_JOBS) == 0) {
            if (job == 0) {
                if ((dom->running_jobs = atoi(args->argv[1])) > 0) {
                    dom->jobs =
                        (struct job **) calloc(dom->running_jobs, sizeof(struct job *));
                }
            }
        }
        else if (strcmp(args->argv[0], STORE_JOB) == 0) {
            if (dom->running_jobs > job) {
                dom->jobs[job] = (struct job *)
                    malloc(sizeof(struct job) + strlen(args->argv[1]) + 1);
                if (dom->jobs[job] != NULL) {
                    strcpy(dom->jobs[job]->id, args->argv[1]);
                    dom->jobs[job]->preemptible = 0;
                    dom->jobs[job]->cpus = 0;
                    dom->jobs[job]->preempted_start = 0;
                    dom->jobs[job]->preempted_time = 0;

                    if (args->argc > 2) {
                        dom->jobs[job]->preemptible =
                            (args->argv[2][0] == 'P');
                    }
                    if (args->argc > 3)
                        dom->jobs[job]->cpus = atoi(args->argv[3]);
                    if (args->argc > 4) {
                        dom->jobs[job]->preempted_start =
                            atol(args->argv[4]);
                    }
                    if (args->argc > 5) {
                        dom->jobs[job]->preempted_time =
                            atol(args->argv[5]);
                    }

                    if (cpus_enabled())
                        dom->cpus += dom->jobs[job]->cpus;
                    job++;
                }
            }
        }
        else if (strcmp(args->argv[0], STORE_PREEMPTIVE) == 0)
            dom->can_preempt = atoi(args->argv[1]);
        else if (strcmp(args->argv[0], STORE_REBOOTING) == 0)
            dom->rebooting = atoi(args->argv[1]);
        else if (strcmp(args->argv[0], STORE_CLUSTER) == 0)
            dom->cluster_name = strdup(args->argv[1]);

        free(args);
    }

    if (lines <= 0)
        return SMAGIC_STORAGE | STATUS_WARN | SCSTO_EMPTY;
    else if (dom->fqdn != NULL
             && dom->name != NULL
             && domains_status_get(dom) != UNDEFINED
             && dom->running_jobs == job
             && !net_addr_equal(dom->ip, net_inaddr_any)) {
        if (domains_status_get(dom) == PREEMPTED) {
            domains_preemption_stop(dom);
            domains_preemption_start(dom);
        }

        return SMAGIC_STORAGE | STATUS_OK | SCSTO_OK;
    }
    else
        return SMAGIC_STORAGE | STATUS_ERROR | SCSTO_SYNTAX;
}/*}}}*/

