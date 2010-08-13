/* $Id: cpus.h,v 1.5 2008/01/31 12:21:25 xdenemar Exp $ */

/** @file
 * Manipulation with CPUs, their counting, allocating, etc.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.5 $ $Date: 2008/01/31 12:21:25 $
 */

#ifndef MASTER_CPUS_H
#define MASTER_CPUS_H

/** Type of domain. */
enum cpus_domain_type {
    /** Normal, preemptible domain. */
    DOMAIN_NORMAL,
    /** Normal domain which has been preempted. */
    DOMAIN_PREEMPTED,
    /** Priority domain, which can preempt others. */
    DOMAIN_PRIORITY,
    /** Priority domain but without taking preemption into account. */
    DOMAIN_PRIORITY_WO_PREEMPTION,
    /** Frozen (suspended) domain. */
    DOMAIN_FROZEN,
    /** Frozen domain but without taking preemption into account.*/
    DOMAIN_FROZEN_WO_PREEMPTION
};


/** Type of job. */
enum cpus_job_type {
    /** Normal, nonpreemptible job. */
    JOB_NORMAL,
    /** Preemptible job. */
    JOB_PREEMPTIBLE,
    /** Any type, that is, type of job does not matter. */
    JOB_ANY
};


/** Type of CPU counter. 
 * See counters for detail description of each type.
 * CPUs immediately available for a domain are in upper case, currently
 * used CPUs are in lower case.
 */
enum cpus_type {
    CPUS_NODE                   = 'N',
    CPUS_FREE                   = 'F',
    CPUS_PREEMPTIBLE            = 'p',
    CPUS_PREEMPTED              = 'P',
    CPUS_RESERVED               = 'R',
    CPUS_RESERVED_USED          = 'r',
    CPUS_OCCUPIED               = 'o'
};

/** Initialize CPU counting subsystem.
 *
 * @param[in] cpus
 *      total number of CPUs available to the master daemon.
 *      Zero disables CPU counting.
 *
 * @return
 *      nothing.
 */
extern void cpus_init(int cpus);


/** Is CPU counting feature enabled?
 *
 * @return
 *      nonzero when CPU counting is enabled.
 */
extern int cpus_enabled(void);


/** Reset CPU counters wrt numbers of jobs of different kind.
 *
 * @param[in] normal
 *      number of CPUs occupied by normal (nonpreemptible) domains.
 *
 * @param[in] preemptible
 *      number of CPUs occupied by preemptible domains.
 *
 * @param[in] preempted
 *      number of CPUs preempted domains would occupy when resumed.
 *
 * @param[in] priority
 *      number of CPUs occupied by priority domains.
 *
 * @param[in] frozen
 *      number of CPUs frozen domains would occupy when defrosted.
 *
 * @return
 *      zero on success, nonzero when jobs occupy more than maximum number
 *      of CPUs.
 */
extern int cpus_reset(int normal,
                      int preemptible,
                      int preempted,
                      int priority,
                      int frozen);


/** Get number of CPUs of a given type.
 *
 * @param[in] type
 *      type of CPUs the caller is interested in.
 *
 * @return
 *      number of CPUs of the given type.
 */
extern int cpus_count(enum cpus_type type);


/** Get number of CPUs available for a given job.
 * Semantics for each type of domain is the following:
 * @arg @c DOMAIN_NORMAL
 *      available CPUs for a job in normal domain, which occupies preemptible
 *      or nonpreemptible CPUs (the @c no_more_preemptible argument must be
 *      used to indicate a change in preemptibility of the domain).
 * @arg @c DOMAIN_PREEMPTED
 *      CPUs for a preempted domain to check whether it can be resumed.
 * @arg @c DOMAIN_PRIORITY
 *      CPUs for a new job in priority domain. Starting a job which needs all
 *      available CPUs may require preemption.
 * @arg @c DOMAIN_PRIORITY_WO_PREEMPTION
 *      CPUs for a new job in priority domain. Even consuming all CPUs returned
 *      will not cause any domain to be preempted (although may use CPUs from
 *      domains which have already been preempted).
 * @arg @c DOMAIN_FROZEN
 *      CPUs for a frozen domain to check whether it can be defrosted. However,
 *      there MUST be enough CPUs available to defrost any frozen domain. Thus,
 *      this check is mainly used for comparison with the following check
 *      (@c DOMAIN_FROZEN_WO_PREEMPTION) to check how many CPUs must be freed
 *      by preemption.
 * @arg @c DOMAIN_FROZEN_WO_PREEMPTION
 *      CPUs for a frozen domain to check whether it can be defrosted without
 *      preempting any domain which has not been preempted yet.
 *
 * @param[in] job
 *      type of the job.
 *
 * @param[in] domain
 *      type of domain.
 *
 * @param[in] no_more_preemptible
 *      number of CPUs which will change from preemptible to nonpreemptible
 *      when enough CPUs will be available for the job. This paramepter is
 *      meaningful only for @c JOB_NORMAL in @c DOMAIN_NORMAL.
 *
 * @return
 *      number of CPUs available for the combination of job and domain.
 */
extern int cpus_available(enum cpus_job_type job,
                          enum cpus_domain_type domain,
                          int no_more_preemptible);


/** Allocate CPUs for a given job in a given domain.
 * No preemption is done by this function. The caller must assure enough CPUs
 * are available to fulfill the request by checking cpus_available() and
 * possibly preempting some domains using cpus_preempt().
 *
 * @param[in] cpus
 *      number of CPUs to allocate.
 *
 * @param[in] job
 *      type of job.
 *
 * @param[in] domain
 *      type of domain.
 *
 * @return
 *      nothing.
 */
extern void cpus_allocate(int cpus,
                          enum cpus_job_type job,
                          enum cpus_domain_type domain);



/** Free CPUs allocated to a given job in a given domain.
 * The caller must assure that the operation is valid.
 *
 * @param[in] cpus
 *      number of CPUs to free.
 *
 * @param[in] job
 *      type of job.
 *
 * @param[in] domain
 *      type of domain.
 *
 * @return
 *      nothing.
 */
extern void cpus_free(int cpus,
                      enum cpus_job_type job,
                      enum cpus_domain_type domain);


/** Take CPUs from a preemptible domain (that is, preempt it).
 * The caller must assure that the operation is valid.
 *
 * @param[in] cpus
 *      number of CPUs the domain occupies.
 *
 * @return
 *      nothing.
 */
extern void cpus_preempt(int cpus);


/** Preempt a preemptible domain so that a frozen domain can be defrosted.
 * The caller must assure that the operation is valid.
 *
 * @param[in] cpus
 *      number of CPUs the domain occupies.
 *
 * @return
 *      nothing.
 */
extern void cpus_preempt_defrosting(int cpus);


/** Return CPUs to preempted domain (that is, resume the domain).
 * The caller must assure that enough CPUs is available using
 * cpus_available().
 *
 * @param[in] cpus
 *      number of CPUs required to resume the domain.
 *
 * @return
 *      nothing.
 */
extern void cpus_resume(int cpus);


/** Mark CPUs as reserved for a frozen domain.
 * Only preemptible jobs can be started on such CPUs to allow immediate defrost
 * of a frozen domain.
 *
 * @param[in] cpus
 *      number of CPUs to be reserved.
 *
 * @return
 *      nothing.
 */
extern void cpus_freeze(int cpus);


/** Return reserved CPUs back to a frozen domain.
 * The caller must assure that the CPUs are not used by any preemptible job,
 * possibly by performing preemption via cpus_preempt().
 *
 * @param[in] cpus
 *      number of CPUs to be returned back to a frozen domain.
 *
 * @return
 *      nothing.
 */
extern void cpus_defrost(int cpus);


/** Change preemptivness of given CPUs.
 * The caller must assure that the operation is valid.
 *
 * @param[in] cpus
 *      number of affected CPUs.
 *
 * @param[in] preemptible
 *      nonzero when CPUs change from normal to preemptible. Zero if they are
 *      not preemptible any more.
 *
 * @return
 *      nothing.
 */
extern void cpus_change_preemptivness(int cpus, int preemptible);


/** Check counter with respect to preempted domains.
 * The function is intended to balance free and preempted CPUs when there is
 * not enough CPUs to resume any preempted domain.
 *
 * @param[in] yes
 *      nonzero when preempted domains exist. Zero, when no preempted domain
 *      exists.
 *
 * @return
 *      nothing.
 */
extern void cpus_set_preempted(int yes);


/** Log statistics about allocated CPUs.
 *
 * @param[in] header
 *      string to be printed at the beginning of the line with statistics.
 *
 * @return
 *      nothing.
 */
extern void cpus_stats(const char *header);


/** Check counter consistency.
 *
 * @return
 *      nothing.
 */
#ifdef NDEBUG
# define cpus_check()   ({})
#else
extern void cpus_check(void);
#endif

#endif

