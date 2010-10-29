/* $Id: domains.h,v 1.18 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * List of running domains (VMs) and their jobs.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.18 $ $Date: 2009/06/04 11:12:30 $
 */

#ifndef MASTER_DOMAINS_H
#define MASTER_DOMAINS_H

#include <time.h>

#include "../net.h"
#include "vmm.h"


/** Domain status. */
enum domain_status {
    /* internal status used when domain's last job finishes */
    UNDEFINED,
    /* internal status used for a just-booted domain */
    UP,

    /* the following must always be higher than any internal status */
    DOMAINS_STATUS_INTERNAL,

    /* domain was removed, will shortly vanish from the list of domains */
    REMOVED,
    /* domain does not exist but master knows about it */
    DOWN,
    /* domain does not exist but master knows about it and resources for
       booting are available*/
    DOWN_BOOTABLE,
    /* waiting for domain to boot */
    BOOTING,
    /* available for job submission */
    FREE,
    /* free but submitting job would cause preemption of another domain */
    OCCUPIED_WOULD_PREEMPT,
    /* free but blocked by another domain */
    OCCUPIED,
    /* all running jobs can be preempted */
    RUNNING_PREEMPTIBLE,
    /* a job which cannot be preempted is running */
    RUNNING,
    RUNNING_CLUSTER,
    /* the domain was preempted by another one */
    PREEMPTED,
    /* the domain was suspended on user request */
    FROZEN,

    /* the following must always be the highest value in this enum */
    DOMAIN_STATUS_LAST
};

/** Domain status reported outside magrathea (for example to status cache). */
enum domain_status_ext {
    /* ensure all values in this enum are higher than those in
     * enum domain_status */
    DOMAIN_STATUS_EXT = DOMAIN_STATUS_LAST,

    /* another domain is booting */
    OCCUPIED_BOOTING,
    /* a job is running inside a priority domain */
    RUNNING_PRIORITY,
    /*RUNNING_CLUSTER */
};


/** Domain status with timpestamp. */
struct status {
    /** Domain status. */
    enum domain_status s;
    /** Timestamp of the last status change. */
    time_t changed;
};


/** Job description. */
struct job {
    /** Is the job preemptible or not? */
    int preemptible;
    /** Is the job a cluster? */
    int cluster;
    /** The beginning of the last preemption. */
    long preempted_start;
    /** Total number of seconds the job was preempted for. */
    long preempted_time;
    /** Number of CPUs used by the job. */
    int cpus;
    /** Job's unique identifier. */
    char id[];
};


/** Information structure about a registered domain. */
struct domain {
    /** Domain's FQDN. */
    char *fqdn;
    /** Domain ID. */
    vmid_t id;
    /** Domain's static ID. */
    char *name;
    /** Cluster identification.
     * @c NULL means the domain does not belong to any cluster. */
    char *cluster;
    /** IP address where the domain's slave can be contacted. */
    net_addr_t ip;
    /** Port number where the domain's slave can be contacted. */
    int port;
    /** Domain status and timestamp of the most recent status change. */
    struct status status;
    /** Is the domain being rebooted?
     * Nonzero iff the domain exists and is @c BOOTING, that is it has to be
     * provided with as much memory as possible. */
    int rebooting;
    /** Number of running jobs in the domain. */
    int running_jobs;
    /** List of running jobs. */
    struct job **jobs;
    /** Cluster  name*/
    char *cluster_name;
    /** Number of CPUs consumed by running jobs. */
    int cpus;
    /** Is domain allowed to preempt other running domains? */
    int can_preempt;
    /** Preempted time.
     * For domains which cannot preempt, this is total number of seconds
     * current jobs were preempted for.
     * For domains which can preempt others, this is a sum of preempted_time
     * filed of all preemptible domains. */
    unsigned long preempted_time;

    /** Pointer to the previous domain in the list. */
    struct domain *prev;
    /** Pointer to the next domain in the list. */
    struct domain *next;
};


/** Status codes. */
enum domains_status_codes {
    SCDOM_OK,
    SCDOM_UNKNOWN,
    SCDOM_CHANGED_DOMAINS,
    SCDOM_CHANGED_CPUS,
    SCDOM_CHANGED_DOMAINS_AND_CPUS,
    SCDOM_ALREADY_EXISTS,
    SCDOM_OUT_OF_MEMORY,
    SCDOM_REMOVE_RUNNING,
    SCDOM_LOAD_NOT_EMPTY,
    SCDOM_JOB_EXISTS,
    SCDOM_OCCUPIED,
    SCDOM_NOT_ENOUGH_CPUS,
    SCDOM_NOT_ENOUGH_RESOURCES,
    SCDOM_RUNNING_JOBS,
    SCDOM_NO_RUNNING_JOB,
    SCDOM_NO_SUCH_JOB,
    SCDOM_TOO_MANY_JOBS,
    SCDOM_UNFREEZABLE,
    SCDOM_NOT_BOOTING,
    SCDOM_FROZEN_EXISTS,
    SCDOM_END_OF_STATUS_CODE_LIST
};


/** Timestamp of the last domains_state_save() call. */
extern time_t domains_last_save;


/** Decode status code.
 *
 * @param[in] status
 *      status code.
 *
 * @return
 *      string representation of the status.
 */
extern const char *domains_status_code_string(int status);


/** Get string representation of a domain status.
 *
 * @param[in] status
 *      domain status.
 *
 * @return
 *      string representation of the status.
 */
extern const char *domains_status_string(enum domain_status status);


/** Get full status of a domain.
 * The caller has to call domains_preemption_update() before calling this
 * function.
 *
 * The full status is a string in the following form:
 *
 * <pre>
 *      status_string [ ":" allocated_CPUs ":" available_CPUs ] \
 *      ";" preempted_time                                      \
 *      [ ";defrost=" defrost_IP_port ]                         \
 *      ";changed=" last_change_timestamp
 * </pre>
 *
 * @param[in] dom
 *      domain.
 *
 * @param[in] booting
 *      value returned by domains_booting().
 *
 * @return
 *      full status of the domain. The string is dynamically allocated using
 *      malloc(); the caller is responsible for freeing it.
 */
extern char *domains_status_full(struct domain *dom, int booting);


/** Decode domain status string.
 *
 * @param[in] status
 *      domain status string.
 *
 * @return
 *      domain status.
 */
extern enum domain_status domains_status_decode(const char *status);


/** Get domain status.
 *
 * @param dom
 *      domain.
 *
 * @return
 *      domain status.
 */
static inline enum domain_status domains_status_get(struct domain *dom)
{
    return dom->status.s;
}


/** Set domain status and update last change timestamp if needed.
 *
 * @param dom
 *      domain.
 *
 * @param value
 *      domain status.
 *
 * @return
 *      nothing.
 */
extern void domains_status_set(struct domain *dom, enum domain_status status);


/** Create a domain.
 *
 * @param[in] fqdn
 *      domain's FQDN. The function creates its own copy of it.
 *
 * @param[in] name
 *      domain's name, i.e., its static ID.
 *
 * @param[in] priority
 *      nonzero if the domain is a priority one. It is considered to be a
 *      normal domain if @c priority is zero.
 *
 * @param[in] ip
 *      IP address of the domain.
 *
 * @return
 *      status code.
 */
extern int domains_create(const char *fqdn,
                          const char *name,
                          int priority,
                          net_addr_t ip);


/** Boot a domain.
 *
 * @param[inout] dom
 *      domain.
 *
 * @return
 *      status code.
 */
extern int domains_boot(struct domain *dom);


/** Reboot a domain.
 *
 * @param[inout] dom
 *      domain.
 *
 * @return
 *      status code.
 */
extern int domains_reboot(struct domain *dom);


/** Mark a domain as being down (if it is booting).
 *
 * @param[inout] dom
 *      domain.
 *
 * @return
 *      status code.
 */
extern int domains_down(struct domain *dom);


/** Set domain's priority.
 *
 * @param[inout] dom
 *      domain.
 *
 * @param[in] priority
 *      nonzero when the domain is allowed to run preempt other domains.
 *
 * @return
 *      status code.
 */
extern int domains_priority(struct domain *dom, int priority);


/** Set domain's cluster.
 *
 * @param[inout] dom
 *      domain.
 *
 * @param[in] cluster
 *      identification of the cluster. @c NULL removes the domain from its
 *      current cluster (if any).
 *
 * @return
 *      status code.
 */
extern int domains_cluster(struct domain *dom, const char *cluster);


/** Register a domain.
 *
 * @param[in] fqdn
 *      fully qualified domain name of the domain.
 *
 * @param[in] domid
 *      domain ID.
 *
 * @param[in] port
 *      port number on which domain's magrathea-slave daemon can be contacted.
 *
 * @return
 *      status code.
 */
extern int domains_register(const char *fqdn,
                            vmid_t domid,
                            int port);


/** Remove a registered domain.
 *
 * @param[in] dom
 *      domain to be removed.
 *
 * @return
 *      status code.
 */
extern int domains_remove(struct domain *dom);


/** Get the first domain in the domain list.
 *
 * @return
 *      the first domain in the list or NULL.
 */
extern struct domain *domains_first(void);


/** Get total number of registered domains.
 *
 * @return
 *      number of domains in the list.
 */
extern int domains_count(void);


/** Add domain to the domain list.
 *
 * @param[in] dom
 *      domain to add.
 *
 * @return
 *      nothing.
 */
extern void domains_add(struct domain *dom);


/** Free all memory occupied by domain, its jobs, etc.
 *
 * @param[inout] dom
 *      domain to be freed. The pointer is no longer valid after this function
 *      is finished.
 *
 * @return
 *      nothing.
 */
extern void domains_free(struct domain *dom);


/** Check at the VMM whether all domains still exist.
 *
 * @param[inout] vmm_state
 *      VMM state.
 *
 * @return
 *      nothing.
 */
extern void domains_check(void *vmm_state);


/** Freeze (that is, suspend) a domain.
 *
 * @param[inout] dom
 *      domain to be frozen.
 *
 * @return
 *      status code.
 */
extern int domains_freeze(struct domain *dom);


/** Defrost a frozen domain.
 *
 * @param[inout] dom
 *      domain to be defrosted.
 *
 * @return
 *      status code.
 */
extern int domains_defrost(struct domain *dom);


/** Set defrost address for frozen domains.
 *
 * @param[in] address
 *      IP address and port where defrost requests can be sent.
 *
 * @return
 *      nothing.
 */
extern void domains_defrost_address(char *address);


/** Set host's (Domain-0) FQDN.
 *
 * @param[in] fqdn
 *      host's fully qualified domain name.
 *
 * @return
 *      nothing.
 */
extern void domains_fqdn(char *fqdn);


/** Find a domain of given ID.
 *
 * @param[in] id
 *      domain ID to be found.
 *
 * @return
 *      pointer to the domain or NULL when not found.
 */
extern struct domain *domains_find_domain(vmid_t id);


/** Find a domain using its status.
 *
 * @param[in] status
 *      domain's status.
 *
 * @return
 *      pointer to the domain or NULL when not found.
 */
extern struct domain *domains_find_status(enum domain_status status);


/** Find a domain using its FQDN.
 *
 * @param[in] fqdn
 *      domain's FQDN.
 *
 * @return
 *      pointer to the domain or NULL when not found.
 */
extern struct domain *domains_find_fqdn(const char *fqdn);


/** Find a domain using its IP address.
 *
 * @param[in] ip
 *      domain's IP address.
 *
 * @return
 *      pointer to the domain or NULL when not found.
 */
extern struct domain *domains_find_ip(net_addr_t ip);


/** Find the best domain to be preempted.
 * Subsequent calls return second, third, etc. preemptible domain.
 *
 * @param[in] cpus
 *      number of CPUs which has to be freed by preemption.
 *
 * @return
 *      the best domain to be preempted iff cpu counting is enabled, or the
 *      first preemptible domain iff cpu counting is disabled. The caller has
 *      to mark the domain as PREEMPTED so that subsequent calls of this
 *      function ignore all previously selected domains.
 */
extern struct domain *domains_find_preemptible(int cpus);


/** Find the best preempted domain to be resumed.
 * Subsequent calls return second, third, etc. preempted domain.
 *
 * @param[in] cpus
 *      number of CPUs available for preempted domains. A negative value means
 *      find any preempted domain.
 *
 * @return
 *      the best preempted domain to be resumed iff cpu counting is enabled, or
 *      the first preempted domain iff cpu counting is disabled. The caller has
 *      to mark the domain as RUNNING_PREEMPTIBLE so that subsequent calls of
 *      this function ignore all previously selected domains.
 */
extern struct domain *domains_find_preempted(int cpus);


/** Start a job in a given domain.
 *
 * @param[inout] dom
 *      domain in which the job is being started.
 *
 * @param[in] jobid
 *      ID of the job.
 *
 * @param[in] cpus
 *      number of CPUs the job is about to occupy.
 *
 * @param[in] preemptible
 *      whether the job can be preempted or not.
 *
 * @return
 *      status code.
 */
extern int domains_jobs_start(struct domain *dom,
                              const char *jobid,
                              int cpus,
                              int preemptible,
                              int cluster);


/** Stop a job (or all of them) running in a given domain.
 *
 * @param[inout] dom
 *      domain in which the job is running.
 *
 * @param[in] jobid
 *      ID of the job to be stopped. If it is NULL, all jobs running in the
 *      domain will be stopped.
 *
 * @return
 *      status code.
 */
extern int domains_jobs_stop(struct domain *dom, const char *jobid);


/** Get number of seconds a job has been preempted.
 *
 * @param[in] dom
 *      domain in which the job is running.
 *
 * @param[in] jobid
 *      ID of the job. NULL can be used to indicate total time of
 *      preemption of all jobs of the domain is requested.
 *
 * @param[in] cpus
 *      nonzero when preemption time of each job is expected to be multiplied
 *      by number of CPUs occupied by the job.
 *
 * @return
 *      number of seconds a job or all jobs of a given domain were
 *      preempted by other domains.
 */
extern long domains_jobs_preempted_time(struct domain *dom,
                                        const char *jobid,
                                        int cpus);


/** Start preemption of a specified domain.
 * All jobs within the domain are marked as preempted since now.
 *
 * @param[in] dom
 *      domain to be preempted.
 *
 * @return
 *      nothing.
 */
extern void domains_preemption_start(struct domain *dom);


/** Stop preemption of a specified domain.
 * Time of preemption of all jobs within the domain gets updated and the jobs
 * are no longer marked as preempted.
 *
 * @param[in] dom
 *      domain was preempted.
 *
 * @return
 *      nothing.
 */
extern void domains_preemption_stop(struct domain *dom);


/** Update preempted time of all domains.
 * For normal domains, preempted time is a total number of seconds jobs
 * running inside the domain have been preempted.
 *
 * For domains which are allowed to preempt others, domain's preempted time
 * is a total number of seconds all other domains have been preempted.
 *
 * @return
 *      nothing.
 */
extern void domains_preemption_update(void);


/** Optimize number of running domains.
 * When CPU counting is disabled, just resume preempted domain, otherwise
 * balance free and preempted CPUs to utilize as many as possible of them.
 *
 * @return
 *      status code.
 */
extern int domains_optimize(void);


/** Recompute global counters from current state of jobs and domains.
 *
 * @return
 *      status code.
 */
extern int domains_reset_counters(void);


/** Update states of all domains according to their usage of CPUs.
 *
 * @return
 *      status code.
 */
extern int domains_state_update(void);


/** Save current state of all domains.
 *
 * @return
 *      nothing.
 */
extern void domains_state_save(void);


/** Load state of all domains from a storage file.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @return
 *      status code.
 */
extern int domains_state_load(void *vmm_state);


/** Is there a booting domain?
 *
 * @return
 *      nonzero when a booting domain exists, zero otherwise.
 */
#define domains_booting()   (domains_find_status(BOOTING) != NULL)


/** Has state of any domain been changed since last domains_state_save() call?
 *
 * @return
 *      nonzero iff anything has been changed.
 */
extern int domains_changed(void);

extern void domains_check_bootables(void);


#endif

