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
 * svr_stat.c 
 *
 * Functions relating to the Status Job, Status Queue, and
 * 	Status Server Batch Requests.
 */
#include <pbs_config.h>   /* the master config generated by configure */

#define STAT_CNTL 1

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "libpbs.h"
#include <ctype.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "queue.h"
#include "credential.h"
#include "batch_request.h"
#include "job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "net_connect.h"
#include "pbs_nodes.h"

/* Global Data Items: */

extern struct server   server;
extern list_head       svr_alljobs;
extern list_head       svr_queues;
extern char            server_name[];
extern attribute_def   svr_attr_def[];
extern attribute_def   que_attr_def[];
extern attribute_def   job_attr_def[];
extern int	           pbs_mom_port;
extern time_t	         time_now;
extern char	          *msg_init_norerun;
extern struct pbsnode *tfind_addr();

/* Extern Functions */

int status_job A_((job *,struct batch_request *,svrattrl *,list_head *,int *));
int status_attrib A_((svrattrl *,attribute_def *,attribute *,int,int,list_head *,int *,int));
extern int   svr_connect A_((pbs_net_t, unsigned int, void (*)(int), enum conn_type));
extern int status_nodeattrib(svrattrl *,attribute_def *,struct pbsnode *,int,int,list_head *,int*);

/* Private Data Definitions */

static int bad;

/* The following private support functions are included */

static void update_state_ct A_((attribute *, int *, char *));
static int  status_que A_((pbs_queue *, struct batch_request *, list_head *));
static int status_node A_(( struct pbsnode *, struct batch_request *, list_head *));
static void req_stat_job_step2 A_((struct stat_cntl *));
static void stat_update A_((struct work_task *));

/*
 * req_stat_job - service the Status Job Request 
 *
 *	This request processes the request for status of a single job or
 *	the set of jobs at a destination.  
 *	If SRV_ATR_PollJobs is not set or false (default), this takes three
 *	steps because of running jobs being known to MOM:
 *	  1. validate and setup the request (done here).
 *	  2. for each candidate job which is running and for which there is no
 *	     current status, ask MOM for an update.
 *	  3. form the reply for each candidate job and return it to the client.
 *      If SRV_ATR_PollJobs is true, then we skip step 2.
 */

void req_stat_job(

  struct batch_request *preq)	/* ptr to the decoded request   */

  {
  struct stat_cntl *cntl;	/* see svrfunc.h		*/
  char		   *name;
  job		   *pjob = NULL;
  pbs_queue	   *pque = NULL;
  int		    rc = 0;
  int		    type = 0;

  /*
   * first, validate the name of the requested object, either
   * a job, a queue, or the whole server.
   */

  /* FORMAT:  name = { <JOBID> | <QUEUEID> | '' } */

  name = preq->rq_ind.rq_status.rq_id;

  if (isdigit((int)*name)) 
    {
    pjob = find_job(name);	/* status a single job */

    if (pjob)
      type = 1;	
    else
      rc = PBSE_UNKJOBID;
    } 
  else if (isalpha((int)*name)) 
    {
    pque = find_queuebyname(name)	/* status jobs in a queue */;

    if (pque)
      type = 2;
    else
      rc = PBSE_UNKQUE;
    } 
  else if ((*name == '\0') || (*name == '@')) 
    {
    type = 3;	/* status all jobs at server */
    } 
  else
    {
    rc = PBSE_IVALREQ;
    }

  if (type == 0) 
    {		/* is invalid - an error */
    req_reject(rc,0,preq,NULL,NULL);

    return;
    }

  preq->rq_reply.brp_choice = BATCH_REPLY_CHOICE_Status;

  CLEAR_HEAD(preq->rq_reply.brp_un.brp_status);

  cntl = (struct stat_cntl *)malloc(sizeof(struct stat_cntl));

  if (cntl == NULL) 
    {
    req_reject(PBSE_SYSTEM,0,preq,NULL,NULL);

    return;
    }

  cntl->sc_type   = type;
  cntl->sc_conn   = -1;
  cntl->sc_pque   = pque;
  cntl->sc_origrq = preq;
  cntl->sc_post   = req_stat_job_step2;
  cntl->sc_jobid[0] = '\0';	/* cause "start from beginning" */

  if (server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long)
    cntl->sc_post = 0; /* we're not going to make clients wait */

  req_stat_job_step2(cntl); /* go to step 2, see if running is current */

  return;
  }  /* END req_stat_job() */





/*
 * req_stat_job_step2 - continue with statusing of jobs
 *	This is re-entered after sending status requests to MOM.
 *
 *	Note, the funny initization/advance of pjob in the "while" loop
 *	comes from the fact we want to look at the "next" job on re-entry.
 */

static void req_stat_job_step2(

  struct stat_cntl *cntl)  /* I/O */

  {
  svrattrl	       *pal;
  job		       *pjob;
  job                  *cpjob;

  struct batch_request *preq;
  struct batch_reply   *preply;
  int		        rc = 0;
  int                   type;

  preq = cntl->sc_origrq;
  type = cntl->sc_type;
  preply = &preq->rq_reply;

  /* See pbs_server_attributes(1B) for details on "poll_jobs" behaviour */

  if (!server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long)
    {
    if (cntl->sc_jobid[0] == '\0')
      pjob = NULL;
    else
      pjob = find_job(cntl->sc_jobid);

    while (1) 
      {
      if (pjob == NULL) 
        { 	
        /* start from the first job */

        if (type == 1)
          pjob = find_job(preq->rq_ind.rq_status.rq_id);
        else if (type == 2) 
          pjob = (job *)GET_NEXT(cntl->sc_pque->qu_jobs);	
        else
          pjob = (job *)GET_NEXT(svr_alljobs);
        } 
      else 
        {			
        /* get next job */

        if (type == 1)
          break;

        if (type == 2)
          pjob = (job *)GET_NEXT(pjob->ji_jobque);
        else
          pjob = (job *)GET_NEXT(pjob->ji_alljobs);
        }

      if (pjob == NULL)
        break;

      /* PBS_RESTAT_JOB defaults to 30 seconds */

      if ((pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING) &&
         ((time_now - pjob->ji_momstat) > JobStatRate)) 
        {
        /* go to MOM for status */

        strcpy(cntl->sc_jobid,pjob->ji_qs.ji_jobid);

        if ((rc = stat_to_mom(pjob,cntl)) == PBSE_SYSTEM) 
          {
          break;
          } 

        if (rc != 0) 
          {
          rc = 0;

          continue;
          } 

        return;	/* will pick up after mom replies */
        }
      }    /* END while(1) */

    if (cntl->sc_conn >= 0)
      svr_disconnect(cntl->sc_conn); 	/* close connection to MOM */

    if (rc) 
      {
      free(cntl);

      reply_free(preply);

      req_reject(rc,0,preq,NULL,NULL);

      return;
      }
    }    /* END if (!server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long) */

  /*
   * now ready for part 3, building the status reply,
   * loop through again
   */

  if (type == 1)
    pjob = find_job(preq->rq_ind.rq_status.rq_id);
  else if (type == 2) 
    pjob = (job *)GET_NEXT(cntl->sc_pque->qu_jobs);	
  else
    pjob = (job *)GET_NEXT(svr_alljobs);

  free(cntl);

  while (pjob != NULL) 
    {
    /* go ahead and build the status reply for this job */

    pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

    rc = status_job(pjob,preq,pal,&preply->brp_un.brp_status,&bad);

    if (rc && (rc != PBSE_PERM)) 
      {
      req_reject(rc,bad,preq,NULL,NULL);

      return;
      }

    /* get next job */

    if (type == 1)
      break;

    if (type == 2)
      pjob = (job *)GET_NEXT(pjob->ji_jobque);
    else
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);

    rc = 0;
    }  /* END while (pjob != NULL) */

  /* add completed jobs */

  cpjob = NULL;  /* disable completed job display for now */

  while (cpjob != NULL)
    {
    /* go ahead and build the status reply for this job */

    pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

    rc = status_job(pjob,preq,pal,&preply->brp_un.brp_status,&bad);

    if (rc && (rc != PBSE_PERM))
      {
      req_reject(rc,bad,preq,NULL,NULL);

      return;
      }

    /* get next job */

    if (type == 1)
      break;

    if (type == 2)
      pjob = (job *)GET_NEXT(pjob->ji_jobque);
    else
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);

    rc = 0;
    }  /* END while (cpjob != NULL) */

  reply_send(preq);

  return;
  }  /* END req_stat_job_step2() */





/*
 * stat_to_mom - send a Job Status to MOM to obtain latest status.
 *	Used by req_stat_job()/stat_step_2() 
 *
 * Returns PBSE_SYSTEM if out of memory, PBSE_NORELYMOM if the MOM
 * is down, offline, or deleted.  Otherwise returns result of MOM
 * contact request.
 */

int stat_to_mom(

  job              *pjob,
  struct stat_cntl *cntl)

  {
  struct batch_request *newrq;
  int		                rc;
  struct work_task     *pwt = 0;
  struct pbsnode *node;


  if ((newrq = alloc_br(PBS_BATCH_StatusJob)) == (struct batch_request *)0)
    {
    return(PBSE_SYSTEM);
    }

  /* set up status request, save address of cntl in request for later */

  newrq->rq_extra = (void *)cntl;

  if (cntl->sc_type == 1) 
    strcpy(newrq->rq_ind.rq_status.rq_id, pjob->ji_qs.ji_jobid);
  else
    newrq->rq_ind.rq_status.rq_id[0] = '\0';  /* get stat of all */

  CLEAR_HEAD(newrq->rq_ind.rq_status.rq_attr);

  /* if MOM is down just return stale information */

  if (((node = tfind_addr(pjob->ji_qs.ji_un.ji_exect.ji_momaddr)) != NULL) &&
       (node->nd_state & (INUSE_DELETED|INUSE_DOWN)))
    {
    return(PBSE_NORELYMOM);
    }

  /* get connection to MOM */

  cntl->sc_conn = svr_connect( 
    pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
    pbs_mom_port, 
    process_Dreply, 
    ToServerDIS);

  if ((rc = cntl->sc_conn) >= 0)
    rc = issue_Drequest(cntl->sc_conn,newrq,stat_update,&pwt);

  if (rc != 0) 
    {
    /* request failed */

    if (pwt)
      delete_task(pwt);

    free_br(newrq);

    if (cntl->sc_conn >= 0)
      svr_disconnect(cntl->sc_conn);
    }

  return(rc);
  }  /* END stat_to_mom() */





/*
 * stat_update - take reply to status request from MOM and update job status 
 */

static void stat_update(

  struct work_task *pwt)

  {
  struct stat_cntl     *cntl;
  job                  *pjob;
  struct batch_request *preq;
  struct batch_reply   *preply;
  struct brp_status    *pstatus;
  svrattrl	       *sattrl;

  preq = pwt->wt_parm1;
  preply = &preq->rq_reply;
  cntl = preq->rq_extra;

  if (preply->brp_choice == BATCH_REPLY_CHOICE_Status) 
    {
    pstatus = (struct brp_status *)GET_NEXT(preply->brp_un.brp_status);

    while (pstatus != NULL) 
      {
      if ((pjob = find_job(pstatus->brp_objname)))
        {
        sattrl = (svrattrl *)GET_NEXT(pstatus->brp_attr);

        modify_job_attr(
          pjob,
          sattrl,
          ATR_DFLAG_MGWR | ATR_DFLAG_SvWR,
          &bad);

        if (pjob->ji_momstat == 0)  
          {
          /* first save since running job, */
          /* must save session id	   */

          job_save(pjob,SAVEJOB_FULL);
          }

        pjob->ji_momstat = time_now;
        }

      pstatus = (struct brp_status *)GET_NEXT(pstatus->brp_stlink);
      }  /* END while (pstatus) */
    }    /* END if (preply->brp_choice == BATCH_REPLY_CHOICE_Status) */

  release_req(pwt);

  cntl->sc_conn = -1;

  if (cntl->sc_post)
    cntl->sc_post(cntl);	/* continue where we left off */
  else
    free(cntl);	/* a bit of a kludge but its saves an extra func */

  return;
  }  /* END stat_update() */




	
/*
 * stat_mom_job - status a single job running under a MOM
 *	This is used after sending a job to mom to get the session id
 */

void stat_mom_job(

  job *pjob)

  {
  struct stat_cntl *cntl;

  cntl = (struct stat_cntl *)malloc(sizeof(struct stat_cntl));

  if (cntl == NULL)
    {
    return;
    }

  cntl->sc_type   = 1;
  cntl->sc_conn   = -1;
  cntl->sc_pque   = (pbs_queue *)0;
  cntl->sc_origrq = (struct batch_request *)0;
  cntl->sc_post   = 0;		/* tell stat_update() to free cntl */
  cntl->sc_jobid[0] = '\0';	/* cause "start from beginning" */

  if (stat_to_mom(pjob,cntl) != 0) 
    {
    free(cntl);
    }

  /* if not an error, cntl freed in stat_update() */

  return;
  }  /* END stat_mom_job() */




void poll_job_task( 

  struct work_task *ptask) 

  {

  job *pjob;
  long when;

  pjob = (job *)ptask->wt_parm1;

  if (pjob == NULL)
    {
    /* FAILURE */

    return;
    }

  if (server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long && 
     (pjob->ji_qs.ji_state == JOB_STATE_RUNNING))
    {
    stat_mom_job(pjob);
    }

  return;
  }  /* END poll_job_task() */




/*
 * req_stat_que - service the Status Queue Request 
 *
 *	This request processes the request for status of a single queue or
 *	the set of queues at a destinaion.
 */

void req_stat_que(

  struct batch_request *preq)	/* ptr to the decoded request   */

  {
  char		   *name;
  pbs_queue	   *pque = NULL;
  struct batch_reply *preply;
  int		    rc   = 0;
  int		    type = 0;

  /*
   * first, validate the name of the requested object, either
   * a queue, or null for all queues
   */

  name = preq->rq_ind.rq_status.rq_id;

  if ((*name == '\0') || (*name == '@'))
    {
    type = 1;
    }
  else 
    {
    pque = find_queuebyname(name);

    if (pque == NULL) 
      {
      req_reject(PBSE_UNKQUE,0,preq,NULL,NULL);

      return;
      }
    }

  preply = &preq->rq_reply;
  preply->brp_choice = BATCH_REPLY_CHOICE_Status;

  CLEAR_HEAD(preply->brp_un.brp_status);

  if (type == 0)  
    {
    /* get status of the named queue */

    rc = status_que(pque,preq,&preply->brp_un.brp_status);
    } 
  else 
    {	
    /* get status of all queues */

    pque = (pbs_queue *)GET_NEXT(svr_queues);

    while (pque != NULL) 
      {
      rc = status_que(pque,preq,&preply->brp_un.brp_status);

      if (rc != 0)
        {
        if (rc != PBSE_PERM)
          break;

        rc = 0;
        }

      pque = (pbs_queue *)GET_NEXT(pque->qu_link);
      }
    }

  if (rc != 0) 
    {
    reply_free(preply);

    req_reject(rc,bad,preq,NULL,NULL);
    } 
  else 
    {
    reply_send(preq);
    }

  return;
  }  /* END req_stat_que() */





/*
 * status_que - Build the status reply for a single queue.
 */

static int status_que(

  pbs_queue *pque,	/* ptr to que to status */
  struct batch_request *preq,
  list_head *pstathd)	/* head of list to append status to */

  {
  struct brp_status *pstat;
  svrattrl          *pal;

  if ((preq->rq_perm & ATR_DFLAG_RDACC) == 0)
    {
    return(PBSE_PERM);
    }

  /* ok going to do status, update count and state counts from qu_qs */

  pque->qu_attr[(int)QA_ATR_TotalJobs].at_val.at_long = pque->qu_numjobs;
  pque->qu_attr[(int)QA_ATR_TotalJobs].at_flags |= ATR_VFLAG_SET;

  update_state_ct(
    &pque->qu_attr[(int)QA_ATR_JobsByState],
    pque->qu_njstate,
    pque->qu_jobstbuf);

  /* allocate status sub-structure and fill in header portion */
	
  pstat = (struct brp_status *)malloc(sizeof(struct brp_status));

  if (pstat == NULL)
    {
    return(PBSE_SYSTEM);
    }

  pstat->brp_objtype = MGR_OBJ_QUEUE;

  strcpy(pstat->brp_objname,pque->qu_qs.qu_name);

  CLEAR_LINK(pstat->brp_stlink);
  CLEAR_HEAD(pstat->brp_attr);

  append_link(pstathd,&pstat->brp_stlink,pstat);

  /* add attributes to the status reply */

  bad = 0;

  pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

  if (status_attrib(
       pal,
       que_attr_def,
       pque->qu_attr,
       QA_ATR_LAST,
       preq->rq_perm, 
       &pstat->brp_attr, 
       &bad,
       1) != 0)   /* IsOwner == TRUE */
    {
    return(PBSE_NOATTR);
    }

  return(0);
  }  /* END stat_que() */






/*
 * req_stat_node - service the Status Node Request 
 *
 *	This request processes the request for status of a single node or
 *	set of nodes at a destinaion.
 */

void req_stat_node(

  struct batch_request *preq)	/* ptr to the decoded request   */

  {
  char	   *name;
  struct pbsnode *pnode = NULL;
  struct batch_reply *preply;
  svrattrl *pal;
  int	    rc   = 0;
  int	    type = 0;
  int	    i;

  /*
   * first, check that the server indeed has a list of nodes
   * and if it does, validate the name of the requested object--
   * either name is that of a specific node, or name[0] is null/@
   * meaning request is for all nodes in the server's jurisdiction
   */

  if ((pbsndmast == 0) || (svr_totnodes <= 0)) 
    {
    req_reject(PBSE_NONODES,0,preq,NULL,NULL);

    return;
    }

  name = preq->rq_ind.rq_status.rq_id;

  if ((*name == '\0') || (*name == '@'))
    {
    type = 1;
    }
  else 
    {
    pnode = find_nodebyname(name);

    if (pnode == NULL) 
      {
      req_reject(PBSE_UNKNODE,0,preq,NULL,NULL);

      return;
      }
    }

  preply = &preq->rq_reply;
  preply->brp_choice = BATCH_REPLY_CHOICE_Status;

  CLEAR_HEAD(preply->brp_un.brp_status);

  if (type == 0) 
    {		/*get status of the named node*/
    rc = status_node(pnode,preq,&preply->brp_un.brp_status);
    } 
  else 
    {			/*get status of all nodes     */
    for (i = 0;i < svr_totnodes;i++) 
      {
      pnode = pbsndmast[i];

      if ((rc = status_node(pnode,preq,&preply->brp_un.brp_status)) != 0)
        break;
      }
    }

  if (!rc) 
    {
    /* SUCCESS */

    reply_send(preq);
    } 
  else 
    {
    if (rc != PBSE_UNKNODEATR)
      {
      req_reject(rc,0,preq,NULL,NULL);
      }
    else 
      {
      pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

      reply_badattr(rc,bad,pal,preq);
      }
    }

  return;
  }  /* END req_stat_node() */





/*
 * status_node - Build the status reply for a single node.
 */

static int status_node(

  struct pbsnode       *pnode,    /* ptr to node receiving status query */
  struct batch_request *preq,
  list_head            *pstathd)  /* head of list to append status to  */

  {
  int		     rc = 0;
  struct brp_status *pstat;
  svrattrl          *pal;

  if (pnode->nd_state & INUSE_DELETED)  /*node no longer valid*/
    {
    return(0);
    }

  if ((preq->rq_perm & ATR_DFLAG_RDACC) == 0)
    {
    return(PBSE_PERM);
    }

  /* allocate status sub-structure and fill in header portion */
	
  pstat = (struct brp_status *)malloc(sizeof (struct brp_status));

  if (pstat == (struct brp_status *)0)
    {
    return(PBSE_SYSTEM);
    }

  pstat->brp_objtype = MGR_OBJ_NODE;

  strcpy(pstat->brp_objname,pnode->nd_name);

  CLEAR_LINK(pstat->brp_stlink);
  CLEAR_HEAD(pstat->brp_attr);

  /*add this new brp_status structure to the list hanging off*/
  /*the request's reply substructure                         */

  append_link(pstathd, &pstat->brp_stlink, pstat);

  /*point to the list of node-attributes about which we want status*/
  /*hang that status information from the brp_attr field for this  */
  /*brp_status structure                                           */

  bad = 0;                                    /*global variable*/

  pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

  rc = status_nodeattrib(
    pal, 
    node_attr_def, 
    pnode, 
    ND_ATR_LAST,
    preq->rq_perm, 
    &pstat->brp_attr, 
    &bad);

  return(rc);
  }  /* END status_node() */





/*
 * req_stat_svr - service the Status Server Request 
 *
 *	This request processes the request for status of the Server
 */

void req_stat_svr(

  struct batch_request *preq)	/* ptr to the decoded request */

  {
  svrattrl	   *pal;
  struct batch_reply *preply;
  struct brp_status  *pstat;

  /* update count and state counts from sv_numjobs and sv_jobstates */

  server.sv_attr[(int)SRV_ATR_TotalJobs].at_val.at_long = server.sv_qs.sv_numjobs;
  server.sv_attr[(int)SRV_ATR_TotalJobs].at_flags |= ATR_VFLAG_SET;

  update_state_ct(
    &server.sv_attr[(int)SRV_ATR_JobsByState],
    server.sv_jobstates,
    server.sv_jobstbuf);

  /* allocate a reply structure and a status sub-structure */

  preply = &preq->rq_reply;
  preply->brp_choice = BATCH_REPLY_CHOICE_Status;

  CLEAR_HEAD(preply->brp_un.brp_status);

  pstat = (struct brp_status *)malloc(sizeof(struct brp_status));

  if (pstat == NULL) 
    {
    reply_free(preply);

    req_reject(PBSE_SYSTEM,0,preq,NULL,NULL);

    return;
    }

  CLEAR_LINK(pstat->brp_stlink);

  strcpy(pstat->brp_objname,server_name);

  pstat->brp_objtype = MGR_OBJ_SERVER;

  CLEAR_HEAD(pstat->brp_attr);

  append_link(&preply->brp_un.brp_status,&pstat->brp_stlink,pstat);

  /* add attributes to the status reply */

  bad = 0;

  pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_status.rq_attr);

  if (status_attrib(
        pal, 
        svr_attr_def, 
        server.sv_attr, 
        SRV_ATR_LAST,
        preq->rq_perm, 
        &pstat->brp_attr, 
        &bad,
        1))    /* IsOwner == TRUE */
    {
    reply_badattr(PBSE_NOATTR,bad,pal,preq);
    }
  else
    {
    reply_send(preq);
    }

  return;
  }  /* END req_stat_svr() */





/*
 * update-state_ct - update the count of jobs per state (in queue and server
 *	attributes.
 */

static void update_state_ct(pattr, ct_array, buf)
	attribute     *pattr;
	int	      *ct_array;
	char	      *buf;
{
	static char *statename[] = {	"Transit", "Queued", "Held",
					"Waiting", "Running", "Exiting" };
	int  index;

	buf[0] = '\0';
	for (index=0; index<PBS_NUMJOBSTATE; index++) {
		sprintf(buf+strlen(buf), "%s:%d ", statename[index],
			*(ct_array + index));
	}
	pattr->at_val.at_str = buf;
	pattr->at_flags |= ATR_VFLAG_SET;
}
