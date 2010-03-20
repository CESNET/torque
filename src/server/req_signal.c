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
/*
 * req_signaljob.c - functions dealing with sending a signal
 *       to a running job.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdio.h>
#include "libpbs.h"
#include <errno.h>
#include <signal.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"

/* Private Function local to this file */

static void post_signal_req (struct work_task *);

/* Global Data Items: */

extern int   LOGLEVEL;

extern void   set_old_nodes (job *);

extern job  *chk_job_request(char *, struct batch_request *);

/*
 * req_signaljob - service the Signal Job Request
 *
 * This request sends (via MOM) a signal to a running job.
 * MOM procceses in req_signaljob()
 * MOM replies with either ack or reject
 * Server gets response from MOM which invokes post_signal_req()
 * Which then replies to the requester with either a reject or ack
 */

void req_signaljob(

  struct batch_request *preq)  /* I */

  {
  job *pjob;
  int  rc;

  if ((pjob = chk_job_request(preq->rq_ind.rq_signal.rq_jid, preq)) == 0)
    {
    return;
    }

  /* the job must be running */

  if (pjob->ji_qs.ji_state != JOB_STATE_RUNNING)
    {
    req_reject(PBSE_BADSTATE, 0, preq, NULL, NULL);

    return;
    }

  /* Special pseudo signals for suspend and resume require op/mgr */

  if (!strcmp(preq->rq_ind.rq_signal.rq_signame, SIG_RESUME) ||
      !strcmp(preq->rq_ind.rq_signal.rq_signame, SIG_SUSPEND))
    {
    if ((preq->rq_perm & (ATR_DFLAG_OPRD | ATR_DFLAG_OPWR | ATR_DFLAG_MGRD | ATR_DFLAG_MGWR)) == 0)
      {
      /* for suspend/resume, must be mgr/op */

      req_reject(PBSE_PERM, 0, preq, NULL, NULL);

      return;
      }

    preq->rq_extra = pjob;  /* save job ptr for post_signal_req() */
    }

  /* FIXME: need a race-free check for available free subnodes before
   * resuming a suspended job */

#ifdef DONOTSUSPINTJOB
  /* interactive jobs don't resume correctly so don't allow a suspend */

  if (!strcmp(preq->rq_ind.rq_signal.rq_signame, SIG_SUSPEND) &&
      (pjob->ji_wattr[(int)JOB_ATR_interactive].at_flags & ATR_VFLAG_SET) &&
      (pjob->ji_wattr[(int)JOB_ATR_interactive].at_val.at_long > 0))
    {
    req_reject(PBSE_JOBTYPE, 0, preq, NULL, NULL);

    return;
    }

#endif

  if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer, "relaying signal request to mom %lu",
            pjob->ji_qs.ji_un.ji_exect.ji_momaddr);

    log_record(
      PBSEVENT_SCHED,
      PBS_EVENTCLASS_REQUEST,
      "req_signaljob",
      log_buffer);
    }

  /* send reply for asynchronous suspend */
  if (preq->rq_type == PBS_BATCH_AsySignalJob)
    {
    reply_ack(preq);
    }

  /* pass the request on to MOM */

  if ((rc = relay_to_mom(
              pjob,
              preq,
              post_signal_req)))
    {
    req_reject(rc, 0, preq, NULL, NULL);  /* unable to get to MOM */

    return;
    }

  /* After MOM acts and replies to us, we pick up in post_signal_req() */

  /* SUCCESS */

  return;
  }  /* END req_signaljob() */





/*
 * issue_signal - send an internally generated signal to a running job
 */

int issue_signal(

  job  *pjob,
  char *signame, /* name of the signal to send */
  void (*func)(struct work_task *),
  void *extra) /* extra parameter to be stored in sig request */

  {
  int rc;

  struct batch_request *newreq;

  /* build up a Signal Job batch request */

  if ((newreq = alloc_br(PBS_BATCH_SignalJob)) == NULL)
    {
    /* FAILURE */

    return(PBSE_SYSTEM);
    }

  newreq->rq_extra = extra;

  strcpy(newreq->rq_ind.rq_signal.rq_jid, pjob->ji_qs.ji_jobid);

  strncpy(newreq->rq_ind.rq_signal.rq_signame, signame, PBS_SIGNAMESZ);

  rc = relay_to_mom(
         pjob,
         newreq,
         func);

  /* when MOM replies, we just free the request structure */

  return(rc);
  }  /* END issue_signal() */





/*
 * post_signal_req - complete a Signal Job Request (externally generated)
 */

static void post_signal_req(

  struct work_task *pwt)

  {
  job                  *pjob;

  struct batch_request *preq;

  svr_disconnect(pwt->wt_event); /* disconnect from MOM */

  preq = pwt->wt_parm1;

  preq->rq_conn = preq->rq_orgconn;  /* restore client socket */

  if (preq->rq_reply.brp_code)
    {
    log_event(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_REQUEST,
      preq->rq_ind.rq_signal.rq_jid,
      pbse_to_txt(PBSE_MOMREJECT));

    errno = 0;

    req_reject(preq->rq_reply.brp_code, 0, preq, NULL, NULL);
    }
  else
    {
    pjob = preq->rq_extra;

    if (strcmp(preq->rq_ind.rq_signal.rq_signame, SIG_SUSPEND) == 0)
      {
      if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_Suspend) == 0)
        {
        pjob->ji_qs.ji_svrflags |= JOB_SVFLG_Suspend;

        set_statechar(pjob);

        job_save(pjob, SAVEJOB_QUICK, 0);

        /* release resources allocated to suspended job - NORWAY */

        free_nodes(pjob);
        }
      }
    else if (strcmp(preq->rq_ind.rq_signal.rq_signame, SIG_RESUME) == 0)
      {
      if (pjob->ji_qs.ji_svrflags & JOB_SVFLG_Suspend)
        {
        /* re-allocate assigned node to resumed job - NORWAY */

        set_old_nodes(pjob);

        pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_Suspend;

        set_statechar(pjob);

        job_save(pjob, SAVEJOB_QUICK, 0);
        }
      }

    reply_ack(preq);
    }

  return;
  }  /* END post_signal_req() */

/* END req_signal.c */

