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
 * Functions relating to the Select Job Batch Request and the Select-Status
 * (SelStat) Batch Request.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#define STAT_CNTL 1

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "libpbs.h"
#include <string.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "queue.h"
#include "job.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"

/* Private Data */

/* Extenal functions called */

extern int   status_job A_((job *, struct batch_request *, svrattrl *, tlist_head *, int *));
extern int   svr_authorize_jobreq A_((struct batch_request *,job *));


/* Global Data Items  */

extern struct server server;
extern int	 resc_access_perm;
extern tlist_head svr_alljobs;
extern time_t	 time_now;

/* Private Functions  */

static int  build_selist A_((svrattrl *, int perm, struct  select_list **,
			     pbs_queue **, int *bad));
static void free_sellist A_((struct select_list *pslist));
static int  sel_attr A_((attribute *, struct select_list *));
static int  select_job A_((job *, struct select_list *));
static void sel_step2 A_((struct stat_cntl *));
static void sel_step3 A_((struct stat_cntl *));


/*
 * order_checkpoint - provide order value for various checkpoint attribute values
 *	n > s > c=minutes > c 
 */

static int order_checkpoint(attr)
	attribute *attr;
{
	if ( ((attr->at_flags & ATR_VFLAG_SET) == 0) || 
	     (attr->at_val.at_str == 0) )
		return 0;

	switch (*attr->at_val.at_str) {
	    case 'n':	return 5;
	    case 's':	return 4;
	    case 'c':	if (*(attr->at_val.at_str+1) != '\0')
				return 3;
			else
				return 2;
	    case 'u':	return 1;
	    default:	return 0;
	}
}

/*
 * comp_checkpoint - compare two checkpoint attribtues for selection 
 */

int  comp_checkpoint(attr, with)
	attribute *attr;
	attribute *with;
{
	int a;
	int w;

	a = order_checkpoint(attr);
	w = order_checkpoint(with);

	if (a == w)
		return 0;
	else if (a > w)
		return 1;
	else
		return -1;
}
static int  comp_state (state, selstate)
	attribute *state;
	attribute *selstate;
{
	char *ps;

	if ( !state || !selstate || !selstate->at_val.at_str)
		return (-1);

	for (ps = selstate->at_val.at_str; *ps; ++ps) {
		if (*ps == state->at_val.at_char)
			return (0);
	}
	return (1);
}

static attribute_def state_sel = {
  ATTR_state,
  decode_str,
  encode_str,
  set_str,
  comp_state,
  free_str,
  NULL_FUNC,
  READ_ONLY,
  ATR_TYPE_STR,
  PARENT_TYPE_JOB };


/*
 * req_selectjobs - service both the Select Job Request and the (special
 *	for the scheduler) Select-status Job Request
 *
 *	This request select jobs based on a supplied criteria and returns
 *	Select   - a list of the job identifiers which meet the criteria
 *	Sel_stat - a list of the status of the jobs that meet the criteria 
 *
 *	For Select, one pass through the job list suffices.
 *
 *	For Sel_stat, the current status from MOM for running jobs is required
 *	(just like for regular status requests).  Therefore, two passes are
 *	made through the jobs:
 *		1. Determine if any job that qualifies is running and has
 *		   stale data from MOM.   If so get new from MOM.
 *		2. Build the status reply for any job that qualifies.
 *	
 *	And just like regular status requests, if poll_job is enabled,
 *	we skip over sending update requests to MOM.
 */

void req_selectjobs(

  struct batch_request *preq)

  {
  int		    bad = 0;
  struct stat_cntl *cntl;
  svrattrl	   *plist;
  pbs_queue	   *pque;
  int		    rc;
  struct select_list *selistp;

  plist = (svrattrl *)GET_NEXT(preq->rq_ind.rq_select);

  rc = build_selist(plist,preq->rq_perm,&selistp,&pque,&bad);

  if (rc != 0) 
    {
    reply_badattr(rc,bad,plist,preq);

    free_sellist(selistp);

    return;
    }

  if ((cntl = (struct stat_cntl *)malloc(sizeof(struct stat_cntl))) == NULL) 
    {
    free_sellist(selistp);

    req_reject(PBSE_SYSTEM,0,preq,NULL,NULL);

    return;
    }

  if (pque != NULL)
    cntl->sc_type = 2;     /* 2: all jobs in a queue, see sc_pque */
  else
    cntl->sc_type = 3;     /* 3: all jobs in the server */

  cntl->sc_conn = -1;	      /* no connection (yet) to mom */
  cntl->sc_pque = pque;	      /* queue or null */
  cntl->sc_origrq = preq;     /* the original request */
  cntl->sc_jobid[0] = '\0';   /* null job id, start from the top */
  cntl->sc_select = selistp;  /* the select list */

  if (preq->rq_type == PBS_BATCH_SelectJobs) 
    {
    sel_step3(cntl);
    } 
  else if (server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long) 
    {
    sel_step3(cntl);
    }
  else 
    {
    cntl->sc_post   = sel_step2;
    sel_step2(cntl);
    }

  return;
  }  /* END req_selectjobs() */





static void sel_step2(

  struct stat_cntl *cntl)

  {
  job *pjob;
  int  rc;
  int		      exec_only = 0;
  pbs_queue           *pque = NULL;

  /* do first pass of finding jobs that match the selection criteria */

  if (cntl->sc_jobid[0] == '\0')
    pjob = NULL;
  else
    pjob = find_job(cntl->sc_jobid);

  if (cntl->sc_origrq->rq_extend != NULL)
    if (!strncmp(cntl->sc_origrq->rq_extend,EXECQUEONLY,strlen(EXECQUEONLY)))
      exec_only = 1;

  while (1) 
    {
    /*
     * Start with either first job in queue or server, or
     * start with the job following the current one in the the
     * list (queue or server).
     */

    if (pjob == NULL) 
      {
      if (cntl->sc_pque) 
        pjob = (job *)GET_NEXT(cntl->sc_pque->qu_jobs);
      else
        pjob = (job *)GET_NEXT(svr_alljobs);
      } 
    else if (cntl->sc_pque)  
      {
      pjob = (job *)GET_NEXT(pjob->ji_jobque);
      } 
    else 
      {
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }
	
    if (pjob == NULL)
      break;

    if (exec_only)
      {
      pque=find_queuebyname(pjob->ji_qs.ji_queue);

      if (pque->qu_qs.qu_type != QTYPE_Execution)
         continue;
      }

    if (server.sv_attr[(int)SRV_ATR_query_others].at_val.at_long ||
       (svr_authorize_jobreq(cntl->sc_origrq,pjob) == 0)) 
      {
      /* have permission to look at job */

      if (select_job(pjob,cntl->sc_select)) 
        {
        if ((pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING) &&
           ((time_now - pjob->ji_momstat) > JobStatRate)) 
          {
          /* need to ask MOM for new status */

          strcpy(cntl->sc_jobid,pjob->ji_qs.ji_jobid);

          if ((rc = stat_to_mom(pjob,cntl)) == PBSE_SYSTEM) 
            {
            break;
            } 
 
          if (rc == 0) 
            {
            return;
            } 

          rc = 0;

          /* ignore this job */

          continue;
          }
        }
      }
    }

  sel_step3(cntl);

  return;
  }  /* END sel_step2() */




static void sel_step3(

  struct stat_cntl *cntl)

  {
  int		      bad = 0;
  job		     *pjob;
  struct batch_request *preq;
  struct batch_reply   *preply;
  struct brp_select    *pselect;
  struct brp_select   **pselx;
  int		      rc = 0;
  int		      exec_only = 0;
  pbs_queue           *pque = NULL;

  /* setup the appropriate return */

  preq = cntl->sc_origrq;
  preply = &preq->rq_reply;

  if (preq->rq_type == PBS_BATCH_SelectJobs) 
    {
    preply->brp_choice = BATCH_REPLY_CHOICE_Select;
    preply->brp_un.brp_select = (struct brp_select *)0;
    } 
  else 
    {
    preply->brp_choice = BATCH_REPLY_CHOICE_Status;
    CLEAR_HEAD(preply->brp_un.brp_status);
    }

  pselx = &preply->brp_un.brp_select;

  if (preq->rq_extend != NULL)
    if (!strncmp(preq->rq_extend,EXECQUEONLY,strlen(EXECQUEONLY)))
      exec_only = 1;

  /* now start checking for jobs that match the selection criteria */

  if (cntl->sc_pque) 
    pjob = (job *)GET_NEXT(cntl->sc_pque->qu_jobs);
  else
    pjob = (job *)GET_NEXT(svr_alljobs);

  while (pjob != NULL) 
    {
    if (server.sv_attr[(int)SRV_ATR_query_others].at_val.at_long ||
        (svr_authorize_jobreq(preq,pjob) == 0) ) 
      {
      /* either job owner or has special permission to look at job */

      if (exec_only)
        {
        pque=find_queuebyname(pjob->ji_qs.ji_queue);

        if (pque->qu_qs.qu_type != QTYPE_Execution)
           goto nextjob;
        }

      if (select_job(pjob,cntl->sc_select))  
        {
        /* job is selected, include in reply */

        if (preq->rq_type == PBS_BATCH_SelectJobs) 
          {
          /* Select Jobs */

	  pselect = malloc(sizeof (struct brp_select));

	  if (pselect == NULL)
            {
	    rc = PBSE_SYSTEM;
	    break;
	    }
	  pselect->brp_next = NULL;

	  strcpy(pselect->brp_jobid,pjob->ji_qs.ji_jobid);
	  *pselx = pselect;
	  pselx = &pselect->brp_next;
	  preq->rq_reply.brp_auxcode++;

	  }
        else
          {
	  /* Select-Status */

	  rc = status_job(pjob,preq,NULL,&preply->brp_un.brp_status,&bad);
          if (rc && (rc != PBSE_PERM))
	    break;
          }
        }
      }

nextjob:

    if (cntl->sc_pque)
      pjob = (job *)GET_NEXT(pjob->ji_jobque);
    else
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
  }

  free_sellist(cntl->sc_select);

  free(cntl);

  if (rc)
    req_reject(rc,0,preq,NULL,NULL);
  else
    reply_send(preq);

  return;
  }  /* END sel_step3() */





/*
 * select_job - determine if a single job matches the selection criteria
 *
 *	Returns: 1 if matches, 0 if not
 */

static int select_job(

  job                *pjob,
  struct select_list *psel)

  {
  while (psel != NULL) 
    {
    if (psel->sl_atindx == (int)JOB_ATR_userlst) 
      {
      if (!acl_check(
            &psel->sl_attr, 
            pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str, 
            ACL_User))
        {
        /* no match */

        return(0);
        }
      } 
    else 
      {
      if (!sel_attr(&pjob->ji_wattr[psel->sl_atindx],psel))
        {
        /* no match */

        return(0);
        }
      }

    psel = psel->sl_next;
    }

  return(1);
  }





/*
 * sel_attr - determine if attribute is according to the selection operator
 *
 *	Returns 1 if attribute meets criteria, 0 if not
 */

static int sel_attr (jobat, pselst)
	attribute	   *jobat;
	struct select_list *pselst;
{
	int	   rc;
	resource  *rescjb;
	resource  *rescsl;

	if (pselst->sl_atindx == JOB_ATR_resource) {

		/* Only one resource per selection entry, 		*/
		/* find matching resource in job attribute if one	*/

		rescsl = (resource *)GET_NEXT(pselst->sl_attr.at_val.at_list);
		rescjb = find_resc_entry(jobat, rescsl->rs_defin);
	
		if (rescjb && (rescjb->rs_value.at_flags & ATR_VFLAG_SET))
			/* found match, compare them */
			rc = pselst->sl_def->at_comp(&rescjb->rs_value, &rescsl->rs_value);
		else		/* not one in job,  force to .lt. */
			rc = -1;

	} else {
		/* "normal" attribute */

		rc = pselst->sl_def->at_comp(jobat, &pselst->sl_attr);
	}

	if (rc < 0) {
		if ((pselst->sl_op == NE) ||
		    (pselst->sl_op == LT) ||
		    (pselst->sl_op == LE))
			return (1);

	} else if (rc > 0) {
		if ((pselst->sl_op == NE) ||
		    (pselst->sl_op == GT) ||
		    (pselst->sl_op == GE))
			return (1);

	} else {	/* rc == 0 */
		if ((pselst->sl_op == EQ) ||
		    (pselst->sl_op == GE) ||
		    (pselst->sl_op == LE))
			return (1);
	}
	return (0);
}

/*
 * free_sellist - free a select_list list
 */

static void free_sellist(pslist)
	struct select_list *pslist;
{
	struct select_list *next;

	while (pslist) {
		next = pslist->sl_next;
		pslist->sl_def->at_free(&pslist->sl_attr); /* free the attr */
		(void)free(pslist);			  /* free the entry */
		pslist = next;
	}
}





/*
 * build_selentry - build a single entry for a select list
 *
 *	returns the pointer to the entry into the third argument
 *	function return is 0 for ok, or an error code
 */

static int build_selentry(

  svrattrl            *plist,
  attribute_def	      *pdef,
  int		       perm,
  struct select_list **rtnentry) /* O */

  {
  struct select_list *entry;
  int                 rc;

  /* create a select list entry for this attribute */

  entry = (struct select_list *)malloc(sizeof(struct select_list));

  if (entry == NULL)
    {
    return(PBSE_SYSTEM);
    }

  entry->sl_next = NULL;

  clear_attr(&entry->sl_attr,pdef);

  if (!(pdef->at_flags & ATR_DFLAG_RDACC & perm)) 
    {
    free(entry);

    return(PBSE_PERM);    /* no read permission */
    }

  if ((pdef->at_flags & ATR_DFLAG_SELEQ) &&
      (plist->al_op != EQ) &&
      (plist->al_op != NE)) 
    {
    /* can only select eq/ne on this attribute */

    free(entry);

    log_err(-1,"build_selentry","cannot select attribute");

    return(PBSE_IVALREQ);
    }

  /* decode the attribute into the entry */

  if ((rc = pdef->at_decode(
             &entry->sl_attr,
             plist->al_name,
             plist->al_resc,
             plist->al_value)))
    {
    free(entry);

    return(rc);
    }

  if ((entry->sl_attr.at_flags & ATR_VFLAG_SET) == 0) 
    {
    free(entry);

    return(PBSE_BADATVAL);
    }

  /*
   * save the pointer to the attribute definition,
   * if a resource, use the resource specific one
   */

			if (pdef == &job_attr_def[(int)JOB_ATR_resource]) {
			    entry->sl_def = (attribute_def *)find_resc_def(
							     svr_resc_def,
							     plist->al_resc,
							     svr_resc_size );
				if (!entry->sl_def) {
					(void)free(entry);
					return (PBSE_UNKRESC);
				}
			} else
				entry->sl_def = pdef;

			/* save the selection operator to pass along */

			entry->sl_op = plist->al_op;

			*rtnentry = entry;
			return (0);
}






/*
 * build_selist - build the list of select_list structures based on
 *	the svrattrl structures in the request.
 *
 *	Function returns non-zero on an error, also returns into last
 *	three of the parameter list.
 */

static int build_selist(

  svrattrl	    *plist,
  int		     perm,
  struct select_list **pselist,	/* RETURN - select list */
  pbs_queue	   **pque,	/* RETURN - queue ptr if limit to que */
  int		    *bad)	/* RETURN - index of bad attr */

  {
	struct select_list *entry;
	int		    i;
	char		   *pc;
	attribute_def	   *pdef;
	struct select_list *prior = (struct select_list *)0;
	int		    rc;

	/* set permission for decode_resc() */

	resc_access_perm = perm;

	*pque = (pbs_queue *)0;
	*bad = 0;
	*pselist = (struct select_list *)0;
	while (plist) {
		(*bad)++;	/* list counter incase one is bad */
		
		/* go for all job unless a "destination" other than */
		/* "@server" is specified			    */
	
		if (!strcmp(plist->al_name, ATTR_q)) {
		    if (plist->al_valln) {
			if ( ((pc = strchr(plist->al_value, (int)'@')) == 0) ||
			     (pc != plist->al_value) ) {

				/* does specified destination exist? */

				*pque = find_queuebyname(plist->al_value);
				if (*pque == (pbs_queue *)0)
					return (PBSE_UNKQUE);
			}
		    }
		} else {
			i = find_attr(job_attr_def,plist->al_name,JOB_ATR_LAST);
			if (i < 0)
				return (PBSE_NOATTR);   /* no such attribute */

			if (i == JOB_ATR_state) {
				pdef = &state_sel;
			} else {
				pdef = job_attr_def + i;
			}

			/* create a select list entry for this attribute */

			if ((rc = build_selentry(plist, pdef, perm, &entry)))
				return (rc);
			entry->sl_atindx = i;

			/* add the entry to the select list */

			if (prior)
				prior->sl_next = entry;    /* link into list */
			else
				*pselist = entry;    /* return start of list */
			prior = entry;
		}
		plist = (svrattrl *)GET_NEXT(plist->al_link);
	}

  return(0);
  }  /* END build_selist() */

/* END req_select.c */


