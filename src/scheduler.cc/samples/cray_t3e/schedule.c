/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/* $Id$ */

/*
 * This is the main loop for the scheduler.  The scheduler receives a
 * command and then calls the routine schedule().  Appropriate actions
 * are taken based on the command specified.  All actions taken by the
 * scheduler must be initiated from here.
 */

#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* PBS header files */
#include "pbs_error.h"
#include "pbs_ifl.h"
#include "log.h"
#include "sched_cmds.h"

/* Scheduler header files */
#include "toolkit.h"
#include "gblxvars.h"
#include "msgs.h"

extern int connector;
extern char *schd_CmdStr[16];

/*
 * How many seconds to wait before attempting to re-request the job and queue
 * information from the server.
 */
#define WAIT_FOR_QUEUE_SANITY 10

Resources schd_Rsrcs;  /* system resources */
time_t  schd_TimeNow;
time_t  schd_TimeLast = 0;

struct tm schd_TmNow;
int     last_run_in_pt = 0;

Job    *schd_AllJobs = NULL;

static int schd_req(int cmd);
static int get_all_queue_info(int numqlists, QueueList *list, ...);
static Job *reject_unrunnables(Job *jobs);
static int schedule_jobs(QueueList *queues, Job *jobs, char *reason);
static int schedule_restart(Job *joblist);
static int make_job_dump(char *dumpfile);
static int dump_sorted_jobs(FILE *file, Job *joblist);
void       fix_jim(Queue *submit, Queue *jim);


/*
 * This routine gets the scheduling command and takes appropriate action.
 * C.f. PBS Administrator's Guide, Section 4.1.
 *
 * Valid commands are:
 *    SCH_ERROR            ERROR.
 *    SCH_SCHEDULE_NULL    Place holder.
 *    SCH_SCHEDULE_NEW     New job arrived or a non-running job changed state.
 *    SCH_SCHEDULE_TERM    A running job terminated.
 *    SCH_SCHEDULE_TIME    The schedulers time interval expired.
 *    SCH_SCHEDULE_RECYC   One job was started in the last scheduling cycle.
 *    SCH_SCHEDULE_CMD     The server attribute "scheduling" was set to true.
 *    SCH_CONFIGURE        Perform any scheduler [re]configuration.
 *    SCH_QUIT             QUIT
 *    SCH_RULESET          Reread the scheduler rules.
 *    SCH_SCHEDULE_FIRST   First schedule run after server starts.
 */
int
schedule(int cmd)
  {
  char   *id = "schedule";
  char   *cmdstr = "Error";

  /*
   * Try to find a text string that describes the command request.
   */

  if (cmd >= 0)
    cmdstr = (schd_CmdStr[cmd] != NULL) ? schd_CmdStr[cmd] : "Unknown";

  (void)sprintf(log_buffer, "Schedule: received cmd %d (%s)", cmd, cmdstr);

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  DBPRT(("--------\n%s\n", log_buffer));

  /*
   * See if a reconfiguration is pending.  If so, do it before anything
   * starts to look at the structures.
   */
  if (schd_sigflags & SCHD_SIGFLAGS_RECONFIG)
    {
    (void)sprintf(log_buffer, "Reconfiguring due to delivery of SIGHUP.");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));

    if (schedinit(0, NULL))
      {
      (void)sprintf(log_buffer,
                    "%s: FATAL ERROR!!!  Configuration failed!!!", id);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));

      exit(1);
      }

    /* Turn off the pending reconfiguration flag. */
    schd_sigflags &= ~SCHD_SIGFLAGS_RECONFIG;
    }

  /*
   * Assume the scheduler is about to use the current configuration.
   * Set the BUSY flag so that reconfiguration will be postponed until
   * the scheduler is idle if a SIGHUP is delivered.
   */
  schd_sigflags |= SCHD_SIGFLAGS_BUSY;

  switch (cmd)
    {
      /*
       * "Null" commands.  These cases are quietly ignored.
       */

    case SCH_SCHEDULE_NULL: /* Placeholder only.                        */

    case SCH_ERROR:  /* Error.                                   */

    case SCH_RULESET:  /* Re-read scheduler rules.                 */
      (void)sprintf(log_buffer, "command %d ignored.", cmd);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s\n", log_buffer));

      break;

      /*
       * These commands cause the scheduler to perform its scheduling cycle.
       */

    case SCH_SCHEDULE_FIRST: /* 1st schedule run after server starts.    */

    case SCH_SCHEDULE_CMD: /* Perform a schedule run now, on command.  */

    case SCH_SCHEDULE_TERM: /* A running job terminated.                */

    case SCH_SCHEDULE_NEW: /* A new job has arrived.                   */

    case SCH_SCHEDULE_TIME: /* Scheduler sleep interval reached.        */

    case SCH_SCHEDULE_RECYC: /* Recycle scheduler after 1 run.           */

      if (!schd_req(cmd))
        {
        schd_cleanup();
        DBPRT(("schd_req(%d) returned 0.  Calling it again.\n", cmd));
        schd_req(cmd);
        }

      schd_cleanup();

      break;

    case SCH_CONFIGURE:  /* Re-initialize the scheduler.             */
      (void)sprintf(log_buffer, "[re]configure scheduler");
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s\n", log_buffer));

      schedinit(0, NULL);  /* XXX args not used... for now! */
      break;

    case SCH_QUIT:  /* Exit gracefully.                         */
      (void)sprintf(log_buffer, "Scheduler was asked to quit.  Exit.");
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s\n", log_buffer));

      exit(0);

    default:
      (void)sprintf(log_buffer, "Schedule command %d unrecognized.", cmd);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s\n", log_buffer));
    }

  /*
   * The scheduling part of the scheduler is now idle.  Unset the BUSY
   * flag.
   */
  schd_sigflags &= ~SCHD_SIGFLAGS_BUSY;

  schd_FirstRun = 0;

  return (0);
  }

/*
 * Service a request to schedule a job.
 */
/* ARGSUSED */
int
schd_req(int cmd)
  {
  char   *id = "schd_req";
  Job    *this, *jobs = NULL;
  QueueList *qptr, *next;
  QueueList *normalQs = NULL, *normalQtail = NULL, *newqlp;
  Outage *outages;
  int     ran, error, total_ran = 0;
  int     hosts_in_dedtime = 0;

  struct tm *tm_ptr;
  char    reason[MAX_TXT + 1];

  /* Save "last" run time (in global 'schd_TimeNow') for later use. */
  schd_TimeLast = schd_TimeNow;

  /*
   * Get the number of seconds since the Epoch, and break it down into
   * the various day, month, year, fields in a struct tm.
   */
  time(&schd_TimeNow);

  if (tm_ptr = localtime(&schd_TimeNow))
    memcpy((void *) & schd_TmNow, (void *)tm_ptr, sizeof(struct tm));
  else
    memset((void *)&schd_TmNow, 0, sizeof(struct tm));

  DBPRT(("[time_t %d] %s", schd_TimeNow, ctime(&schd_TimeNow)));

  /*
   * If the configuration file has been changed since the last time the
   * scheduler was run, than note that in the logs.  Don't re-read it
   * automatically, just note the fact.  Don't reset the timestamp - it
   * will be done when someone finally HUP's the scheduler.
   */
  if (schd_CfgFilename && schd_file_has_changed(schd_CfgFilename, 0))
    {
    (void)sprintf(log_buffer,
                  "WARNING!!!  Scheduler config file %s has changed!",
                  schd_CfgFilename);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    (void)sprintf(log_buffer, "Run 'kill -HUP %ld' to reconfigure.",
                  (long)getpid());
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    }

  /*
   * See if the holidays file has changed.  If it's re-read successfully,
   * update the last changed timestamp.  Otherwise, keep it around and
   * keep trying to re-read it until someone fixes the problem.  "This
   * shouldn't happen."
   */
  if (schd_file_has_changed(HOLIDAYS_FILE, 0) > 0)
    {
    (void)sprintf(log_buffer,
                  "Attempting to update holidays/primetime from %s.", HOLIDAYS_FILE);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s\n", log_buffer));

    if (schd_read_holidays() < 0)
      {
      (void)sprintf(log_buffer, "Failed to read holidays file.");
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s\n", log_buffer));
      }
    else
      {
      /* Reset the "last changed time", since it was re-read okay. */
      (void)schd_file_has_changed(HOLIDAYS_FILE, 1);
      }
    }

  /*
   * If this is the first run during non-primetime, set all the execution
   * queues' observed primetime back to 'on'.  If it's primetime now, set
   * the "last run in primetime" global.
   */
  if (schd_ENFORCE_PRIME_TIME && schd_TimeNow >= schd_ENFORCE_PRIME_TIME)
    {
    if (schd_prime_time(0))
      {
      last_run_in_pt = 1;

      }
    else if (last_run_in_pt)
      {
      DBPRT(("%s: First non-pt run, reset queue observed times.\n", id));

      if (schd_BatchQueues)
        schd_reset_observed_pt(schd_BatchQueues);

      /* Last run was not in prime time. */
      last_run_in_pt = 0;
      }
    }

  /* Get the current list of all jobs known to our server.
   * Set the job->priority field so that each jobs is assign an
   * inherent priority, using several criteria including recent
   * past usage, nodes requsted, time on queue, originating queue
   * and then populate the schd_AllJobs list with these jobs
   */
  jobs = schd_get_jobs(NULL, NULL);

  /*
   * Check for queued jobs on any of the run queues.  This may happen if
   * there is some glitch and the POSIX jobs are checkpointed.
   * schedule_restart() will return non-zero if it finds and restarts
   * any jobs.  Recycle if this is the case.
   */
  if (schd_SCHED_RESTART_ACTION != SCHD_RESTART_NONE)
    {
    if (schedule_restart(jobs))
      {
      schd_free_jobs(jobs);
      return (0);
      }
    }

  /*
   * Sort jobs by the priority field set above. Note that the jobs
   * are reordered "in situ".  The sorting routine returns a pointer to
   * the new head of the list created by relinking the elements of the
   * linked list, or NULL if an error occurs.  Zero the original list
   * pointer to reduce confusion - the same list, in different order, now
   * lives on schd_AllJobs.
   */
  schd_AllJobs = schd_sort_jobs(jobs);

  jobs = NULL;

  /* now query the GRM on the T3e to get the node map information;
   * and then correlatate in-use PEs to running jobs, filling the global
   * schd_PEMAP in the process.
   */
  if (load_pe_map(schd_AllJobs))
    {
    /* unable to get PE map ... do something... */
    ;
    }

  /*
   * Get the queue limits and utilization for each queue about which the
   * scheduler knows.  Any jobs on schd_AllJobs (set by get_and_sort_jobs()
   * above) that belong to the queue will be placed on the queue->jobs
   * list.
   *
   * If PBS fails to provide us any information about a queue, treat it
   * as a fatal error.
   */

  error = get_all_queue_info(3 /* Number of queue lists */,
                             schd_SubmitQueue,
                             schd_BatchQueues,
                             schd_DedQueues);


  if (error < 0)
    {
    DBPRT(("get_all_queue_info() failed\n"));

    return (1); /* Bogus queue - don't recycle. */

    }
  else if (error > 0)
    {
    DBPRT(("queue failed sanity check - wait and recycle.\n"));

    sleep(WAIT_FOR_QUEUE_SANITY);

    return (0); /* Attempt to recycle scheduler. */
    }

  /* Fix added by jjones per Dr. Hook to change behavior of jobs being
   * moved from submit queue to exec queue before run, at request of
   * ERDC.
   */
  fix_jim(schd_SubmitQueue->queue, schd_BatchQueues->queue);


  /*
   * At this point, schd_AllJobs should hold only orphan jobs (i.e. only
   * jobs that belong to queues about which the scheduler does not care).
   * Note it and go on scheduling -- unless nothing is being scheduled,
   * this is more-or-less meaningless.
   */
  if (schd_AllJobs)
    {
    (void)sprintf(log_buffer, "Some jobs not claimed by queues.");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n%s: Unclaimed jobs: ", id, log_buffer, id));
#ifdef DEBUG

    for (this = schd_AllJobs; this != NULL; this = this->next)
      {
      DBPRT(("%s%s", this->jobid, this->next ? ", " : ""));
      }

    DBPRT(("\n"));

#endif /* DEBUG */
    }

  /* Dump the list of jobs being scheduled from submit queue. */
  if (schd_JOB_DUMPFILE)
    {
    (void)sprintf(log_buffer, "Dumping sorted job information to %s",
                  schd_JOB_DUMPFILE);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    make_job_dump(schd_JOB_DUMPFILE);
    }

  /*
   * Allocation and usage information are updated at [roughly] 2:00 AM
   * (Eastern time).  Since they may have been updated, attempt to fetch
   * them again in the middle of the night.
   */

  if (schd_NeedToGetDecayInfo)
    schd_decay_info("r");    /* get users' recent past usage */

  if (schd_ENFORCE_ALLOCATION && schd_TimeNow >= schd_ENFORCE_ALLOCATION)
    {
    /*
     * If the allocations file has already been loaded, consult the file
     * timestamp to determine if it has changed.  If so, flag that it
     * needs to be reloaded.
     */

    if (!schd_NeedToGetAllocInfo && schd_AllocFilename)
      schd_NeedToGetAllocInfo =
        schd_file_has_changed(schd_AllocFilename, 1);

    if (!schd_NeedToGetYTDInfo && schd_CurrentFilename)
      schd_NeedToGetYTDInfo =
        schd_file_has_changed(schd_CurrentFilename, 1);

    /* If either file needs to be [re]loaded, do so. */

    if (schd_NeedToGetAllocInfo || schd_NeedToGetYTDInfo)
      schd_alloc_info();
    }

  /*
   * We need to save the past usage data periodically, so that a restart
   * of pbs_sched doesn't lose it ...
   */
  if (schd_save_decay()) /* is it time yet ? */
    schd_decay_info("w"); /* yep, so do it */

  if (schd_SubmitQueue->queue->jobs &&
      !(schd_SubmitQueue->queue->flags & (QFLAGS_DISABLED | QFLAGS_STOPPED)))
    {
    /*
     * Test each job against the set of execution queues.  If it can
     * never be run in any queue, reject it immediately.  This saves
     * the user having to wait for the scheduler to get around to being
     * able to run it.
     */
    jobs = reject_unrunnables(schd_SubmitQueue->queue->jobs);

    /*
     * Look for queues whose execution hosts are in dedicated time.  If
     * any are found, note that fact and continue.  Otherwise, add them
     * to the normalQs list, which will be scheduled normally.  If the
     * flag is set indicating that one or more hosts is in dedtime, they
     * will be scheduled after everything else is done.
     */

    for (qptr = schd_BatchQueues; qptr != NULL; qptr = qptr->next)
      {
      if (schd_ENFORCE_DEDTIME && schd_TimeNow >= schd_ENFORCE_DEDTIME)
        outages = schd_host_outage(qptr->queue->exechost, 0);
      else
        outages = NULL;

      /*
       * Is there a scheduled outage right now for this host?  If so,
       * note that fact and continue to the next queue.  All of this
       * information is cached, so this isn't as expensive as it seems.
       */
      if (outages != NULL)
        {
        if ((outages->beg_time <= schd_TimeNow) &&
            (outages->end_time > schd_TimeNow))
          {
          DBPRT(("%s: Host %s is in dedtime (from %s:%s to %s:%s)\n",
                 id, outages->exechost,
                 outages->beg_datestr, outages->beg_timestr,
                 outages->end_datestr, outages->end_timestr));
          DBPRT(("%s: Queue %s@%s will not be scheduled.\n", id,
                 qptr->queue->qname, qptr->queue->exechost));

          /* This exechost is in dedicated time, ignore the queue. */
          hosts_in_dedtime ++;
          continue;

          }
        else if (outages->beg_time > schd_TimeNow)
          {

          /* Upcoming dedtime, but not yet.  Schedule the queue. */
          DBPRT(("%s: Host %s upcoming dedtime (at %s:%s to %s:%s)\n",
                 id, outages->exechost,
                 outages->beg_datestr, outages->beg_timestr,
                 outages->end_datestr, outages->end_timestr));
          }
        }

      /*
       * This host is not currently in dedicated time.  Add it to the
       * tail of the list of queues to be scheduled.
       */
      newqlp = (QueueList *)malloc(sizeof(QueueList));

      if (newqlp == NULL)
        {
        (void)sprintf(log_buffer, "malloc(QueueList) for %s@%s failed",
                      qptr->queue->qname, qptr->queue->exechost);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                   log_buffer);
        DBPRT(("%s: %s\n", id, log_buffer));

        if (normalQs)
          schd_free_qlist(normalQs);

        return (1);
        }

      newqlp->queue = qptr->queue;

      if (normalQtail)
        normalQtail->next = newqlp;
      else
        normalQs = newqlp;

      normalQtail = newqlp;

      newqlp->next = NULL;
      }

    DBPRT(("%s: calling schedule_jobs(", id));

    if (normalQs)
      {
      for (qptr = normalQs; qptr != NULL; qptr = qptr->next)
        DBPRT(("%s@%s%s", qptr->queue->qname, qptr->queue->exechost,
               qptr->next ? ", " : ""));
      }
    else
      {
      DBPRT(("<no batch queues>"));
      }

    DBPRT((")\n"));


    /* Now make the call to actually run some jobs */
    total_ran += ran = schedule_jobs(normalQs, jobs, reason);

    if (ran < 0)
      {
      DBPRT(("Could not run any jobs!\n"));
      }
    else
      {
      DBPRT(("RAN %d jobs.\n", ran));
      }

    if (normalQs)
      schd_free_qlist(normalQs);

    normalQs = normalQtail = NULL;
    }

  /*
   * Now check the dedtime queues with queued jobs for hosts that are
   * in dedicated time.  If any are found, comment the jobs appropriately
   * and/or schedule them.
   */

  for (qptr = schd_DedQueues; qptr != NULL; qptr = qptr->next)
    {
    if (qptr->queue->queued == 0)
      continue;

    DBPRT(("%s: schd_handle_dedicated_time(%s)\n", id, qptr->queue->qname));

    /*
     * Keep track of the next pointer, and zero the queue's next ptr so
     * it looks like a single queue.
     */
    next = qptr->next;

    qptr->next = NULL;

    ran = schd_handle_dedicated_time(qptr->queue);

    if (ran < 0)
      {
      (void)sprintf(log_buffer,
                    "schd_handle_dedicated_time(%s@%s) failed!",
                    qptr->queue->qname, qptr->queue->exechost);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                 log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));
      }
    else
      {
      DBPRT(("RAN %d jobs on %s@%s.\n", ran, qptr->queue->qname,
             qptr->queue->exechost));
      total_ran += ran;
      }

    /* Replace the zero'd next pointer to rechain the list. */
    qptr->next = next;
    }

  if (total_ran > 0)
    {
    (void)sprintf(log_buffer, "System resources after scheduling:");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    schd_dump_rsrclist();
    }

  (void)sprintf(log_buffer, ">>>  End Scheduling Cycle (ran %d jobs)  <<<",
                total_ran);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
  DBPRT(("%s\n", log_buffer));

  return (1);
  }

static int
schedule_jobs(QueueList *queues, Job *jobs, char *reason)
  {
  char *id = "schedule_jobs";
  int    numran;
  Job   *job;
  Queue *shortest;
  int    priority_to_1st = 1;

  /*
   * Since the sorting code has provided an order in which the jobs should
   * be run, attempt to honor that order by treating the first job on the
   * list as our first priority.  This amounts to draining the queue in
   * order to run that job, if necessary.
   *
   * If the job has been waiting too long, find the smallest queue in which
   * the job will fit, and consider its expected run time.  If the waiting
   * job cannot run when the queue has emptied, then go on to the next.
   * However, if there are jobs running on the queue, it is possible that
   * this queue could support the waiting job if it were started draining
   * now.  When enough jobs had exited, the waiting job would be runnable.
   * In order to determine if this is true, walk through the list of jobs,
   * which are sorted in order of completion (from soonest to last), and
   * find how many resources would be available after that job finished.
   * If there is space, calculate what time it will be when that many jobs
   * have completed, and see if the primetime limits apply at that time.
   * If the job fits in the primetime limits at that time, then start the
   * queue draining.  If it will not fit after all jobs have been tested,
   * then give up on this queue and go on to the next.
   *
   * If a queue was found that requires draining, mark it for draining.
   *
   * After the waiting job handling has completed, collect a list of
   * all the available execution queues, and place it into the pointer
   * given to this function by the caller.
   */

  for (job = jobs; job != NULL; job = job->next)
    {
    if (job->state != 'Q')
      continue;

    if (!priority_to_1st && !(job->flags & JFLAGS_WAITING))
      continue;

    DBPRT(("%s: job %s is %s (eligible for %s, needs %d nodes)\n", id,
           job->jobid,
           priority_to_1st ? "FIRSTJOB" :
           (job->flags & JFLAGS_PRIORITY) ? "SPECIAL" : "WAITING",
           schd_sec2val(job->eligible), job->nodes));

    /*
     * Find the smallest, shortest-wait queue in which this job will
     * fit.  If it is empty, great.  If not, mark it to be drained,
     * in anticipation of the job being run soon.  Note that the queue
     * drain_by time should only be shortened - it doesn't make sense
     * to push it out.
     */
    shortest = schd_find_drain(queues, job);

    if (shortest)
      {
      /*
       * If there are no jobs running in the queue, then unset the
       * draining flag (if present), so that the queue will be
       * available for this job.
       *
       * If there are running jobs, set the draining flag, and
       * adjust the empty_by value to be the expected time when
       * the job will first become runnable.
       */
      if (shortest->running == 0)
        {
        shortest->flags &= ~QFLAGS_DRAINING;
        }
      else
        {
        /* If running jobs, empty_by should be non-zero. */
        if (shortest->drain_by <= shortest->empty_by)
          {
          shortest->flags |= QFLAGS_DRAINING;

          DBPRT(("%s: shortest queue %s now draining, drain_by %s",
                 id, shortest->qname, ctime(&shortest->drain_by)));
          }
        }
      }

    /*
     * We have looked at (and possibly arranged for special treatment
     * of) the first job on the list.  Now only look for special or
     * waiting jobs.
     */
    priority_to_1st = 0;
    }

  numran = schd_pack_queues(jobs, queues, reason);

  if (numran < 0)
    {
    (void)sprintf(log_buffer,
                  "sched_pack_queues() failed!");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    }

  return (numran);
  }

/*
 * Get information about each of the queues in the list of lists.  If
 * schd_get_queue_limits() fails, return the error condition.  It may
 * be a transient or a hard failure, which the caller may want to deal
 * with.  If all queues are successful, return '0'.
 */
static int
get_all_queue_info(int numqlists, QueueList *list, ...)
  {
  va_list ap;
  int    count = 0, ret;
  QueueList *qptr;
  char *id = "get_all_queue_info";

  va_start(ap, numqlists);

  while (count < numqlists)
    {

    list = va_arg(ap, QueueList *);

    for (qptr = list; qptr != NULL; qptr = qptr->next)
      {

      /*
       * Get the limits, current resources, and any jobs for this
       * queue.
       */
      if ((ret = schd_get_queue_limits(qptr->queue)) != 0)
        {
        DBPRT(("get_all_queue_info: get_queue_limits for %s failed.\n",
               qptr->queue->qname));
        va_end(ap);
        return (ret);
        }

      /*
       * Set the queue flags if limits are exceeded.  Don't bother
       * getting a reason string.
       */
      schd_check_queue_limits(qptr->queue, NULL);
      }

    count ++;
    }

  va_end(ap);

  return (0);
  }

static int
schedule_restart(Job *joblist)
  {
  char   *id = "schedule_restart";
  Job    *job, *nextjob;
  QueueList *qptr;
  int     found, changed;
  int     local_errno = 0;

  changed = found = 0;

  for (job = joblist; job != NULL; job = nextjob)
    {
    nextjob = job->next;

    if (job->state != 'Q')
      continue;

    /*
     * See if the job is queued on one of the batch queues.  If not,
     * go on to the next job.
     */
    for (qptr = schd_BatchQueues; qptr != NULL; qptr = qptr->next)
      if (strcmp(qptr->queue->qname, job->qname) == 0)
        break;

    if (qptr == NULL)
      continue;

    found++;

    if (schd_SCHED_RESTART_ACTION == SCHD_RESTART_RERUN)
      {
      (void)sprintf(log_buffer, "Restart job '%s' on queue '%s'.",
                    job->jobid, job->qname);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
                 id, log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));

      schd_comment_job(job, schd_JobMsg[JOB_RESTARTED],
                       JOB_COMMENT_REQUIRED);

      if (schd_run_job_on(job, job->queue, schd_SCHED_HOST,
                          LEAVE_JOB_COMMENT))
        {
        (void)sprintf(log_buffer,
                      "Unable to run job '%s' on queue '%s'.", job->jobid,
                      job->qname);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                   log_buffer);
        }
      else
        changed ++;

      }
    else /* (SCHED_RESTART_ACTION == SCHD_RESTART_RESUBMIT) */
      {
      if (schd_TEST_ONLY)
        {
        DBPRT(("%s: would have moved %s back to queue %s\n", id,
               job->jobid, schd_SubmitQueue->queue->qname));
        }
      else
        {
        /* Move the job back to its originating queue. */
        if (pbs_movejob(connector, job->jobid,
                        job->oqueue, NULL, &local_errno) != 0)
          {
          (void)sprintf(log_buffer,
                        "failed to move %s to queue %s, %d", job->jobid,
                        job->oqueue, local_errno);
          log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                     log_buffer);
          DBPRT(("%s: %s\n", id, log_buffer));
          }
        else
          {
          (void)sprintf(log_buffer,
                        "Requeued job '%s' on queue '%s'.", job->jobid,
                        job->oqueue);
          log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
                     id, log_buffer);
          DBPRT(("%s: %s\n", id, log_buffer));
          schd_comment_job(job, schd_JobMsg[JOB_RESUBMITTED],
                           JOB_COMMENT_REQUIRED);
          changed ++;
          }
        }
      }
    }

  if (found)
    {
    if (schd_SCHED_RESTART_ACTION == SCHD_RESTART_RERUN)
      {
      (void)sprintf(log_buffer,
                    "Re-ran %d jobs (of %d) found queued on run queues.\n",
                    changed, found);
      }
    else
      {
      (void)sprintf(log_buffer,
                    "Moved %d queued jobs (of %d) from run queues back to '%s'.\n",
                    changed, found, schd_SubmitQueue->queue->qname);
      }

    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

    DBPRT(("%s: %s\n", id, log_buffer));
    }

  return (changed);
  }

static Job *
reject_unrunnables(Job *jobs)
  {
  Job *this, *nextjob;
  char tmpstr[300];

  for (this = jobs; this != NULL; this = nextjob)
    {
    nextjob = this->next;

    if (!schd_job_can_queue(this))
      {

      /*
       * If this job is at the head of the list, we must deal with
       * it specially.  We need to advance the list pointer forward
       * so that further scheduling will not be done on the now
       * bogus job.  Advance 'jobs', and make 'nextjob' the 'next'
       * pointer for the new head of the list.
       */
      if (this == jobs)
        {
        jobs = jobs->next;
        nextjob = jobs ? jobs->next : NULL;
        }

      DBPRT(("job %s does not fit on any execution queue - reject\n",

             this->jobid));
      schd_reject_job(this,
                      "Job will not fit on any execution queue.\n"
                      "\n"
                      "Use 'qstat -q' to get execution queue limits.\n");

      continue;
      }

    /*
     * Enforce maximum job limits
     * "Big" jobs are given a maximum walltime limit (WALLT_LARGE_LIMIT)
     * that differs from "small" jobs. (Job size distinction based on
     * the size specified by SMALL_JOB_MAX.) We need to reject any job
     * which violate these limits.
     *
     * Special-priority jobs are not affected.
     */
    if (!(this->flags & JFLAGS_PRIORITY) && (schd_SMALL_JOB_MAX > 0))
      {
      if (this->nodes <= schd_SMALL_JOB_MAX)
        {
        if (this->walltime > schd_WALLT_SMALL_LIMIT)
          {
          if (this == jobs)
            {
            jobs = jobs->next;
            nextjob = jobs ? jobs->next : NULL;
            }

          DBPRT(("job %s exceeds Small job walltime limit - reject\n",

                 this->jobid));
          sprintf(tmpstr,
                  "Job exceeds maximum walltime limit (%s) policy\n"
                  "\tfor small jobs (1 - %d nodes).\n",
                  schd_sec2val(schd_WALLT_SMALL_LIMIT),
                  schd_SMALL_JOB_MAX);
          schd_reject_job(this, tmpstr);
          continue;
          }
        }
      else
        {
        if (this->walltime > schd_WALLT_LARGE_LIMIT)
          {
          if (this == jobs)
            {
            jobs = jobs->next;
            nextjob = jobs ? jobs->next : NULL;
            }

          DBPRT(("job %s exceeds Large job walltime limit - reject\n",

                 this->jobid));
          sprintf(tmpstr,
                  "Job exceeds maximum walltime limit (%s) policy\n"
                  "\tfor large jobs (%d+ nodes).\n",
                  schd_sec2val(schd_WALLT_LARGE_LIMIT),
                  schd_SMALL_JOB_MAX + 1);
          schd_reject_job(this, tmpstr);

          continue;
          }
        }
      }
    }

  return (jobs);
  }

static int
make_job_dump(char *dumpfile)
  {
  char    *id = "make_job_dump";
  FILE    *dump;
  QueueList *qptr;

  /*
   * Attempt to open the dump file, creating it if necessary.  It should
   * be truncated each time this runs, so don't open with append mode.
   */

  if ((dump = fopen(dumpfile, "w")) == NULL)
    {
    (void)sprintf(log_buffer, "Cannot write to %s: %s\n", dumpfile,
                  strerror(errno));
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    return (-1);
    }

  /* Head the file with a timestamp. */
  fprintf(dump, "%s\n", ctime(&schd_TimeNow));

  /* And some more useful information about the state of the world. */
  fprintf(dump, "Scheduler running on '%s'\n", schd_ThisHost);

  fprintf(dump, "Prime-time is ");

  if (schd_ENFORCE_PRIME_TIME && schd_TimeNow >= schd_ENFORCE_PRIME_TIME)
    {
    fprintf(dump, "from %s ", schd_sec2val(schd_PRIME_TIME_START));
    fprintf(dump, "to %s.\n", schd_sec2val(schd_PRIME_TIME_END));
    }
  else
    fprintf(dump, "not enforced.\n");

  fprintf(dump, "\nJOBS LISTED IN ORDER FROM HIGHEST TO LOWEST PRIORITY\n\n");

  /* Now dump the jobs queued on the various queues, in order of priority. */
  qptr = schd_SubmitQueue;

  if (qptr->queue->jobs)
    {
    fprintf(dump, "Jobs on submit queue '%s':\n", qptr->queue->qname);
    dump_sorted_jobs(dump, qptr->queue->jobs);
    }

  for (qptr = schd_DedQueues; qptr != NULL; qptr = qptr->next)
    {
    if (qptr->queue->jobs)
      {
      fprintf(dump, "Jobs on dedicated queue '%s':\n",
              qptr->queue->qname);
      dump_sorted_jobs(dump, qptr->queue->jobs);
      }
    }

  if (fclose(dump))
    {
    (void)sprintf(log_buffer, "close(%s): %s\n", dumpfile, strerror(errno));
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    return (-1);
    }

  return (0);
  }

static int
dump_sorted_jobs(FILE *dump, Job *joblist)
  {
  Job     *job;
  int      njobs;
  int      elig_mesg = 0;

#define DUMP_JID_LEN  16
#define DUMP_STATE_LEN  1
#define DUMP_OWNER_LEN  8
#define DUMP_NODES_LEN  3
#define DUMP_PRIORITY_LEN 4
#define DUMP_WALLT_LEN  8
#define DUMP_WAITT_LEN  8
#define DUMP_ELIGI_LEN  9 /* time plus '*' if wait != eligible */
#define DUMP_FLAGS_LEN  18

  char     jid[DUMP_JID_LEN + 1];
  char     owner[DUMP_OWNER_LEN + 1];
  char     wallt[DUMP_WALLT_LEN + 1];
  char     waitt[DUMP_WAITT_LEN + 1];
  char     eligi[DUMP_ELIGI_LEN + 1];
  char     flags[DUMP_FLAGS_LEN + 1];

  fprintf(dump, "  %*s %*s %*s %*s %*s %*s %*s %*s %*s\n",
          -DUMP_JID_LEN,  "Job ID",
          -DUMP_STATE_LEN, "S",
          -DUMP_OWNER_LEN, "Owner",
          -DUMP_NODES_LEN, "PEs",
          -DUMP_PRIORITY_LEN, "Pri",
          -DUMP_WALLT_LEN, "Walltime",
          -DUMP_WAITT_LEN, "Q'd for",
          -DUMP_ELIGI_LEN, "Eligible",
          -DUMP_FLAGS_LEN, "Flags");

  fprintf(dump, "  %*s %c %*s %*s %*s %*s %*s %*s %*s\n",
          -DUMP_JID_LEN,  "----------------",
          '-',
          -DUMP_OWNER_LEN, "--------",
          -DUMP_NODES_LEN, "---",
          -DUMP_PRIORITY_LEN, "----",
          -DUMP_WALLT_LEN, "--------",
          -DUMP_WAITT_LEN, "--------",
          -DUMP_ELIGI_LEN, "---------",
          -DUMP_FLAGS_LEN, "------------------");

  for (njobs = 0, job = joblist; job != NULL; job = job->next)
    {

    njobs++;
    strncpy(jid, job->jobid, DUMP_JID_LEN);
    strncpy(owner, job->owner, DUMP_OWNER_LEN);
    strcpy(wallt, schd_sec2val(job->walltime));
    strcpy(waitt, schd_sec2val(job->time_queued));
    strcpy(eligi, schd_sec2val(job->eligible));

    if (job->time_queued != job->eligible)
      {
      strcat(eligi, "*");
      elig_mesg ++;
      }

    flags[0] = '\0';

    /* Watch length of 'flags[]' array! */

    if (job->flags & JFLAGS_INTERACTIVE)
      strcat(flags, "Int ");

    /* "Priority" jobs are marked as being waiting, even if they're new. */
    if (job->flags & JFLAGS_PRIORITY)
      strcat(flags, "High ");
    else if (job->flags & JFLAGS_WAITING)
      strcat(flags, "Wait ");

    if (job->flags & JFLAGS_DEDICATED)
      strcat(flags, "Ded ");

    /* Trim off the trailing space if any flags were listed. */
    if (flags[0] != '\0')
      flags[strlen(flags) - 1] = '\0';

    fflush(dump);

    fprintf(dump, "  %*s %c %*s %*d %*d %*s %*s %*s %*s\n",
            -DUMP_JID_LEN, jid,
            job->state,
            -DUMP_OWNER_LEN, owner,
            -DUMP_NODES_LEN, job->nodes,
            -DUMP_PRIORITY_LEN, job->priority,
            -DUMP_WALLT_LEN, wallt,
            -DUMP_WAITT_LEN, waitt,
            -DUMP_ELIGI_LEN, eligi,
            -DUMP_FLAGS_LEN, flags);
    }

  fprintf(dump, "    Total: %d job%s\n\n", njobs, (njobs == 1) ? "" : "s");

  if (elig_mesg)
    {
    fprintf(dump, "Jobs marked with a ``*'' have an etime different "
            "from their ctime.\n\n");
    }

  return (njobs);
  }

/* Fix added by jjones per Dr. Hook to change behavior of jobs being
 * moved from submit queue to exec queue before run, at request of
 * ERDC.
 */

void fix_jim(Queue *submit, Queue *jim)
  {
  /* there'll be no running jobs on 'jim', but the scheduler  */
  /* _assumes_ that jobs *only* run out of that queue ... :-P */
  jim->running = submit->running;
  submit->running = 0;

  /* also, the scheduler would be surprised to find any */
  /* resources "assigned" to any queue but 'jim' ...    */
  jim->nodes_assn = submit->nodes_assn;
  jim->nodes_rsvd = submit->nodes_rsvd;   /* _probably_ unnecessary */
  submit->nodes_assn = submit->nodes_rsvd = 0;

  /* for the "draining" logic, we should probably adjust 'empty_by' */
  jim->empty_by = submit->empty_by;

  return;
  }

