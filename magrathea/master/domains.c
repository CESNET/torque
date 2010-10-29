/* $Id: domains.c,v 1.27 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * List of running domains (VMs) and their jobs.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.27 $ $Date: 2009/06/04 11:12:30 $
 */

#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "../utils.h"
#include "../net.h"
#include "status.h"
#include "vmm.h"
#include "cpus.h"
#include "storage.h"
#include "reporter.h"

#include "domains.h"


/** Domain status to string conversion array. */
static const char *status_str[] = {
    "--internal UNDEFINED--",
    "--internal UP--",

    "--internal separator--",

    "removed",
    "down",
    "down-bootable",
    "booting",
    "free",
    "occupied-would-preempt",
    "occupied",
    "running-preemptible",
    "running",
    "running-cluster",
    "preempted",
    "frozen",

    "--external separator--",

    "occupied-booting",
    "running-priority",
};


/** Status code to string conversion array. */
static const char *sc_str[] = {
    /* SCDOM_OK */
    "success",
    /* SCDOM_UNKNOWN */
    "domain not found",
    /* SCDOM_CHANGED_DOMAINS */
    "success, domain list changed",
    /* SCDOM_CHANGED_CPUS */
    "success, CPU allocation changed",
    /* SCDOM_CHANGED_DOMAINS_AND_CPUS */
    "success, domain list and CPU allocation changed",
    /* SCDOM_ALREADY_EXISTS */
    "domain already exists",
    /* SCDOM_OUT_OF_MEMORY */
    "not enough memory",
    /* SCDOM_REMOVE_RUNNING */
    "cannot remove running domain",
    /* SCDOM_LOAD_NOT_EMPTY */
    "cannot load storage file while domain list is not empty",
    /* SCDOM_JOB_EXISTS */
    "a job with the same ID is already running",
    /* SCDOM_OCCUPIED */
    "another domain is running",
    /* SCDOM_NOT_ENOUGH_CPUS */
    "not enough CPUs to start the job",
    /* SCDOM_NOT_ENOUGH_RESOURCES */
    "not enough resources",
    /* SCDOM_RUNNING_JOBS */
    "the domain is running jobs",
    /* SCDOM_NO_RUNNING_JOB */
    "the domain is not running any job",
    /* SCDOM_NO_SUCH_JOB */
    "no such job",
    /* SCDOM_TOO_MANY_JOBS */
    "existing jobs occupy too many CPUs",
    /* SCDOM_UNFREEZABLE */
    "unfreezable domain, it is not a running priority domain",
    /* SCDOM_NOT_BOOTING */
    "domain is not booting; use /sbin/shutdown to shut it down",
    /* SCDOM_FROZEN_EXISTS */
    "frozen domain exists; only preemptible jobs are allowed",
    /* SCDOM_END_OF_STATUS_CODE_LIST */
    NULL
};


/** List of registered domains. */
static struct {
    /** Number of domains in the list. */
    int count;
    /** First item. */
    struct domain *first;
    /** Last item. */
    struct domain *last;
} list = { 0, NULL, NULL };


time_t domains_last_save = 0;

static char *defrost_address = NULL;
static char *host_fqdn = NULL;

static int changed = 0;


/** Handle domain which no longer exists.
 *
 * @param[inout] dom
 *      domain.
 *
 * @return
 *      nonzero iff domain's state was changed by this function.
 */
static int domains_vanished(struct domain *dom);


/* Get externally visible domain status for, for example, status cache.
 *
 * @param[in] dom
 *      domain.
 *
 * @param[in] booting
 *      value returned by domains_booting().
 *
 * @return
 *      value from either enum domain_status or enum domain_status_ext, which
 *      can be used as an index 
 */
static int domains_status_get_external(struct domain *dom, int booting);


const char *domains_status_code_string(int status)
{/*{{{*/
    int code = status & STATUS_CODE_MASK;

    if ((status & ~(STATUS_CODE_MASK | STATUS_TYPE_MASK)) == SMAGIC_DOMAINS
        && code < SCDOM_END_OF_STATUS_CODE_LIST)
        return sc_str[code];
    else
        return "unknown status code";
}/*}}}*/


const char *domains_status_string(enum domain_status status)
{/*{{{*/
    return status_str[status];
}/*}}}*/


char *domains_status_full(struct domain *dom, int booting)
{/*{{{*/
    /*               ceil(log2(max_time)[bits] / log2(10)) */
    int time_len = (int) (sizeof(time_t) * 8   / 3.3 + 1 );
    int status;
    int len;
    char *str;

    status = domains_status_get_external(dom, booting);

    /* status */
    len = strlen(status_str[status]);

    /* [ ":" allocated_CPUs ":" available_CPUs ] */
    if (cpus_enabled())
        len += 1 + time_len + 1 + time_len;

    /* ";" preempted_time */
    len += 1 + time_len;

    /* [ ";defrost=" defrost_IP_port ] */
    if (status == FROZEN)
        len += 1 + strlen("defrost=") + strlen(defrost_address);

    /* ";changed=" last_change_timestamp */
    len += 1 + strlen("changed=") + time_len;

    /* NULL-terminated */
    len++;

    if ((str = malloc(len)) == NULL)
        return NULL;

    /* status */
    sprintf(str, "%s", status_str[status]);

    /* [ ":" allocated_CPUs ":" available_CPUs ] */
    if (cpus_enabled()) {
        enum cpus_domain_type dt;

        if (dom->can_preempt)
            dt = DOMAIN_PRIORITY;
        else
            dt = DOMAIN_NORMAL;

        sprintf(str + strlen(str), ":%d:%d",
                dom->cpus, cpus_available(JOB_ANY, dt, 0));
    }

    /* ";" preempted_time */
    sprintf(str + strlen(str), ";%ld", dom->preempted_time);

    /* [ ";defrost=" defrost_IP_port ] */
    if (status == FROZEN) {
        assert(defrost_address != NULL);
        sprintf(str + strlen(str), ";defrost=%s", defrost_address);
    }

    /* ";changed=" last_change_timestamp */
    sprintf(str + strlen(str), ";changed=%ld", dom->status.changed);

    return str;
}/*}}}*/


enum domain_status domains_status_decode(const char *status)
{/*{{{*/
    int i;

    for (i = DOMAINS_STATUS_INTERNAL + 1; i < DOMAIN_STATUS_LAST; i++) {
        if (strcmp(status, status_str[i]) == 0)
            return (enum domain_status) i;
    }

    return UNDEFINED;
}/*}}}*/


void domains_status_set(struct domain *dom, enum domain_status status)
{/*{{{*/
    struct domain *tmp;

    assert(status >= UNDEFINED
           && status != DOMAINS_STATUS_INTERNAL
           && status < DOMAIN_STATUS_LAST);

    if (dom->status.s != status) {
        dom->status.s = status;
        dom->status.changed = time(NULL);
    }

    if (((tmp = domains_find_status(DOWN)) != NULL)
        || ((tmp = domains_find_status(DOWN)) != NULL))
        domains_check_bootables();
}/*}}}*/


int domains_create(const char *fqdn,
                   const char *name,
                   int priority,
                   net_addr_t ip)
{/*{{{*/
    struct domain *dom;

    if (domains_find_fqdn(fqdn) != NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_ALREADY_EXISTS;

    dom = (struct domain *) calloc(1, sizeof(struct domain));
    if (dom == NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;

    domains_status_set(dom, DOWN);
    dom->can_preempt = priority;
    dom->ip = ip;
    dom->port = DEFAULT_PORT;
    dom->fqdn = strdup(fqdn);
    dom->name = strdup(name);
    if (dom->fqdn == NULL || dom->name == NULL) {
        free(dom->fqdn);
        free(dom->name);
        free(dom);
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;
    }

    domains_add(dom);

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


int domains_boot(struct domain *dom)
{/*{{{*/
    struct domain *dom_preemptible;

    assert(dom != NULL);

    if (domains_find_status(RUNNING) != NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_NOT_ENOUGH_RESOURCES;

    domains_status_set(dom, BOOTING);

    while ((dom_preemptible = domains_find_preemptible(0)) != NULL) {
        domains_status_set(dom_preemptible, PREEMPTED);
        domains_preemption_start(dom_preemptible);
        log_msg(LOG_INFO, 0, "Domain %s preempted by booting %s",
                dom_preemptible->fqdn, dom->fqdn);
        if (cpus_enabled())
            cpus_preempt(dom_preemptible->cpus);
    }

    log_msg(LOG_INFO, 0,
            "Booting domain %s; refusing jobs until it is up", dom->fqdn);

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_CHANGED_DOMAINS_AND_CPUS;
}/*}}}*/


int domains_reboot(struct domain *dom)
{/*{{{*/
    struct domain *dom_running;
    struct domain *dom_preemptible;
    struct status old;

    old = dom->status;
    domains_status_set(dom, UNDEFINED);
    dom_running = domains_find_status(RUNNING);
    dom->status = old;

    if (dom_running != NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_NOT_ENOUGH_RESOURCES;

    while ((dom_preemptible = domains_find_preemptible(0)) != NULL) {
        domains_status_set(dom_preemptible, PREEMPTED);
        domains_preemption_start(dom_preemptible);
        log_msg(LOG_INFO, 0, "Domain %s preempted by rebooting %s",
                dom_preemptible->fqdn, dom->fqdn);

        if (cpus_enabled())
            cpus_preempt(dom_preemptible->cpus);
    }

    if (dom->running_jobs) {
        log_msg(LOG_INFO, 0,
                "Removing all jobs from %s; domain is going down for reboot",
                dom->fqdn);
        domains_jobs_stop(dom, NULL);
    }

    domains_status_set(dom, BOOTING);
    dom->rebooting = 1;

    log_msg(LOG_INFO, 0,
            "Rebooting domain %s; refusing jobs until it is up", dom->fqdn);

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_CHANGED_DOMAINS_AND_CPUS;
}/*}}}*/


int domains_down(struct domain *dom)
{/*{{{*/
    assert(dom != NULL);

    /*Do we need a domain to be marked as booting to shut it down ?
     *
     * if (domains_status_get(dom) != BOOTING)                        
     *     return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_NOT_BOOTING;
     */
    domains_status_set(dom, DOWN);

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_CHANGED_DOMAINS_AND_CPUS;
}/*}}}*/


int domains_priority(struct domain *dom, int priority)
{/*{{{*/
    if (dom->can_preempt == priority)
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;

    if (dom->running_jobs)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_RUNNING_JOBS;

    dom->can_preempt = priority;

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


int domains_cluster(struct domain *dom, const char *cluster)
{/*{{{*/
    if (dom->cluster != NULL && cluster != NULL
        && strcmp(cluster, dom->cluster) == 0) {
        log_msg(LOG_DEBUG, 0, "Domain %s already belongs to cluster ``%s''",
                dom->fqdn, cluster);
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
    }

    if (dom->cluster != NULL) {
        log_msg(LOG_INFO, 0, "Removing domain %s from cluster ``%s''",
                dom->fqdn, dom->cluster);
        free(dom->cluster);
        dom->cluster = NULL;
    }

    if (cluster != NULL) {
        log_msg(LOG_INFO, 0, "Adding domain %s to cluster ``%s''",
                dom->fqdn, cluster);
        dom->cluster = strdup(cluster);
        if (dom->cluster == NULL)
            return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;
    }

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


int domains_register(const char *fqdn,
                     vmid_t domid,
                     int port)
{/*{{{*/
    struct domain *dom;
    int sc = SCDOM_OK;

    if ((dom = domains_find_fqdn(fqdn)) == NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_UNKNOWN;

    switch (domains_status_get(dom)) {
    case FROZEN:
        log_msg(LOG_WARNING, 0, "Registering frozen domain");
        break;

    case BOOTING:
    case DOWN:
    case DOWN_BOOTABLE:
        log_msg(LOG_INFO, 0, "Domain %s is up", fqdn);
        domains_status_set(dom, UP);
        sc = SCDOM_CHANGED_DOMAINS_AND_CPUS;
        break;

    default:
        break;
    }

    dom->id = domid;
    dom->port = port;
    dom->rebooting = 0;

    log_msg(LOG_INFO, 0, "Domain %s with ID %lu reachable at %s, port %d",
            fqdn, domid, net_addr_str(dom->ip), port);

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


int domains_remove(struct domain *dom)
{/*{{{*/
    if (dom == NULL)
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;

    if ((domains_status_get(dom) != DOWN)
        || (domains_status_get(dom) != DOWN_BOOTABLE))
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_REMOVE_RUNNING;

    domains_status_set(dom, REMOVED);
    domains_preemption_update();
    reporter_change(M_MAGRATHEA, dom->fqdn, domains_status_full(dom, 0), 0);
    reporter_change(M_CLUSTER, dom->fqdn, NULL, 0);
    reporter_change(M_HOST, dom->fqdn, NULL, 0);

    if (dom->prev != NULL)
        dom->prev->next = dom->next;
    else
        list.first = dom->next;

    if (dom->next != NULL)
        dom->next->prev = dom->prev;
    else
        list.last = dom->prev;

    list.count--;

    domains_free(dom);

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


struct domain *domains_first(void)
{/*{{{*/
    return list.first;
}/*}}}*/


int domains_count(void)
{/*{{{*/
    return list.count;
}/*}}}*/


void domains_add(struct domain *dom)
{/*{{{*/
    if (list.first == NULL)
        list.first = dom;
    if (list.last != NULL)
        list.last->next = dom;
    dom->prev = list.last;
    dom->next = NULL;
    list.last = dom;
    list.count++;
}/*}}}*/


void domains_free(struct domain *dom)
{/*{{{*/
    int i;

    for (i = 0; i < dom->running_jobs; i++)
        free(dom->jobs[i]);

    free(dom->cluster);
    free(dom->fqdn);
    free(dom->name);
    free(dom->jobs);
    free(dom);
}/*}}}*/


void domains_check(void *vmm_state)
{/*{{{*/
    struct domain *dom;
    struct domain *next;

    dom = list.first;
    while (dom != NULL) {
        next = dom->next;

        if (!vmm_vm_exists(dom->id, dom->fqdn, vmm_state))
            changed |= domains_vanished(dom);

        dom = next;
    }
}/*}}}*/


int domains_freeze(struct domain *dom)
{/*{{{*/
    int sc = SCDOM_OK;

    if (!dom->can_preempt || domains_status_get(dom) != RUNNING)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_UNFREEZABLE;

    log_msg(LOG_INFO, 0, "Freezing domain %s", dom->fqdn);

    domains_status_set(dom, FROZEN);

    if (cpus_enabled()) {
        cpus_freeze(dom->cpus);
        sc = SCDOM_CHANGED_CPUS;
    }

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


int domains_defrost(struct domain *dom)
{/*{{{*/
    int changed_dom = 0;
    int changed_cpu = cpus_enabled();
    int sc;

    log_msg(LOG_INFO, 0, "Defrosting domain %s", dom->fqdn);

    if (!cpus_enabled()
        || cpus_available(JOB_ANY, DOMAIN_FROZEN_WO_PREEMPTION, 0)
                < dom->cpus) {
        struct domain *dom_preempt;
        int req_cpus;

        changed_dom = 1;
        req_cpus = dom->cpus
                   - cpus_available(JOB_ANY, DOMAIN_FROZEN_WO_PREEMPTION, 0);

        while ((dom_preempt = domains_find_preemptible(req_cpus)) != NULL) {
            domains_status_set(dom_preempt, PREEMPTED);
            domains_preemption_start(dom_preempt);
            log_msg(LOG_INFO, 0, "Domain %s preempted by defrosting %s",
                    dom_preempt->fqdn, dom->fqdn);

            if (cpus_enabled()) {
                cpus_preempt_defrosting(dom_preempt->cpus);
                if ((req_cpus -= dom_preempt->cpus) <= 0)
                    break;
            }
        }
    }

    domains_status_set(dom, RUNNING);

    if (cpus_enabled())
        cpus_defrost(dom->cpus);

    if (changed_dom && changed_cpu)
        sc = SCDOM_CHANGED_DOMAINS_AND_CPUS;
    else if (changed_dom)
        sc = SCDOM_CHANGED_DOMAINS;
    else if (changed_cpu)
        sc = SCDOM_CHANGED_CPUS;
    else
        sc = SCDOM_OK;

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


void domains_defrost_address(char *address)
{/*{{{*/
    if (defrost_address != NULL)
        free(defrost_address);

    defrost_address = address;

    if (defrost_address != NULL)
        log_msg(LOG_INFO, 0, "Will accept defrost requests at %s", address);
}/*}}}*/


void domains_fqdn(char *fqdn)
{/*{{{*/
    if (host_fqdn != NULL)
        free(host_fqdn);

    host_fqdn = fqdn;

    if (host_fqdn != NULL)
        log_msg(LOG_INFO, 0, "Running on %s", host_fqdn);
}/*}}}*/


struct domain *domains_find_domain(vmid_t id)
{/*{{{*/
    struct domain *dom;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (dom->id == id)
            return dom;
    }

    return NULL;
}/*}}}*/


struct domain *domains_find_status(enum domain_status status)
{/*{{{*/
    struct domain *dom;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (domains_status_get(dom) == status)
            return dom;
    }

    return NULL;
}/*}}}*/


struct domain *domains_find_fqdn(const char *fqdn)
{/*{{{*/
    struct domain *dom;

    if (fqdn == NULL)
        return NULL;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (dom->fqdn != NULL && strcmp(dom->fqdn, fqdn) == 0)
            return dom;
    }

    return NULL;
}/*}}}*/


struct domain *domains_find_ip(net_addr_t ip)
{/*{{{*/
    struct domain *dom;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (net_addr_equal(dom->ip, ip))
            return dom;
    }

    return NULL;
}/*}}}*/


struct domain *domains_find_preemptible(int cpus)
{/*{{{*/
    struct domain *dom;
    struct domain *winner = NULL;

    if (cpus_enabled()) {
        for (dom = list.first; dom != NULL; dom = dom->next) {
            if (domains_status_get(dom) == RUNNING_PREEMPTIBLE
                && (winner == NULL
                    || (dom->cpus >= cpus && dom->cpus < winner->cpus)
                    || (winner->cpus < cpus && dom->cpus > winner->cpus)))
                winner = dom;
        }
    }
    else {
        dom = list.first;
        while (dom != NULL && domains_status_get(dom) != RUNNING_PREEMPTIBLE)
            dom = dom->next;
        winner = dom;
    }

    return winner;
}/*}}}*/


struct domain *domains_find_preempted(int cpus)
{/*{{{*/
    struct domain *dom;
    struct domain *winner = NULL;

    if (cpus_enabled() && cpus == 0)
        return NULL;
    else if (cpus_enabled() && cpus > 0) {
        for (dom = list.first; dom != NULL; dom = dom->next) {
            if (domains_status_get(dom) == PREEMPTED
                && dom->cpus <= cpus
                && (winner == NULL || dom->cpus > winner->cpus))
                winner = dom;
        }
    }
    else {
        dom = list.first;
        while (dom != NULL && domains_status_get(dom) != PREEMPTED)
            dom = dom->next;
        winner = dom;
    }

    return winner;
}/*}}}*/


int domains_jobs_start(struct domain *dom,
                       const char *jobid,
                       int cpus,
                       int preemptible,
                       int cluster)
{/*{{{*/
    int i;
    void *jobs;
    struct job *job;
    enum cpus_domain_type dt;
    enum cpus_job_type jt;
    int changed_cpu = 0;
    int changed_dom = 0;
    int nmp;
    int sc;

    if (cluster && dom->running_jobs)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_RUNNING_JOBS;

    for (i = 0; i < dom->running_jobs; i++) {
        if (strcmp(dom->jobs[i]->id, jobid) == 0)
            return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_JOB_EXISTS;
    }

    if (domains_status_get(dom) == OCCUPIED
        || domains_status_get(dom) == PREEMPTED)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OCCUPIED;

    if (dom->can_preempt)
        dt = DOMAIN_PRIORITY;
    else
        dt = DOMAIN_NORMAL;

    if (!dom->can_preempt
        && preemptible
        && !cluster
        && (domains_status_get(dom) == FREE
            || domains_status_get(dom) == RUNNING_PREEMPTIBLE))
        jt = JOB_PREEMPTIBLE;
    else
        jt = JOB_NORMAL;

    if (dom->can_preempt)
        preemptible = 0;

    if (!preemptible && domains_status_get(dom) == RUNNING_PREEMPTIBLE)
        nmp = dom->cpus;
    else
        nmp = 0;

    if (cpus_enabled() && cpus_available(jt, dt, nmp) < cpus)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_NOT_ENOUGH_CPUS;

    if (!cpus_enabled()
        && jt != JOB_PREEMPTIBLE
        && domains_find_status(FROZEN) != NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_FROZEN_EXISTS;

    jobs = realloc(dom->jobs, sizeof(struct job *) * (dom->running_jobs + 1));
    if (jobs == NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;

    dom->jobs = (struct job **) jobs;

    job = (struct job *) malloc(sizeof(struct job) + strlen(jobid) + 1);
    if (job == NULL)
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;

    if (cluster) {
        dom->cluster_name = malloc(sizeof(char) * strlen(jobid));
        strcpy(dom->cluster_name, jobid);
    }
    else
        dom->cluster_name = NULL;

    job->preemptible = preemptible;
    job->preempted_start = 0;
    job->preempted_time = 0;
    job->cluster = cluster;
    job->cpus = cpus;
    strcpy(job->id, jobid);
    dom->jobs[dom->running_jobs] = job;

    if (cpus_enabled())
        changed_cpu = 1;

    if (dom->can_preempt
        && ((!cpus_enabled() && dom->running_jobs == 0)
            || (cpus_enabled()
                && cpus_available(JOB_ANY,
                                  DOMAIN_PRIORITY_WO_PREEMPTION, 0) < cpus))) {
        struct domain *dom_preemptible;
        int req_cpus;

        req_cpus = cpus - cpus_available(JOB_ANY,
                                         DOMAIN_PRIORITY_WO_PREEMPTION, 0);
        changed_dom = 1;

        while ((dom_preemptible = domains_find_preemptible(req_cpus)) != NULL) {
            domains_status_set(dom_preemptible, PREEMPTED);
            domains_preemption_start(dom_preemptible);
            log_msg(LOG_INFO, 0, "Domain %s preempted by %s",
                    dom_preemptible->fqdn, dom->fqdn);

            if (cpus_enabled()) {
                cpus_preempt(dom_preemptible->cpus);
                if ((req_cpus -= dom_preemptible->cpus) <= 0)
                    break;
            }
        }
    }

    if (domains_status_get(dom) == RUNNING)
        preemptible = 0;

    if (cpus_enabled()) {
        cpus_allocate(cpus, jt, dt);
        if (!preemptible && domains_status_get(dom) == RUNNING_PREEMPTIBLE)
            cpus_change_preemptivness(dom->cpus, 0);
        dom->cpus += cpus;
    }

    if (preemptible)
        domains_status_set(dom, RUNNING_PREEMPTIBLE);
    else if (cluster)
        domains_status_set(dom, RUNNING_CLUSTER);
    else
        domains_status_set(dom, RUNNING);

    if (dom->running_jobs == 0)
        changed_dom = 1;

    dom->running_jobs++;

    log_msg(LOG_INFO, 0,
            "Job started: id=%s, cpus=%d, domain=%s, preemptible=%s",
            jobid, cpus, dom->fqdn, (preemptible) ? "yes" : "no");

    if (changed_dom && changed_cpu)
        sc = SCDOM_CHANGED_DOMAINS_AND_CPUS;
    else if (changed_dom)
        sc = SCDOM_CHANGED_DOMAINS;
    else if (changed_cpu)
        sc = SCDOM_CHANGED_CPUS;
    else
        sc = SCDOM_OK;

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


int domains_jobs_stop(struct domain *dom, const char *jobid)
{/*{{{*/
    int changed_cpu = 0;
    int preemptible = 1;
    int cpus = 0;
    int i;
    int sc;

    if (!dom->running_jobs)
        return SMAGIC_DOMAINS | STATUS_WARN | SCDOM_NO_RUNNING_JOB;

    if (jobid == NULL) {
        for (i = 0; i < dom->running_jobs; i++) {
            log_msg(LOG_INFO, 0, "Job stopped: id=%s, cpus=%d, domain=%s",
                    dom->jobs[i]->id, dom->jobs[i]->cpus, dom->fqdn);
            free(dom->jobs[i]);
        }

        dom->running_jobs = 0;
        free(dom->jobs);
        dom->jobs = NULL;

        if (cpus_enabled()) {
            cpus = dom->cpus;
            dom->cpus = 0;
        }
    }
    else {
        void *jobs;
        int job = -1;

        for (i = 0; i < dom->running_jobs; i++) {
            if (strcmp(dom->jobs[i]->id, jobid) == 0)
                job = i;
            else
                preemptible &= dom->jobs[i]->preemptible;
        }

        if (job == -1)
            return SMAGIC_DOMAINS | STATUS_WARN | SCDOM_NO_SUCH_JOB;

        log_msg(LOG_INFO, 0, "Job stopped: id=%s, cpus=%d, domain=%s",
                dom->jobs[job]->id, dom->jobs[job]->cpus, dom->fqdn);

        if (cpus_enabled()) {
            cpus = dom->jobs[job]->cpus;
            dom->cpus -= cpus;
        }

        free(dom->jobs[job]);
        dom->jobs[job] = dom->jobs[dom->running_jobs - 1];

        dom->running_jobs--;
        jobs = realloc(dom->jobs, sizeof(struct job *) * dom->running_jobs);
        if (dom->running_jobs == 0 || jobs != NULL)
            dom->jobs = (struct job **) jobs;
    }

    if (cpus_enabled()) {
        enum cpus_job_type jt;
        enum cpus_domain_type dt;

        changed_cpu = 1;

        if (domains_status_get(dom) == PREEMPTED)
            dt = DOMAIN_PREEMPTED;
        else if (dom->can_preempt)
            dt = DOMAIN_PRIORITY;
        else
            dt = DOMAIN_NORMAL;

        if (domains_status_get(dom) == RUNNING_PREEMPTIBLE)
            jt = JOB_PREEMPTIBLE;
        else
            jt = JOB_NORMAL;

        cpus_free(cpus, jt, dt);
        if (preemptible && domains_status_get(dom) == RUNNING)
            cpus_change_preemptivness(dom->cpus, 1);
    }

    if (dom->running_jobs == 0) {
        /* domains_state_update() will set the status to a correct value */
        domains_status_set(dom, UNDEFINED);
    }
    else if (preemptible && domains_status_get(dom) == RUNNING)
        domains_status_set(dom, RUNNING_PREEMPTIBLE);
    /* by stopping a job, the domain cannot become nonpreemptible */

    if (changed_cpu)
        sc = SCDOM_CHANGED_CPUS;
    else
        sc = SCDOM_OK;

    dom->cluster_name = NULL;

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


long domains_jobs_preempted_time(struct domain *dom,
                                 const char *jobid,
                                 int cpus)
{/*{{{*/
    time_t now = time(NULL);
    long total = 0;
    int i;

    for (i = 0; i < dom->running_jobs; i++) {
        if (jobid == NULL
            || strcmp(dom->jobs[i]->id, jobid) == 0) {
            int t;

            t = dom->jobs[i]->preempted_time;
            if (dom->jobs[i]->preempted_start > 0)
                t += now - dom->jobs[i]->preempted_start;

            total += t * (cpus ? dom->jobs[i]->cpus : 1);
        }
    }

    return total;
}/*}}}*/


void domains_preemption_start(struct domain *dom)
{/*{{{*/
    int i;
    long now = time(NULL);

    for (i = 0; i < dom->running_jobs; i++) {
        if (dom->jobs[i] != NULL)
            dom->jobs[i]->preempted_start = now;
    }
}/*}}}*/


void domains_preemption_stop(struct domain *dom)
{/*{{{*/
    int i;
    long now = time(NULL);
    long t;

    for (i = 0; i < dom->running_jobs; i++) {
        if (dom->jobs[i] != NULL
            && (t = dom->jobs[i]->preempted_start) > 0) {
            dom->jobs[i]->preempted_time += now - t;
            dom->jobs[i]->preempted_start = 0;
        }
    }
}/*}}}*/


void domains_preemption_update(void)
{/*{{{*/
    struct domain *dom;
    unsigned long total = 0;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (!dom->can_preempt) {
            dom->preempted_time = domains_jobs_preempted_time(dom, NULL, 1);
            total += dom->preempted_time;
        }
    }

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (dom->can_preempt)
            dom->preempted_time = total;
    }
}/*}}}*/


int domains_optimize(void)
{/*{{{*/
    int changed_dom = 0;
    int resume = 1;
    int sc;

    if (domains_booting())
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;

    if (cpus_enabled())
        cpus_stats("CPUs");
    else {
        struct domain *dom;

        for (dom = list.first; dom != NULL; dom = dom->next) {
            if (domains_status_get(dom) == RUNNING && dom->can_preempt)
                resume = 0;
        }
    }

    if (resume || cpus_enabled()) {
        struct domain *dom;

        while ((dom = domains_find_preempted(cpus_available(
                            JOB_ANY, DOMAIN_PREEMPTED, 0))) != NULL) {
            changed_dom = 1;

            domains_status_set(dom, RUNNING_PREEMPTIBLE);
            domains_preemption_stop(dom);
            if (!cpus_enabled()) {
                log_msg(LOG_INFO, 0,
                        "Domain %s returned from preempted state", dom->fqdn);
            }
            else {
                log_msg(LOG_INFO, 0,
                        "Domain %s (%d CPUs) returned from preempted state",
                        dom->fqdn, dom->cpus);

                cpus_resume(dom->cpus);
            }
        }
    }

    if (cpus_enabled()) {
        if (domains_find_preempted(-1) == NULL)
            cpus_set_preempted(0);
        else
            cpus_set_preempted(1);

        cpus_stats("Balanced CPUs");
        cpus_check();
    }

    if (changed_dom)
        sc = SCDOM_CHANGED_DOMAINS;
    else
        sc = SCDOM_OK;

    return SMAGIC_DOMAINS | STATUS_OK | sc;
}/*}}}*/


int domains_reset_counters(void)
{/*{{{*/
    struct domain *dom;
    int preemptible = 0;
    int priority = 0;
    int normal = 0;
    int preempted = 0;
    int frozen = 0;

    if (!cpus_enabled())
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;

    for (dom = list.first; dom != NULL; dom = dom->next) {
        switch (domains_status_get(dom)) {
        case RUNNING_PREEMPTIBLE:
            preemptible += dom->cpus;
            break;

        case RUNNING:
        case RUNNING_CLUSTER:
            if (dom->can_preempt)
                priority += dom->cpus;
            else
                normal += dom->cpus;
            break;

        case PREEMPTED:
            preempted += dom->cpus;
            break;

        case FROZEN:
            frozen += dom->cpus;
            break;

        case UNDEFINED:
        case UP:
        case DOMAINS_STATUS_INTERNAL:
        case DOMAIN_STATUS_LAST:
            /* internal states/separators */
        case REMOVED:
        case DOWN:
        case DOWN_BOOTABLE:
        case BOOTING:
        case FREE:
        case OCCUPIED:
        case OCCUPIED_WOULD_PREEMPT:
            break;
        }
    }

    log_msg(LOG_INFO, 0,
            "CPUs occupied by jobs: total running %d, preemptible %d, "
            "priority %d, normal %d, preempted %d, frozen %d",
            preemptible + priority + normal, preemptible,
            priority, normal, preempted, frozen);

    if (cpus_reset(normal, preemptible, preempted, priority, frozen))
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_TOO_MANY_JOBS;

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


int domains_state_update(void)
{/*{{{*/
    struct domain *dom;
    enum domain_status normal;
    enum domain_status priority;
    int changed = 0;

    if (cpus_enabled()) {
        if (cpus_available(JOB_PREEMPTIBLE, DOMAIN_NORMAL, 0) > 0)
            normal = FREE;
        else
            normal = OCCUPIED;

        if (cpus_available(JOB_ANY, DOMAIN_PRIORITY, 0) == 0)
            priority = OCCUPIED;
        else if (cpus_available(JOB_ANY, DOMAIN_PRIORITY, 0)
                 > cpus_count(CPUS_FREE))
            priority = OCCUPIED_WOULD_PREEMPT;
        else
            priority = FREE;
    }
    else {
        int running = 0;
        int running_preemptible = 0;
        int running_priority = 0;
        int frozen = 0;

        for (dom = list.first; dom != NULL; dom = dom->next) {
            switch (domains_status_get(dom)) {
            case RUNNING_PREEMPTIBLE:
                running_preemptible++;
                break;

            case RUNNING:
            case RUNNING_CLUSTER:
                if (dom->can_preempt)
                    running_priority++;
                else
                    running++;
                break;

            case FROZEN:
                frozen++;
                break;

            case UNDEFINED:
            case UP:
            case DOMAINS_STATUS_INTERNAL:
            case DOMAIN_STATUS_LAST:
                /* internal states/separators */
            case REMOVED:
            case DOWN:
            case DOWN_BOOTABLE:
            case BOOTING:
            case FREE:
            case OCCUPIED_WOULD_PREEMPT:
            case OCCUPIED:
            case PREEMPTED:
                continue;
            }
        }

        if (running + running_priority + running_preemptible == 0) {
            normal = FREE;
            priority = FREE;
        }
        else {
            normal = OCCUPIED;
            if (running + running_priority == 0)
                priority = OCCUPIED_WOULD_PREEMPT;
            else
                priority = OCCUPIED;
        }
    }

    for (dom = list.first; dom != NULL; dom = dom->next) {
        switch (domains_status_get(dom)) {
        case DOMAINS_STATUS_INTERNAL:
        case DOMAIN_STATUS_LAST:
            /* internal states/separators */
        case REMOVED:
        case DOWN:
        case DOWN_BOOTABLE:
        case BOOTING:
        case RUNNING_PREEMPTIBLE:
        case RUNNING:
        case RUNNING_CLUSTER:
        case PREEMPTED:
        case FROZEN:
            break;

        case UP:
            changed = 1;
        case UNDEFINED:
        case FREE:
        case OCCUPIED_WOULD_PREEMPT:
        case OCCUPIED:
            domains_status_set(dom, (dom->can_preempt) ? priority : normal);
            break;
        }
    }

    domains_check_bootables();

    if (changed)
        return SMAGIC_DOMAINS | STATUS_OK | SCDOM_CHANGED_DOMAINS;

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


void domains_state_save(void)
{/*{{{*/
    int storage = 1;
    struct domain *dom;
    int sc;
    int booting;

    if (STATUS_IS_ERROR(sc = storage_open(1))) {
        log_msg(LOG_ERR, errno, "Cannot save current state: %s",
                status_decode(sc));
        log_msg(LOG_WARNING, 0, "Changes will only be reported to the cache");
        storage = 0;
    }

    domains_preemption_update();
    booting = domains_booting();

    for (dom = list.first; dom != NULL; dom = dom->next) {
        if (storage
            && STATUS_IS_ERROR(sc = storage_domain_save(dom))) {
            log_msg(LOG_ERR, errno, "Cannot save domain %s: %s",
                    dom->fqdn, status_decode(sc));
            storage_close();
            storage = 0;
        }

        reporter_change(M_MAGRATHEA, dom->fqdn,
                        domains_status_full(dom, booting), 0);
        reporter_change(M_CLUSTER, dom->fqdn, dom->cluster, 1);
        reporter_change(M_HOST, dom->fqdn, host_fqdn, 1);
    }

    storage_close();

    changed = 0;
    domains_last_save = time(NULL);
}/*}}}*/


int domains_state_load(void *vmm_state)
{/*{{{*/
    int sc;
    struct domain *dom;

    if (list.count != 0) 
        return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_LOAD_NOT_EMPTY;

    if (STATUS_IS_ERROR(sc = storage_open(0)))
        return sc;

    while (!storage_eof()) {
        dom = (struct domain *) calloc(1, sizeof(struct domain));
        if (dom == NULL) {
            storage_close();
            return SMAGIC_DOMAINS | STATUS_ERROR | SCDOM_OUT_OF_MEMORY;
        }

        if (!STATUS_IS_OK(sc = storage_domain_load(dom))) {
            if (STATUS_IS_ERROR(sc)) {
                log_msg(LOG_WARNING, 0, "Cannot load domain%s%s: %s",
                        (dom->fqdn != NULL) ? " " : "",
                        (dom->fqdn != NULL) ? dom->fqdn : "",
                        status_decode(sc));
            }
            domains_free(dom);
            continue;
        }

        if (!vmm_vm_exists(dom->id, dom->fqdn, vmm_state))
            domains_vanished(dom);

        log_msg(LOG_INFO, 0, "Domain loaded: %s", dom->fqdn);
        domains_add(dom);
    }

    storage_close();

    return SMAGIC_DOMAINS | STATUS_OK | SCDOM_OK;
}/*}}}*/


int domains_changed(void)
{/*{{{*/
    return changed;
}/*}}}*/


static int domains_vanished(struct domain *dom)
{/*{{{*/
    switch (domains_status_get(dom)) {
    case UNDEFINED:
    case UP:
    case DOMAINS_STATUS_INTERNAL:
    case DOMAIN_STATUS_LAST:
        /* internal states/separators */
    case REMOVED:
    case DOWN:
    case DOWN_BOOTABLE:
        break;

    case BOOTING:
        if (dom->rebooting) {
            log_msg(LOG_INFO, 0,
                    "Domain %s is down; waiting for it to come up again",
                    dom->fqdn);
            dom->rebooting = 0;
        }
        else {
            log_msg(LOG_INFO, 0,
                    "Domain %s is booting; waiting for it to come up",
                    dom->fqdn);
        }
        return 1;

    case FREE:
    case OCCUPIED_WOULD_PREEMPT:
    case OCCUPIED:
    case RUNNING_PREEMPTIBLE:
    case RUNNING:
    case RUNNING_CLUSTER:
    case PREEMPTED:
    case FROZEN:
        log_msg(LOG_INFO, 0, "Domain %s is down", dom->fqdn);
        if (dom->running_jobs)
            domains_jobs_stop(dom, NULL);
        domains_status_set(dom, DOWN);
        return 1;
    }

    return 0;
}/*}}}*/


static int domains_status_get_external(struct domain *dom, int booting)
{/*{{{*/
    int status = domains_status_get(dom);

    if (booting
        && status != BOOTING
        && status != DOWN
        && status != DOWN_BOOTABLE
        && status != REMOVED)
        status = OCCUPIED_BOOTING;
    else if (status == RUNNING && dom->can_preempt)
        status = RUNNING_PRIORITY;
    else if (dom->cluster_name != NULL)
        status = RUNNING_CLUSTER;

    return status;
}/*}}}*/

void domains_check_bootables(void)
{
    struct domain *dom;

    if ((dom = domains_find_status(RUNNING)) == NULL
        && (dom = domains_find_status(RUNNING_CLUSTER)) == NULL
        && (dom = domains_find_status(BOOTING)) == NULL)
        while ((dom = domains_find_status(DOWN)) != NULL)
            domains_status_set(dom, DOWN_BOOTABLE);
    else
        while ((dom = domains_find_status(DOWN_BOOTABLE)) != NULL)
            domains_status_set(dom, DOWN);
}

