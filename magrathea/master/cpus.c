/* $Id: cpus.c,v 1.7 2008/05/21 15:56:16 xdenemar Exp $ */

/** @file
 * Manipulation with CPUs, their counting, allocating, etc.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.7 $ $Date: 2008/05/21 15:56:16 $
 */

#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "../utils.h"
#include "vmm.h"
#include "cpus.h"


/** Returns nonzero iff CPU counting is enabled. */
#define CPU_COUNTING    (counters.node > 0)

/** CPU counters. */
static struct counters {
    /** Total number of CPUs.
     * Zero means CPU counting is disabled. */
    int node;
    /** Free CPUs. */
    int free;
    /** CPUs occupied by preemptible (and not yet preempted) domains. */
    int preemptible;
    /** CPUs from preempted domains but not used by any priority domain. */
    int preempted;
    /** CPUs reserved for frozen domains so that they can be defrosted. */
    int reserved;
    /** Reserved CPUs which are used by preemptible jobs. */
    int reserved_used;
} counters = { 0, 0, 0, 0, 0, 0 };


/** Description and allocation of all CPUs.
 * It is an array of counters.node structs.
 */
struct cpu_description {
    /** Type of the CPU. */
    enum cpus_type type;
    /** @todo VM running on the CPU. */
//    vmid_t vm;
};

/** Array of counters.node CPUs. */
static struct cpu_description *map = NULL;

/** Last CPU map as a string. */
static char *map_string = NULL;


/** Domain type to string conversion. */
static const char *cpus_domain_str[] = {
    "normal",
    "preempted",
    "priority", "priority",
    "frozen", "frozen"
};

/** Job type to string conversion. */
static const char *cpus_job_str[] = {
    "normal",
    "preemptible",
    "normal"
};


/** Log current CPU map.
 *
 * @param[in] register_old
 *      if nonzero, the function will only remember current CPU map for later
 *      call to cpus_map_log(0), which can log change since the old map.
 *
 * @return
 *      nothing.
 */
static void cpus_map_log(int register_old);


/** Change CPU map.
 *
 * @param[in] old
 *      which type of CPUs should be changed.
 *      Use CPUS_NODE to change all CPUs.
 *
 * @param[in] new
 *      what type the CPUs should be changed to.
 *
 * @param[in] count
 *      number of CPUs to change.
 *      Use zero to change all CPUs matching old type.
 *
 * @return
 *      nothing.
 */
static void cpus_map_change(enum cpus_type old, enum cpus_type new, int count);


void cpus_init(int cpus)
{
    int err = 0;

    if (counters.node != 0) {
        log_msg(LOG_ERR, 0, "CPU counting already configured");
        return;
    }

    map = malloc(sizeof(struct cpu_description) * cpus);
    if (map == NULL)
        err = 1;
    else {
        map_string = malloc(cpus + cpus / 4 + 1);
        if (map_string == NULL) {
            free(map);
            err = 1;
        }
    }

    if (err)
        log_msg(LOG_ERR, errno, "Cannot initialize CPU counting");
    else {
        counters.node = cpus;
        cpus_map_change(CPUS_NODE, CPUS_FREE, 0);
        map_string[0] = '\0';
    }

    if (CPU_COUNTING)
        log_msg(LOG_NOTICE, 0, "Configured for %d CPUs", counters.node);
    else
        log_msg(LOG_NOTICE, 0, "CPU counting disabled");
}


int cpus_enabled(void)
{
    return CPU_COUNTING;
}


int cpus_reset(int normal,
               int preemptible,
               int preempted,
               int priority,
               int frozen)
{
    int i;
    int cpu;

    if (preemptible + priority + normal > counters.node
        || frozen + priority + normal > counters.node
        || preemptible + preempted + normal > counters.node)
        return 1;

    cpus_map_log(1);

    counters.preemptible = preemptible;
    counters.free = counters.node - preemptible - priority - normal;

    counters.reserved = frozen;
    if (counters.reserved >= counters.preemptible)
        counters.reserved_used = counters.preemptible;
    else
        counters.reserved_used = counters.reserved;
    counters.free -= counters.reserved - counters.reserved_used;

    if (counters.free > 0) {
        if (preempted >= counters.free)
            counters.preempted = counters.free;
        else
            counters.preempted = preempted;
        counters.free -= counters.preempted;
    }

    assert(cpus_count(CPUS_OCCUPIED) <= counters.node);

    for (i = 0, cpu = 0; i < counters.free; i++, cpu++)
        map[cpu].type = CPUS_FREE;

    for (i = 0; i < counters.preemptible - counters.reserved_used; i++, cpu++)
        map[cpu].type = CPUS_PREEMPTIBLE;

    for (i = 0; i < counters.preempted; i++, cpu++)
        map[cpu].type = CPUS_PREEMPTED;

    for (i = 0; i < counters.reserved - counters.reserved_used; i++, cpu++)
        map[cpu].type = CPUS_RESERVED;

    for (i = 0; i < counters.reserved_used; i++, cpu++)
        map[cpu].type = CPUS_RESERVED_USED;

    for ( ; cpu < counters.node; cpu++)
        map[cpu].type = CPUS_OCCUPIED;

    cpus_map_log(0);

    return 0;
}


int cpus_count(enum cpus_type type)
{
    switch (type) {
    case CPUS_NODE:         return counters.node;
    case CPUS_FREE:         return counters.free;
    case CPUS_PREEMPTIBLE:  return counters.preemptible;
    case CPUS_PREEMPTED:    return counters.preempted;
    case CPUS_RESERVED:     return counters.reserved;
    case CPUS_RESERVED_USED:return counters.reserved_used;
    case CPUS_OCCUPIED:     return counters.node
                                   - counters.free
                                   - counters.preemptible
                                   + counters.reserved_used
                                   - counters.preempted
                                   - counters.reserved;
    }

    /* should be unreachable :-) */
    return 0;
}


int cpus_available(enum cpus_job_type job,
                   enum cpus_domain_type domain,
                   int no_more_preemptible)
{
    switch (domain) {
    case DOMAIN_NORMAL:
        switch (job) {
        case JOB_NORMAL:
        case JOB_ANY:
            if (no_more_preemptible > 0
                && counters.preemptible - no_more_preemptible
                        < counters.reserved_used)
                /* after changing preemptible CPUs to normal ones as a result
                 * of starting nonpreemptible job, reserved_used CPUs would not
                 * be covered by preemptible CPUs
                 * => we do not allow the job to be started */
                return 0;
            else
                return counters.free;
        case JOB_PREEMPTIBLE:
            return counters.free
                   + counters.reserved - counters.reserved_used;
        }

    case DOMAIN_PREEMPTED:
        /* only preemptible job can exist in preempted domain */
        return counters.free
               + counters.preempted
               + counters.reserved - counters.reserved_used;

    case DOMAIN_PRIORITY:
        /* no preemptible job can exist inside priority domain */
        return counters.free
               + counters.preemptible - counters.reserved_used
               + counters.preempted;

    case DOMAIN_PRIORITY_WO_PREEMPTION:
        return counters.free + counters.preempted;

    case DOMAIN_FROZEN:
        /* reserved_used CPUs are already counted as preemptible CPUs */
        return counters.free
               + counters.preemptible
               + counters.reserved - counters.reserved_used
               + counters.preempted;

    case DOMAIN_FROZEN_WO_PREEMPTION:
        return counters.free
               + counters.reserved - counters.reserved_used
               + counters.preempted;
    }

    /* should be unreachable */
    return 0;
}


void cpus_allocate(int cpus,
                   enum cpus_job_type job,
                   enum cpus_domain_type domain)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Allocating %d CPUs to a %s job in a %s domain",
            cpus, cpus_job_str[job], cpus_domain_str[domain]);

    switch (domain) {
    case DOMAIN_NORMAL:
        switch (job) {
        case JOB_NORMAL:
        case JOB_ANY:
            assert(cpus <= counters.free);
            counters.free -= cpus;
            cpus_map_change(CPUS_FREE, CPUS_OCCUPIED, cpus);
            break;
        case JOB_PREEMPTIBLE:
            assert(cpus <= counters.free
                           + counters.reserved - counters.reserved_used);

            counters.preemptible += cpus;
            counters.reserved_used += cpus;
            if (counters.reserved_used > counters.reserved) {
                int take_free = counters.reserved_used - counters.reserved;

                counters.free -= take_free;
                counters.reserved_used = counters.reserved;

                if (cpus > take_free) {
                    cpus_map_change(CPUS_RESERVED, CPUS_RESERVED_USED,
                                    cpus - take_free);
                }
                cpus_map_change(CPUS_FREE, CPUS_PREEMPTIBLE, take_free);
            }
            else
                cpus_map_change(CPUS_RESERVED, CPUS_RESERVED_USED, cpus);
            break;
        }
        break;

    case DOMAIN_PREEMPTED:
        /* no job can be started inside a preempted domain */
        assert(cpus == 0);
        break;

    case DOMAIN_PRIORITY:
    case DOMAIN_PRIORITY_WO_PREEMPTION:
        assert(cpus <= counters.free + counters.preempted);

        counters.preempted -= cpus;
        if (counters.preempted < 0) {
            int take_free = -counters.preempted;

            counters.free -= take_free;
            counters.preempted = 0;

            if (cpus > take_free) {
                cpus_map_change(CPUS_PREEMPTED, CPUS_OCCUPIED,
                                cpus - take_free);
            }
            cpus_map_change(CPUS_FREE, CPUS_OCCUPIED, take_free);
        }
        else
            cpus_map_change(CPUS_PREEMPTED, CPUS_OCCUPIED, cpus);
        break;

    case DOMAIN_FROZEN:
    case DOMAIN_FROZEN_WO_PREEMPTION:
        /* no job can be started inside a frozen domain */
        assert(cpus == 0);
        break;
    }

    cpus_map_log(0);
}


void cpus_free(int cpus,
               enum cpus_job_type job,
               enum cpus_domain_type domain)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Freeing %d CPUs allocated by a %s job in a %s domain",
            cpus, cpus_job_str[job], cpus_domain_str[domain]);

    switch (domain) {
    case DOMAIN_NORMAL:
        switch (job) {
        case JOB_NORMAL:
        case JOB_ANY:
            assert(cpus_count(CPUS_OCCUPIED) >= cpus);
            counters.free += cpus;
            cpus_map_change(CPUS_OCCUPIED, CPUS_FREE, cpus);
            break;
        case JOB_PREEMPTIBLE:
            assert(cpus <= counters.preemptible);

            counters.preemptible -= cpus;
            counters.reserved_used -= cpus;
            if (counters.reserved_used < 0) {
                int make_free = -counters.reserved_used;

                counters.free += make_free;
                counters.reserved_used = 0;

                if (cpus > make_free) {
                    cpus_map_change(CPUS_RESERVED_USED, CPUS_RESERVED,
                                    cpus - make_free);
                }
                cpus_map_change(CPUS_PREEMPTIBLE, CPUS_FREE, make_free);
            }
            else
                cpus_map_change(CPUS_RESERVED_USED, CPUS_RESERVED, cpus);
            break;
        }
        break;

    case DOMAIN_PREEMPTED:
        /* preempted jobs do not occupy any cpu, nothing to be done here */
        break;

    case DOMAIN_PRIORITY:
    case DOMAIN_PRIORITY_WO_PREEMPTION:
        assert(cpus_count(CPUS_OCCUPIED) >= cpus);
        counters.free += cpus;
        cpus_map_change(CPUS_OCCUPIED, CPUS_FREE, cpus);
        break;

    case DOMAIN_FROZEN:
    case DOMAIN_FROZEN_WO_PREEMPTION:
        /* no job can stop in a frozen domain */
        assert(cpus == 0);
        break;
    }

    cpus_map_log(0);
}


void cpus_preempt(int cpus)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Preempting %d CPUs", cpus);

    assert(cpus <= counters.preemptible);

    counters.preemptible -= cpus;
    counters.preempted += cpus;

    if (counters.reserved_used > counters.preemptible) {
        int reserved_freed = counters.reserved_used - counters.preemptible;

        counters.reserved_used -= reserved_freed;
        counters.preempted -= reserved_freed;

        if (cpus > reserved_freed) {
            cpus_map_change(CPUS_PREEMPTIBLE, CPUS_PREEMPTED,
                            cpus - reserved_freed);
        }
        cpus_map_change(CPUS_RESERVED_USED, CPUS_RESERVED, reserved_freed);
    }
    else
        cpus_map_change(CPUS_PREEMPTIBLE, CPUS_PREEMPTED, cpus);

    cpus_map_log(0);
}


void cpus_preempt_defrosting(int cpus)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Preempting %d CPUs by a frozen domain", cpus);

    assert(cpus <= counters.preemptible);

    counters.preemptible -= cpus;
    counters.reserved_used -= cpus;
    if (counters.reserved_used < 0) {
        int make_preempted = -counters.reserved_used;

        counters.preempted += make_preempted;
        counters.reserved_used = 0;

        if (cpus > make_preempted) {
            cpus_map_change(CPUS_RESERVED_USED, CPUS_RESERVED,
                            cpus - make_preempted);
        }
        cpus_map_change(CPUS_PREEMPTIBLE, CPUS_PREEMPTED, make_preempted);
    }
    cpus_map_change(CPUS_RESERVED_USED, CPUS_RESERVED, cpus);

    cpus_map_log(0);
}


void cpus_resume(int cpus)
{
    int use_free = 0;       /* free => preemptible */
    int use_preempted = 0;  /* preempted => preemptible */
    int use_reserved = 0;   /* reserved => reserved_used */

    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0,
            "Giving back %d CPUs to a previously preempted domain", cpus);

    assert(cpus <= counters.free
                 + counters.preempted
                 + counters.reserved - counters.reserved_used);

    counters.preemptible += cpus;

    counters.reserved_used += cpus;
    use_reserved = cpus;

    if (counters.reserved_used > counters.reserved) {
        use_preempted = counters.reserved_used - counters.reserved;
        use_reserved -= use_preempted;
        counters.preempted -= use_preempted;
        counters.reserved_used = counters.reserved;
    }
    if (counters.preempted < 0) {
        use_free = -counters.preempted;
        use_preempted -= use_free;
        counters.free -= use_free;
        counters.preempted = 0;
    }

    if (use_reserved > 0)
        cpus_map_change(CPUS_RESERVED, CPUS_RESERVED_USED, use_reserved);
    if (use_preempted > 0)
        cpus_map_change(CPUS_PREEMPTED, CPUS_PREEMPTIBLE, use_preempted);
    if (use_free > 0)
        cpus_map_change(CPUS_FREE, CPUS_PREEMPTIBLE, use_free);

    cpus_map_log(0);
}


void cpus_freeze(int cpus)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Freezing %d CPUs", cpus);

    assert(cpus <= cpus_count(CPUS_OCCUPIED));

    counters.reserved += cpus;
    cpus_map_change(CPUS_OCCUPIED, CPUS_RESERVED, cpus);

    cpus_map_log(0);
}


void cpus_defrost(int cpus)
{
    cpus_map_log(1);
    log_msg(LOG_DEBUG, 0, "Defrosting %d CPUs", cpus);

    assert(cpus <= counters.reserved);
    assert(cpus <= counters.free
                   + counters.reserved - counters.reserved_used
                   + counters.preempted);

    counters.reserved -= cpus;

    if (counters.reserved >= counters.reserved_used)
        cpus_map_change(CPUS_RESERVED, CPUS_OCCUPIED, cpus);
    else if (counters.reserved < counters.reserved_used) {
        int take_preempted = counters.reserved_used - counters.reserved;

        counters.reserved_used -= take_preempted;
        counters.preempted -= take_preempted;

        cpus_map_change(CPUS_RESERVED, CPUS_OCCUPIED, cpus - take_preempted);
        cpus_map_change(CPUS_RESERVED_USED, CPUS_PREEMPTIBLE, take_preempted);
        cpus_map_change(CPUS_PREEMPTED, CPUS_OCCUPIED, take_preempted);

        if (counters.preempted < 0) {
            int take_free = -counters.preempted;

            counters.preempted = 0;
            counters.free -= take_free;

            cpus_map_change(CPUS_FREE, CPUS_OCCUPIED, take_free);
        }
    }

    cpus_map_log(0);
}


void cpus_change_preemptivness(int cpus, int preemptible)
{
    if (cpus == 0)
        return;

    cpus_map_log(1);

    if (preemptible) {
        log_msg(LOG_DEBUG, 0,
                "Changing %d occupied CPUs to preemptible", cpus);

        assert(cpus_count(CPUS_OCCUPIED) >= cpus);

        counters.preemptible += cpus;
        cpus_map_change(CPUS_OCCUPIED, CPUS_PREEMPTIBLE, cpus);
    }
    else {
        log_msg(LOG_DEBUG, 0,
                "Changing %d preemptible CPUs to occupied", cpus);

        assert(cpus <= counters.preemptible - counters.reserved_used);

        counters.preemptible -= cpus;
        cpus_map_change(CPUS_PREEMPTIBLE, CPUS_OCCUPIED, cpus);
    }

    cpus_map_log(0);
}


void cpus_set_preempted(int yes)
{
    int map_changed = 0;

    cpus_map_log(1);

    if (yes) {
        if (counters.free > 0) {
            log_msg(LOG_DEBUG, 0,
                    "Setting %d free CPUs as preempted", counters.free);
            cpus_map_change(CPUS_FREE, CPUS_PREEMPTED, counters.free);
            map_changed = 1;
        }

        counters.preempted += counters.free;
        counters.free = 0;
    }
    else {
        if (counters.preempted > 0) {
            log_msg(LOG_DEBUG, 0,
                    "Setting %d preempted CPUs as free", counters.preempted);
            cpus_map_change(CPUS_PREEMPTED, CPUS_FREE, counters.preempted);
            map_changed = 1;
        }

        counters.free += counters.preempted;
        counters.preempted = 0;
    }

    if (map_changed)
        cpus_map_log(0);
}


void cpus_stats(const char *header)
{
    log_msg(LOG_DEBUG, 0,
            "%s: total %d, occupied %d, free %d, preempted %d, "
            "preemptible %d, reserved %d, reserved_used %d",
            header,
            counters.node,
            cpus_count(CPUS_OCCUPIED),
            counters.free,
            counters.preempted,
            counters.preemptible,
            counters.reserved,
            counters.reserved_used);
}


#ifndef NDEBUG
void cpus_check(void)
{
    /*assert(free_cpus >= 0);
    assert(preempted_cpus >= 0);
    assert(preemptible_cpus >= 0);
    assert(node_cpus >= free_cpus + preempted_cpus + preemptible_cpus);
    assert(counters.preemptible >= counters.reserved_used);
    assert(counters.reserved >= counters.reserved_used);
    */
}
#endif


static void cpus_map_log(int register_old)
{
    char mapstr[counters.node + counters.node / 4 + 1];
    int i;
    int cpu;

    memset(mapstr, '\0', counters.node + counters.node / 4 + 1);
    i = 0;
    for (cpu = 0; cpu < counters.node; cpu++) {
        if (cpu > 0 && cpu % 4 == 0)
            mapstr[i++] = ' ';
        mapstr[i++] = (char) map[cpu].type;
    }

    if (register_old)
        strcpy(map_string, mapstr);
    else {
        if (map_string[0] != '\0' && strcmp(map_string, mapstr) != 0)
            log_msg(LOG_DEBUG, 0, "CPU map: %s => %s", map_string, mapstr);
        else
            log_msg(LOG_DEBUG, 0, "CPU map: %s", mapstr);
        map_string[0] = '\0';
    }
}


static void cpus_map_change(enum cpus_type old, enum cpus_type new, int count)
{
    int i;
    int n = 0;

    for (i = 0; i < counters.node; i++) {
        if (old == CPUS_NODE || old == map[i].type) {
            map[i].type = new;
            if (++n == count)
                break;
        }
    }
}

