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
 * svr_holdjob.c 
 *
 * Functions relating to the Hold and Release Job Batch Requests.
 *
 * Included funtions are:
 *	req_holdjob()
 *	req_releasejob()
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "acct.h"
#include "svrfunc.h"

/* Private Functions Local to this file */

static void process_hold_reply A_((struct work_task *));

/* Global Data Items: */

extern struct server server;
extern char	*msg_jobholdset;
extern char	*msg_jobholdrel;
extern char	*msg_mombadhold;
extern char	*msg_postmomnojob;
extern time_t	 time_now;

int chk_hold_priv A_((long val, int perm));
int get_hold A_((tlist_head *, char **, attribute *));



/*
 * chk_hold_priv - check that client has privilege to set/clear hold
 */

int chk_hold_priv(

  long val,   /* hold bits being changed */
  int  perm)  /* client privilege */

  {
  if ((val & HOLD_s) && ((perm & ATR_DFLAG_MGWR) == 0))
    {
    return(PBSE_PERM);
    }

  if ((val & HOLD_o) && ((perm & (ATR_DFLAG_MGWR|ATR_DFLAG_OPWR)) == 0))
    {
    return(PBSE_PERM);
    }

  return(PBSE_NONE);
  }  /* END chk_hold_priv() */



/*
 * req_holdjob - service the Hold Job Request
 *
 *	This request sets one or more holds on a job.
 *	The state of the job may change as a result.
 */

void req_holdjob(

  struct batch_request *preq)

  {
  long		*hold_val;
  int		 newstate;
  int		 newsub;
  long		 old_hold;
  job    *pjob;
  char    *pset;
  int     rc;
  attribute temphold;
 
  pjob = chk_job_request(preq->rq_ind.rq_hold.rq_orig.rq_objname,preq);

  if (pjob == NULL)
    {
    return;
    }

  /* cannot do anything until we decode the holds to be set */

  if ( (rc=get_hold(&preq->rq_ind.rq_hold.rq_orig.rq_attr, &pset, 
                    &temphold)) != 0)
    {
    req_reject(rc,0,preq,NULL,NULL);
    return;
    }
      
  /* if other than HOLD_u is being set, must have privil */

  if ((rc = chk_hold_priv(temphold.at_val.at_long, preq->rq_perm)) != 0)
    {
      req_reject(rc,0,preq,NULL,NULL);
      return;
    }

  hold_val = &pjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long;
  old_hold = *hold_val;
  *hold_val |= temphold.at_val.at_long;
  pjob->ji_wattr[(int)JOB_ATR_hold].at_flags |= ATR_VFLAG_SET;
  sprintf(log_buffer, msg_jobholdset, pset, preq->rq_user,
          preq->rq_host);

  if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING)
    {
       
    /* have MOM attempt checkpointing */

    if ((rc = relay_to_mom(pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
               preq, process_hold_reply)) != 0)
      {
      *hold_val = old_hold;  /* reset to the old value */
      req_reject(rc, 0, preq,NULL,NULL);
      } 
    else
      {
      pjob->ji_qs.ji_substate = JOB_SUBSTATE_RERUN;
      pjob->ji_qs.ji_svrflags |=
          JOB_SVFLG_HASRUN | JOB_SVFLG_CHKPT;
      job_save(pjob, SAVEJOB_QUICK);
      LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid, log_buffer);
      }
    } 
  else 
    {
    /* everything went well, may need to update the job state */

    LOG_EVENT(
      PBSEVENT_JOB, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      log_buffer);

    if (old_hold != *hold_val) 
      {
      /* indicate attributes changed     */

      pjob->ji_modified = 1;   

      svr_evaljobstate(pjob,&newstate,&newsub,0);

      svr_setjobstate(pjob,newstate,newsub);
      }

    reply_ack(preq);
    }
  }  /* END req_holdjob() */




/*
 * req_releasejob - service the Release Job Request 
 *
 *  This request clears one or more holds on a job.
 *	As a result, the job might change state.
 */

void req_releasejob(

  struct batch_request *preq)	/* ptr to the decoded request   */

  {
  int		 newstate;
  int		 newsub;
  long		 old_hold;
  job		*pjob;
  char		*pset;
  int		 rc;
  attribute      temphold;

  pjob = chk_job_request(preq->rq_ind.rq_release.rq_objname,preq);

  if (pjob == NULL)
    {
    return;
    }

  /* cannot do anything until we decode the holds to be set */

	if ( (rc=get_hold(&preq->rq_ind.rq_hold.rq_orig.rq_attr, &pset, &temphold)) != 0) {
		req_reject(rc, 0, preq,NULL,NULL);
		return;
	}
			
	/* if other than HOLD_u is being released, must have privil */

	if ((rc = chk_hold_priv(temphold.at_val.at_long, preq->rq_perm)) != 0) {
			req_reject(rc, 0, preq,NULL,NULL);
			return;
	}

	/* all ok so far, unset the hold */

	old_hold = pjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long;
	if ((rc = job_attr_def[(int)JOB_ATR_hold].at_set(&pjob->ji_wattr[(int)JOB_ATR_hold], &temphold, DECR))) {
		req_reject(rc, 0, preq,NULL,NULL);
		return;
	}

  /* everything went well, if holds changed, update the job state */

  if (old_hold != pjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long) 
    {
    pjob->ji_modified = 1;	/* indicates attributes changed */

    svr_evaljobstate(pjob,&newstate,&newsub, 0);

    svr_setjobstate(pjob,newstate,newsub); /* saves job */
    }

  sprintf(log_buffer,msg_jobholdrel, 
    pset, 
    preq->rq_user,
    preq->rq_host);

  LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, pjob->ji_qs.ji_jobid,
    log_buffer);

  reply_ack(preq);

  return;
  }  /* END req_releasejob() */





/*
 * get_hold - search a list of attributes (svrattrl) for the hold-types
 * 	attribute.  This is used by the Hold Job and Release Job request,
 *	therefore it is an error if the hold-types attribute is not present,
 *	or there is more than one.
 *
 *	Decode the hold attribute into temphold.
 */

int get_hold(

  tlist_head *phead,
  char      **pset,     /* O - ptr to hold value */
  attribute *temphold   /* O - ptr to attribute to decode value into  */
  )

  {
  int		 have_one = 0;
  struct svrattrl *holdattr = NULL;
  struct svrattrl *pal;

  pal = (struct svrattrl *)GET_NEXT((*phead));

  while (pal != NULL) 
    {
    if (!strcmp(pal->al_name,job_attr_def[(int)JOB_ATR_hold].at_name)) 
      {
      holdattr = pal;

      *pset    = pal->al_value;

      have_one++;
      } 
    else 
      {
      return(PBSE_IVALREQ);
      }

    pal = (struct svrattrl *)GET_NEXT(pal->al_link);
    }

  if (have_one != 1)
    {
    return(PBSE_IVALREQ);
    }

  /* decode into temporary attribute structure */

  clear_attr(temphold,&job_attr_def[(int)JOB_ATR_hold]);

  return(job_attr_def[(int)JOB_ATR_hold].at_decode(
    temphold,
    holdattr->al_name,
    (char *)0,
    holdattr->al_value) );
  }






/*
 * process_hold_reply
 *	called when a hold request was sent to MOM and the answer
 *	is received.  Completes the hold request for running jobs.
 */

static void process_hold_reply(

  struct work_task *pwt)
  {
  job		     *pjob;
  struct batch_request *preq;
  int		 newstate;
  int		 newsub;

  svr_disconnect(pwt->wt_event);	/* close connection to MOM */

  preq = pwt->wt_parm1;
  preq->rq_conn = preq->rq_orgconn;  /* restore client socket */

  if ((pjob = find_job(preq->rq_ind.rq_hold.rq_orig.rq_objname)) == (job *)0)
    {
    LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB,
        preq->rq_ind.rq_hold.rq_orig.rq_objname,
        msg_postmomnojob);
    req_reject(PBSE_UNKJOBID, 0, preq,NULL,msg_postmomnojob);
    }
  else if (preq->rq_reply.brp_code != 0)
    {
    pjob->ji_qs.ji_substate = JOB_SUBSTATE_RUNNING;  /* reset it */
	  if (preq->rq_reply.brp_code != PBSE_NOSUP)
  	  {
      sprintf(log_buffer, msg_mombadhold, preq->rq_reply.brp_code);
      LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid, log_buffer);
      req_reject(preq->rq_reply.brp_code, 0, preq,NULL,log_buffer);
      }
      else
      {
      reply_ack(preq);
      }
    }
  else
    {
    /* record that MOM has a checkpoint file */

    /* Stupid PBS_CHKPT_MIGRATE is defined as zero therefore this code will never fire.
     * And if these flags are not set, start_exec will not try to run the job from
     * the checkpoint image file.
     */

    pjob->ji_qs.ji_svrflags |= JOB_SVFLG_CHKPT;
    if (preq->rq_reply.brp_auxcode)  /* chkpt can be moved */
      {
      pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_CHKPT;
      pjob->ji_qs.ji_svrflags |=  JOB_SVFLG_HASRUN | JOB_SVFLG_ChkptMig;
      }

    pjob->ji_modified = 1;    /* indicate attributes changed     */
    svr_evaljobstate(pjob,&newstate,&newsub,0);
    svr_setjobstate(pjob,newstate,newsub); /* saves job */

    account_record(PBS_ACCT_CHKPNT, pjob, (char *)0);  /* note in accounting file */
    reply_ack(preq);
    }
  }
