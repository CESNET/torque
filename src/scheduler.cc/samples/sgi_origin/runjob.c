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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PBS header files */
#include        "pbs_error.h"
#include        "pbs_ifl.h"
#include        "log.h"

/* Toolkit header files */
#include "toolkit.h"
#include "gblxvars.h"

extern int connector;

/*
 * run_job_on(job, queue, set_comment) :
 *
 * Run the job pointed to by 'job' in the queue on which it is enqueued.
 * If 'queue' is non-NULL, move the job to the queue before running it.
 * If 'set_comment' is non-zero, the job comment will be changed to reflect
 * the time at which the job was started.
 */

int
schd_run_job_on(Job *job, Queue *destq, char *exechost, int set_comment)
  {
  char   *id = "schd_run_job_on";
  Queue  *srcq = NULL;
  char   *date;
  char    reason[128];

  /* Get the datestamp from 'ctime()'.  Remove the trailing '\n'. */
  date = ctime(&schd_TimeNow);
  date[strlen(date) - 1] = '\0';

  if (job->state != 'Q')
    {
    (void)sprintf(log_buffer,
                  "run_job_on(%s, %s) called with job not in 'Q' state.\n",
                  job->jobid, (destq ? destq->qname : "(NULL)"));
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));
    return (-1);
    }

  if (set_comment)
    {
    sprintf(reason, "Started on %s", date);

    if (job->flags & JFLAGS_PRIORITY)
      {
      strcat(reason, " (special access job)");
      }
    else if (job->flags & JFLAGS_WAITING)
      {
      strcat(reason, " (waited too long)");
      }

    schd_comment_job(job, reason, JOB_COMMENT_REQUIRED);
    }

#ifdef NODEMASK
  if (destq->flags & QFLAGS_NODEMASK)
    {
    schd_mask2str(reason, &job->nodemask);

    if (schd_TEST_ONLY)
      {
      (void)sprintf(log_buffer,
                    "would have set nodemask for job %s to %s", job->jobid, reason);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
                 id, log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));
      }
    else
      {
      if (schd_alterjob(connector, job, ATTR_l, reason, "nodemask"))
        {
        (void)sprintf(log_buffer,
                      "failed to set nodemask %s for job %s", job->jobid, reason);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
                   id, log_buffer);
        DBPRT(("%s: %s\n", id, log_buffer));
        }
      }
    }

#endif /* NODEMASK */

  /*
   * If a destination Queue is provided, and it is different from the
   * source queue, then ask PBS to move the job to that queue before
   * running it.
   */
  srcq = job->queue;

  if ((destq != NULL) && (strcmp(destq->qname, srcq->qname) != 0))
    {
    /* Move the job from its queue to the specified run queue. */
    if (schd_TEST_ONLY)
      {
      DBPRT(("%s: would have moved %s from queue %s to queue %s\n", id,
             job->jobid, srcq->qname, destq->qname));
      }
    else
      {
      if (pbs_movejob(connector, job->jobid, destq->qname, NULL))
        {
        (void)sprintf(log_buffer, "move job %s to queue %s failed, %d",
                      job->jobid, destq->qname, pbs_errno);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
                   id, log_buffer);
        DBPRT(("%s: %s\n", id, log_buffer));
        return (-1);
        }
      }

    schd_move_job_to(job, destq);
    }

  /* Give the job handle (JOBID) to PBS to run. */
  if (schd_TEST_ONLY)
    {
    DBPRT(("%s: would have run %s on queue %s\n", id, job->jobid,
           destq->qname));
    }
  else
    {
    if (pbs_runjob(connector, job->jobid, exechost, NULL))
      {
      (void)sprintf(log_buffer, "failed start job %s on queue %s@%s, %d",
                    job->jobid, destq->qname, exechost, pbs_errno);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));

      /*
       * Running failed!  Move the job back to the source queue (if
       * applicable) before returning.  This prevents jobs being marooned
       * in execution queues.
       */

      if (srcq)
        {
        DBPRT(("Attempting to move job %s back to queue %s\n",
               job->jobid, srcq->qname));

        if (pbs_movejob(connector, job->jobid, srcq->qname, NULL))
          {
          (void)sprintf(log_buffer,
                        "failed to move job %s back to queue %s, %d", job->jobid,
                        srcq->qname, pbs_errno);

          log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                     log_buffer);
          DBPRT(("%s: %s\n", id, log_buffer));
          }

        schd_move_job_to(job, srcq);
        }

      return (-1);
      }

    }

  /* PBS accepted the job (and presumably will run it).  Log it. */
  (void)sprintf(log_buffer, "job %s started on %s@%s",
                job->jobid, destq->qname, exechost);

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  DBPRT(("%s: %s\n", id, log_buffer));

  /*
   * Change the state of the local representation of the job to "Running".
   */
  job->state = 'R';

  /*
   * Account for the job on this queue's statistics.  'queued' will be
   * bumped up if the queued job was moved to a new destination queue.
   */

  job->queue->queued --;

  job->queue->running ++;

  /* The queue is no longer idle.  Unset the idle timer. */
  job->queue->idle_since = 0;

  return (0);    /* Job successfully started. */
  }

/*
 * schd_charge_job(job, queue, rsrcs)
 *
 * Account for the job resources requested by 'job' in the assigned and
 * available resources in 'queue' and 'rsrcs'
 *
 * Possible side effects:
 *  - If job allocates all of some resource, the queue will be marked "FULL".
 *  - queue->empty_by time may be extended to the end of this job.
 *
 * Returns 0 on success.
 */
int
schd_charge_job(Job *job, Queue *queue, Resources *rsrcs)
  {
#ifdef DEBUG
  char   *id = "schd_charge_job";
#endif /* DEBUG */
  time_t  job_ends;

#ifdef NODEMASK
  Bitfield nodes;
#endif /* NODEMASK */

  /* Update the queue and resources with the job's expected usage. */

  queue->nodes_assn  += job->nodes;
  rsrcs->nodes_alloc += job->nodes;
  rsrcs->freemem     -= (job->nodes * MB_PER_NODE);
  rsrcs->loadave     += (double)(job->nodes * PE_PER_NODE);

#ifdef NODEMASK
  /*
   * If the queue has a nodemask, account for the job's nodemask in the
   * available nodes.  Do a sanity check to make sure that the job's
   * nodemask actually fits the nodes assigned to this queue.
   */

  if (queue->flags & QFLAGS_NODEMASK)
    {
    /* Compute the nodes that lie within the queue's queuemask. */
    BITFIELD_CPY(&nodes, &(job->nodemask));
    BITFIELD_ANDM(&nodes, &(queue->queuemask));

    if (!BITFIELD_EQ(&nodes, &(job->nodemask)))
      {
      (void)sprintf(log_buffer,
                    "job %s run on queue %s has nodemask outside queue",
                    job->jobid, queue->qname);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      DBPRT(("%s: %s\n", id, log_buffer));

      }
    else
      {
      /*
       * The job's nodemask lies within the queue.  Account for it by
       * adding its nodes to the host's in-use mask, and removing them
       * from the available mask.
       */
      BITFIELD_SETM(&(rsrcs->nodes_used), &nodes);
      BITFIELD_CLRM(&(queue->availmask), &nodes);
      }
    }

#endif /* NODEMASK */

  /*
   * If this is an HPM job, assert that the HPM counters are in use for
   * this host.
   */
  if (schd_MANAGE_HPM && (job->flags & JFLAGS_NEEDS_HPM))
    rsrcs->flags |= RSRCS_FLAGS_HPM_IN_USE;

  rsrcs->njobs ++;

  /* Has the queue node limit been reached?  If so, note it and fail. */
  if ((queue->nodes_max != UNSPECIFIED) &&
      (queue->nodes_max <= queue->nodes_assn))
    {
    DBPRT(("%s: Queue %s node limit (%d) reached - mark full\n", id,
           queue->qname, queue->nodes_max));
    queue->flags |= QFLAGS_FULL;
    }

  /*
   * If job specified a walltime, see if the queue "empty at" time needs
   * to be updated.  This function should only be called once we are
   * committed to running this job, so it is appropriate to update this
   * value at this point.
   */
  if (job->walltime != UNSPECIFIED)
    {
    job_ends = schd_TimeNow + job->walltime;

    if (job_ends > queue->empty_by)
      {
      DBPRT(("%s: Queue %s empty_by %s to %s", id, queue->qname,
             queue->empty_by ? "bumped" : "set to", ctime(&job_ends)));

      queue->empty_by = job_ends;
      }
    }

  /* Lastly, update the Recent Usage Table used by the sorting algorithm */
  schd_update_resource_usage(job);

  return (0);
  }
