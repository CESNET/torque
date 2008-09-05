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
 * req_runjob.c - functions dealing with a Run Job Request
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "job.h"
#include "queue.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "acct.h"
#include "svrfunc.h"
#include "net_connect.h"
#include "pbs_proto.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif  /* HAVE_NETINET_IN_H */

/* External Functions Called: */

extern int                   send_job A_((job *,pbs_net_t,int,int,void (*x)(),struct batch_request *));
extern void                  set_resc_assigned A_((job *, enum batch_op));
extern struct batch_request *cpy_stage A_((struct batch_request *,job *,enum job_atr,int));
void                         stream_eof A_((int,u_long,int));
extern int                   job_set_wait(attribute *,void *,int);
extern void                  stat_mom_job A_((job *));

extern int LOGLEVEL;  

/* Public Functions in this file */

int  svr_startjob A_((job *,struct batch_request *,char *,char *));

/* Private Functions local to this file */

static void post_sendmom A_((struct work_task *));
static int  svr_stagein A_((job *,struct batch_request *,int,int)); 
static int  svr_strtjob2 A_((job *,struct batch_request *));
static job *chk_job_torun A_((struct batch_request *,int));
static int  assign_hosts A_((job *,char *,int,char *,char *));

/* Global Data Items: */

extern pbs_net_t pbs_mom_addr;
extern int	 pbs_mom_port;
extern struct server server;
extern char  server_host[PBS_MAXHOSTNAME + 1];
extern char  server_name[PBS_MAXSERVERNAME + 1];
extern char *msg_badexit;
extern char *msg_jobrun;
extern char *msg_manager;
extern char *msg_stageinfail;
extern int   scheduler_jobct;
extern int   scheduler_sock;
extern time_t time_now;
extern int   svr_totnodes;	/* non-zero if using nodes */
extern int   svr_tsnodes;	/* non-zero if time-shared nodes */

extern const char   *PJobSubState[];
extern unsigned int  pbs_rm_port;
extern char         *msg_err_malloc;

long  DispatchTime[20];
job  *DispatchJob[20];
char *DispatchNode[20];



/*
 * req_runjob - service the Run Job and Async Run Job Requests
 *
 *	This request forces a job into execution.  Client must be privileged.
 */

void req_runjob(

  struct batch_request *preq)  /* I (modified) */

  {
  job  *pjob;
  int   rc;
  void *bp;

  int   setneednodes;

  char  failhost[1024];
  char  emsg[1024];

  /* chk_job_torun will extract job id and assign hostlist if specified */

  if (getenv("TORQUEAUTONN"))
    setneednodes = 1;
  else
    setneednodes = 0;

  if ((pjob = chk_job_torun(preq,setneednodes)) == NULL)
    {
    /* FAILURE - chk_job_torun performs req_reject internally */

    return;
    }

  if (preq->rq_conn == scheduler_sock)
    ++scheduler_jobct;	/* see scheduler_close() */

  sprintf(log_buffer,msg_manager,
    msg_jobrun, 
    preq->rq_user, 
    preq->rq_host);

  log_event(
    PBSEVENT_JOB, 
    PBS_EVENTCLASS_JOB, 
    pjob->ji_qs.ji_jobid, 
    log_buffer);

  /* If async run, reply now; otherwise reply is handled in */
  /* post_sendmom or post_stagein */

  /* perhaps node assignment should be handled immediately in async run? */

  if ((preq != NULL) && 
      (preq->rq_type == PBS_BATCH_AsyrunJob)) 
    {
    reply_ack(preq);

    preq = NULL;  /* cleared so we don't try to reuse */
    }

  /* NOTE:  nodes assigned to job in svr_startjob() */

  rc = svr_startjob(pjob,preq,failhost,emsg);

  if ((rc != 0) && (preq != NULL)) 
    {
    free_nodes(pjob);

    /* if the job has a non-empty rejectdest list, pass the first host into req_reject() */

    if ((bp = GET_NEXT(pjob->ji_rejectdest)) != NULL)
      {
      req_reject(rc,0,preq,((struct badplace *)bp)->bp_dest,"could not contact host");
      }
    else
      {
      req_reject(rc,0,preq,failhost,emsg);
      }
    }

  return;
  }  /* END req_runjob() */





/*
 * req_stagein - service the Stage In Files for a Job Request
 *
 *	This request causes MOM to start staging in files. 
 *	Client must be privileged.
 */

void req_stagein(

  struct batch_request *preq)  /* I */

  {
  job *pjob;
  int  rc;

  int  setneednodes;

  if (getenv("TORQUEAUTONN"))
    setneednodes = 1;
  else
    setneednodes = 0;

  if ((pjob = chk_job_torun(preq,setneednodes)) == NULL) 
    {
    return;
    } 

  if ((pjob->ji_wattr[(int)JOB_ATR_stagein].at_flags & ATR_VFLAG_SET) == 0) 
    {
    log_err(-1,"req_stagein","stage-in information not set");

    req_reject(PBSE_IVALREQ,0,preq,NULL,NULL);

    return;
    } 

  if ((rc = svr_stagein(
      pjob, 
      preq,
      JOB_STATE_QUEUED, 
      JOB_SUBSTATE_STAGEIN)) )
    {
    free_nodes(pjob);

    req_reject(rc,0,preq,NULL,NULL);
    }

  return;
  }  /* END req_stagein() */





/*
 * post_stagein - process reply from MOM to stage-in request
 */

static void post_stagein(

  struct work_task *pwt)

  {
  int		      code;
  int		      newstate;
  int		      newsub;
  job		     *pjob;
  struct batch_request *preq;
  attribute	     *pwait;

  preq = pwt->wt_parm1;
  code = preq->rq_reply.brp_code;
  pjob = find_job(preq->rq_extra);

  free(preq->rq_extra);

  if (pjob != NULL) 
    {
    if (code != 0) 
      {
      /* stage in failed - hold job */

      free_nodes(pjob);

      pwait = &pjob->ji_wattr[(int)JOB_ATR_exectime];

      if ((pwait->at_flags & ATR_VFLAG_SET) == 0) 
        {
        pwait->at_val.at_long = time_now + PBS_STAGEFAIL_WAIT;

        pwait->at_flags |= ATR_VFLAG_SET;

        job_set_wait(pwait,pjob,0);
        }

      svr_setjobstate(pjob,JOB_STATE_WAITING,JOB_SUBSTATE_STAGEFAIL);

      if (preq->rq_reply.brp_choice == BATCH_REPLY_CHOICE_Text)
        {
        /* set job comment */

        /* NYI */

        svr_mailowner(
          pjob, 
          MAIL_STAGEIN, 
          MAIL_FORCE,
          preq->rq_reply.brp_un.brp_txt.brp_str);
        }
      } 
    else 
      {
      /* stage in was successful */

      pjob->ji_qs.ji_svrflags |= JOB_SVFLG_StagedIn;

      if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEGO) 
        {
        /* continue to start job running */

        svr_strtjob2(pjob,NULL);
        } 
      else 
        {
        svr_evaljobstate(pjob,&newstate,&newsub,0);

        svr_setjobstate(pjob,newstate,newsub);
        }
      }
    }    /* END if (pjob != NULL) */

  release_req(pwt);	/* close connection and release request */

  return;	
  }  /* END post_stagein() */





/*
 * svr_stagein - direct MOM to stage in the requested files for a job
 */

static int svr_stagein(

  job                  *pjob,     /* I */
  struct batch_request *preq,     /* I */
  int                   state,    /* I */
  int                   substate) /* I */

  {
  struct batch_request *momreq = 0;
  int		      rc;

  momreq = cpy_stage(momreq,pjob,JOB_ATR_stagein,STAGE_DIR_IN);

  if (momreq == NULL) 
    {	
    /* no files to stage, go directly to sending job to mom */

    return(svr_strtjob2(pjob,preq));
    }

  /* have files to stage in */

  /* save job id for post_stagein */

  momreq->rq_extra = malloc(PBS_MAXSVRJOBID + 1);

  if (momreq->rq_extra == 0) 
    {
    return(PBSE_SYSTEM);
    }

  strcpy(momreq->rq_extra, pjob->ji_qs.ji_jobid);

  rc = relay_to_mom(
    pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
    momreq, 
    post_stagein);

  if (rc == 0) 
    {
    svr_setjobstate(pjob,state,substate);

    /*
     * stage-in started ok - reply to client as copy may
     * take too long to wait.
     */

    if (preq != NULL)
      reply_ack(preq);
    } 
  else 
    {
    free(momreq->rq_extra);
    }

  return(rc);
  }  /* END svr_stagein() */





/*
 * svr_startjob - place a job into running state by shipping it to MOM
 *   called by req_runjob() 
 */

int svr_startjob(

  job                  *pjob,     /* I job to run (modified) */
  struct batch_request *preq,     /* I Run Job batch request (optional) */
  char                 *FailHost, /* O (optional,minsize=1024) */
  char                 *EMsg)     /* O (optional,minsize=1024) */

  {
  int     f;
  int     rc;
#ifdef BOEING
  int     sock, nodenum;
  struct  hostent *hp;
  char   *nodestr, *cp, *hostlist;
  int     size;
  struct  sockaddr_in saddr;

  badplace *bp;
  char     *id = "svr_startjob";
#endif

  if (FailHost != NULL)
    FailHost[0] = '\0';

  if (EMsg != NULL)
    EMsg[0] = '\0';

  /* if not already setup, transfer the control/script file basename */
  /* into an attribute accessible by MOM */

  if (!(pjob->ji_wattr[(int)JOB_ATR_hashname].at_flags & ATR_VFLAG_SET))
    {
    if (job_attr_def[(int)JOB_ATR_hashname].at_decode(
          &pjob->ji_wattr[(int)JOB_ATR_hashname],
          NULL, 
          NULL, 
          pjob->ji_qs.ji_fileprefix))
      {
      return(PBSE_SYSTEM);
      }
    }

  /* if exec_host already set and either (hot start or checkpoint) */
  /* then use the host(s) listed in exec_host			*/

  /* NOTE:  qrun hostlist assigned in req_runjob() */

  rc = 0;

  f = pjob->ji_wattr[(int)JOB_ATR_exec_host].at_flags & ATR_VFLAG_SET;

  if ((f != 0) && 
     ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HOTSTART) ||
      (pjob->ji_qs.ji_svrflags & JOB_SVFLG_CHKPT)) && 
     ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HasNodes) == 0)) 
    {
    rc = assign_hosts(    /* inside svr_startjob() */
           pjob,
           pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,
           0,
           FailHost,
           EMsg);
    } 
  else if (f == 0) 
    {
    /* exec_host not already set, get hosts and set it */

    rc = assign_hosts(
      pjob,
      NULL,
      1,
      FailHost,
      EMsg);  /* inside svr_startjob() */
    }

  if (rc != 0)
    {
    /* FAILURE */

    return(rc);
    }

#ifdef BOEING
  /* Verify that all the nodes are alive via a TCP connect. */

  /* NOTE: Copy the nodes into a temp string because strtok() is destructive. */

  size = strlen(pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str);
  hostlist = malloc(size + 1);

  if (hostlist == NULL)
    {
    sprintf(log_buffer,"could not allocate temporary buffer (malloc failed) -- skipping TCP connect check");
    log_err(errno,id,log_buffer);
    }
  else
    {
    /* Get the first host. */

    strncpy(hostlist,pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,size);
    hostlist[size] = '\0';
    nodestr = strtok(hostlist,"+");
    }

  while (nodestr != NULL)
    {
    /* truncate from trailing slash on (if one exists). */

    if ((cp = strchr(nodestr,'/')) != NULL)
      {
      cp[0] = '\0';
      }

    /* Lookup IP address of host. */

    if ((hp = gethostbyname(nodestr)) == NULL)
      {
      sprintf(log_buffer,"could not contact %s (gethostbyname failed, errno: %d (%s))",
        nodestr,
        errno,
        pbs_strerror(errno));

      if (FailHost != NULL)
        strncpy(FailHost,nodestr,1024);

      if (EMsg != NULL)
        strncpy(EMsg,log_buffer,1024);

      log_record(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        log_buffer);

      /* Add this host to the reject destination list for the job */

      bp = (badplace *)malloc(sizeof(badplace));

      if (bp == NULL)
        {
        log_err(errno,id,msg_err_malloc);

        return;
        }

      CLEAR_LINK(bp->bp_link);

      strcpy(bp->bp_dest,nodestr);

      append_link(&pjob->ji_rejectdest,&bp->bp_link,bp);

      /* FAILURE - cannot lookup master compute host */

      return(PBSE_RESCUNAV);
      }

    /* open a socket. */

    /* NOTE:  should change to PF_* */

    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1)
      {
      sprintf(log_buffer,"could not contact %s (cannot create socket, errno: %d (%s))",
        nodestr,
        errno,
        pbs_strerror(errno));

      if (FailHost != NULL)
        strncpy(FailHost,nodestr,1024);

      if (EMsg != NULL)
        strncpy(EMsg,log_buffer,1024);

      log_record(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        log_buffer);

      /* Add this host to the reject destination list for the job */

      bp = (badplace *)malloc(sizeof(badplace));

      if (bp == NULL)
        {
        /* FAILURE - cannot allocate memory */

        log_err(errno,id,msg_err_malloc);

        return(PBSE_RESCUNAV);
        }

      CLEAR_LINK(bp->bp_link);

      strcpy(bp->bp_dest,nodestr);

      append_link(&pjob->ji_rejectdest,&bp->bp_link,bp);

      /* FAILURE - cannot create socket for master compute host */

      return(PBSE_RESCUNAV);
      }

    /* Set the host information. */

    memset(&saddr,'\0',sizeof(saddr));
    saddr.sin_family = AF_INET;
    memcpy(&saddr.sin_addr,hp->h_addr,hp->h_length);
    saddr.sin_port = htons(pbs_rm_port);

    /* Connect to the host. */

    if (connect(sock,(struct sockaddr *)&saddr,sizeof(saddr)) < 0)
      {
      sprintf(log_buffer,"could not contact %s (connect failed, errno: %d (%s))",
        nodestr,
        errno,
        pbs_strerror(errno));

      if (FailHost != NULL)
        strncpy(FailHost,nodestr,1024);

      if (EMsg != NULL)
        strncpy(EMsg,log_buffer,1024);

      log_record(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        log_buffer);

      /* Add this host to the reject list for the job */

      bp = (badplace *)malloc(sizeof(badplace));

      if (bp == NULL)
        {
        /* FAILURE - cannot allocate memory */

        log_err(errno,id,msg_err_malloc);

        return(PBSE_RESCUNAV);
        }

      CLEAR_LINK(bp->bp_link);

      strcpy(bp->bp_dest,nodestr);

      append_link(&pjob->ji_rejectdest,&bp->bp_link,bp);

      /* FAILURE - cannot connect to master compute host */

      return(PBSE_RESCUNAV);
      }

    /* clean up and get next host. */

    close(sock);

    nodestr = strtok(NULL,"+");
    }  /* END while (nodestr != NULL) */

  if (hostlist != NULL)
    free(hostlist);

  /* END MOM verification check via TCP. */

#endif  /* END BOEING */

  /* Next, are there files to be staged-in? */

  if ((pjob->ji_wattr[(int)JOB_ATR_stagein].at_flags & ATR_VFLAG_SET) &&
      (pjob->ji_qs.ji_substate != JOB_SUBSTATE_STAGECMP)) 
    {
    /* yes, we do that first; then start the job */

    rc = svr_stagein(
      pjob, 
      preq, 
      JOB_STATE_RUNNING,
      JOB_SUBSTATE_STAGEGO);

    /* note, the positive acknowledgment is done by svr_stagein */
    } 
  else 
    {
    /* No stage-in or already done, start job executing */

    rc = svr_strtjob2(pjob,preq);
    }

  return(rc);
  }  /* END svr_startjob() */


/* PATH
    req_runjob()
 >    svr_startjob()
        svr_strtjob2()
          send_job()         - svr_movejob.c
            svr_connect()
            PBSD_queuejob() 
*/




static int svr_strtjob2(

  job                  *pjob,  /* I */
  struct batch_request *preq)  /* I (modified - report status) */

  {
  extern char *PAddrToString(pbs_net_t *);

  int old_state;
  int old_subst;
  attribute *pattr;

  char tmpLine[1024];

  old_state = pjob->ji_qs.ji_state;
  old_subst = pjob->ji_qs.ji_substate;

  pattr = &pjob->ji_wattr[(int)JOB_ATR_start_time];
  if ((pattr->at_flags & ATR_VFLAG_SET) == 0) 
    {
    pattr->at_val.at_long = time(NULL);
    pattr->at_flags |= ATR_VFLAG_SET;
    }

  pattr = &pjob->ji_wattr[(int)JOB_ATR_start_count];
  pattr->at_val.at_long++;
  pattr->at_flags |= ATR_VFLAG_SET;

  /* send the job to MOM */

  svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_PRERUN);

  if (send_job(
        pjob,
        pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
        pbs_mom_port, 
        MOVE_TYPE_Exec, 
        post_sendmom,
        (void *)preq) == 2) 
    {
    /* SUCCESS */

    return(0);
    } 

  sprintf(tmpLine,"unable to run job, send to MOM '%s' failed",
    PAddrToString(&pjob->ji_qs.ji_un.ji_exect.ji_momaddr));
      
  log_event(
    PBSEVENT_JOB, 
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    tmpLine);

  pjob->ji_qs.ji_destin[0] = '\0';

  svr_setjobstate(
    pjob, 
    old_state, 
    old_subst);

  return(pbs_errno);
  }    /* END svr_strtjob2() */






/*
 * post_sendmom - clean up action for child started in send_job
 *	which was sending a job "home" to MOM
 *
 * If send was successful, mark job as executing, and call stat_mom_job()
 * to obtain session id.
 *
 * If send didn't work, requeue the job.
 *
 * If the work_task has a non-null wt_parm2, it is the address of a batch
 * request to which a reply must be sent.
 *
 * Returns: none.
 */

static void post_sendmom(

  struct work_task *pwt)  /* I */

  {
  char	*id = "post_sendmom";

  int	 newstate;
  int	 newsub;
  int	 r;
  int	 stat;
  job	*jobp = (job *)pwt->wt_parm1;
  struct batch_request *preq = (struct batch_request *)pwt->wt_parm2;

  char  *MOMName = NULL;

  int    jindex;
  long DTime = time_now - 10000;

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      jobp->ji_qs.ji_jobid,
      "entering post_sendmom");
    }

  stat = pwt->wt_aux;

  if (WIFEXITED(stat)) 
    {
    r = WEXITSTATUS(stat);
    } 
  else 
    {
    r = 2;

    /* cannot get child exit status */

    sprintf(log_buffer,msg_badexit,
      stat);

    strcat(log_buffer,id);

    log_event(
      PBSEVENT_SYSTEM, 
      PBS_EVENTCLASS_JOB,
      jobp->ji_qs.ji_jobid,
      log_buffer);
    }

  /* maintain local struct to associate job id with dispatch time */

  for (jindex = 0;jindex < 20;jindex++)
    {
    if (DispatchJob[jindex] == jobp)
      {
      DTime = DispatchTime[jindex];

      DispatchJob[jindex] = NULL;

      MOMName = DispatchNode[jindex];

      break;
      }
    }

  if (LOGLEVEL >= 1)
    {
    sprintf(log_buffer,"child reported %s for job after %ld seconds (dest=%s), rc=%d",
      (r == 0) ? "success" : "failure",
      time_now - DTime,
      (MOMName != NULL) ? MOMName : "???",
      r);

    log_event(
      PBSEVENT_SYSTEM,
      PBS_EVENTCLASS_JOB,
      jobp->ji_qs.ji_jobid,
      log_buffer);
    }

  switch (r) 
    {
    case 0:  /* send to MOM went ok */

      jobp->ji_qs.ji_svrflags &= ~JOB_SVFLG_HOTSTART;

      if (preq != NULL)
        reply_ack(preq);
			
      /* record start time for accounting */

      jobp->ji_qs.ji_stime = time_now;

      /* update resource usage attributes */

      set_resc_assigned(jobp,INCR);

      if (jobp->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN) 
        {
        /* may be EXITING if job finished first */

        svr_setjobstate(jobp,JOB_STATE_RUNNING,JOB_SUBSTATE_RUNNING);	

        /* above saves job structure */
        }

      /* accounting log for start or restart */

      if (jobp->ji_qs.ji_svrflags & JOB_SVFLG_CHKPT)
        account_record(PBS_ACCT_RESTRT,jobp,NULL);
      else
        account_jobstr(jobp);

      /* if any dependencies, see if action required */
		
      if (jobp->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
        depend_on_exec(jobp);

      /*
       * it is unfortunate, but while the job has gone into execution,
       * there is no way of obtaining the session id except by making
       * a status request of MOM.  (Even if the session id was passed
       * back to the sending child, it couldn't get up to the parent.)
       */

      jobp->ji_momstat = 0;

      stat_mom_job(jobp);

      break;

    case 10:

      /* NOTE: if r == 10, connection to mom timed out.  Mark node down */

      stream_eof(-1,jobp->ji_qs.ji_un.ji_exect.ji_momaddr,0);

      /* send failed, requeue the job */

      log_event(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        jobp->ji_qs.ji_jobid,
        "unable to run job, MOM rejected/timeout");

      free_nodes(jobp);

      if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_ABORT)
        {
        if (preq != NULL)
          req_reject(PBSE_MOMREJECT,0,preq,MOMName,"connection to mom timed out");

        svr_evaljobstate(jobp,&newstate,&newsub,1);

        svr_setjobstate(jobp,newstate,newsub);
        }
      else
        {
        if (preq != NULL)
          req_reject(PBSE_BADSTATE,0,preq,MOMName,"job was aborted by mom");
        }

      break;

    case 1:   /* commit failed */
    default:	

      {
      int JobOK = 0;

      /* send failed, requeue the job */

      sprintf(log_buffer,"unable to run job, MOM rejected/rc=%d",
        r);

      log_event(
        PBSEVENT_JOB, 
        PBS_EVENTCLASS_JOB,
        jobp->ji_qs.ji_jobid,
        log_buffer);

      free_nodes(jobp);

      if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_ABORT) 
        {
        if (preq != NULL)
          {
          char tmpLine[1024];

          if (preq->rq_reply.brp_code == PBSE_JOBEXIST)
            {
            /* job already running, start request failed but return success since
             * desired behavior (job is running) is accomplished */

            JobOK = 1;
            }
          else
            { 
            sprintf(tmpLine,"cannot send job to %s, state=%s",
              (MOMName != NULL) ? MOMName : "mom",
              PJobSubState[jobp->ji_qs.ji_substate]);
          
            req_reject(PBSE_MOMREJECT,0,preq,MOMName,tmpLine);
            }
          }

        if (JobOK == 1)
          {
          /* do not re-establish accounting - completed first time job was started */

          /* update mom-based job status */

          jobp->ji_momstat = 0;

          stat_mom_job(jobp);
          }
        else
          {
          svr_evaljobstate(jobp,&newstate,&newsub,1);

          svr_setjobstate(jobp,newstate,newsub);
          }
        } 
      else 
        {
        if (preq != NULL)
          req_reject(PBSE_BADSTATE,0,preq,MOMName,"send failed - abort");
        }
			
      break;
      }
    }  /* END switch (r) */

  return;
  }  /* END post_sendmom() */





/*
 * chk_job_torun - check state and past execution host of a job for which
 *	files are about to be staged in or for a job that is about to be run.
 * 	Returns pointer to job if all is ok, else returns NULL.
 */

static job *chk_job_torun(

  struct batch_request *preq,  /* I */
  int                   setnn) /* I */

  {
  static char *id = "chk_job_torun";

  job              *pjob;
  struct rq_runjob *prun;
  int               rc;

  char              EMsg[1024];
  char              FailHost[1024];
  char              exec_host[1024];
  char              *ptr;

  prun = &preq->rq_ind.rq_run;

  if ((pjob = chk_job_request(prun->rq_jid,preq)) == 0)
    {
    /* FAILURE */

    return(NULL);
    }

  if ((pjob->ji_qs.ji_state == JOB_STATE_TRANSIT) ||
      (pjob->ji_qs.ji_state == JOB_STATE_EXITING) ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEGO) ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN)  ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING))  
    {
    /* FAILURE - job already started */

    req_reject(PBSE_BADSTATE,0,preq,NULL,"job already running");

    return(NULL);
    }

  if (preq->rq_type == PBS_BATCH_StageIn) 
    {
    if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEIN) 
      {
      /* FAILURE */

      req_reject(PBSE_BADSTATE,0,preq,NULL,NULL);

      return(NULL);
      }
    }

  if ((preq->rq_perm & (ATR_DFLAG_MGWR|ATR_DFLAG_OPWR)) == 0) 
    {
    /* FAILURE - run request not authorized */

    req_reject(PBSE_PERM,0,preq,NULL,NULL);

    return(NULL);
    }

  if (pjob->ji_qhdr->qu_qs.qu_type != QTYPE_Execution) 
    {
    /* FAILURE - job must be in execution queue */

    log_err(-1,id,"attempt to start job in non-execution queue");

    req_reject(PBSE_IVALREQ,0,preq,NULL,"job not in execution queue");

    return(NULL);
    }

  /* where to execute the job */

  if (pjob->ji_qs.ji_svrflags & (JOB_SVFLG_CHKPT|JOB_SVFLG_StagedIn)) 
    {
    /* job has been checkpointed or files already staged in */
    /* in this case, exec_host must be already set          */

    if (prun->rq_destin && *prun->rq_destin) /* If a destination has been specified */
      {
      /* specified destination must match exec_host */

      strcpy(exec_host,pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str);
      if ((ptr = strchr(exec_host,'/')))
        *ptr = 0; /* For some reason, node name has "/0" on the end (i.e. "node0001/0"). */
      if (strcmp(prun->rq_destin,exec_host) != 0) 
        {
        /* FAILURE */

        if (pjob->ji_qs.ji_svrflags & (JOB_SVFLG_CHKPT))
          req_reject(PBSE_EXECTHERE,0,preq,NULL,"allocated nodes must match checkpoint location");
        else
          req_reject(PBSE_EXECTHERE,0,preq,NULL,"allocated nodes must match input file stagein location");

        return(NULL);
        }
      }

    if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HasNodes) == 0)
      {
      /* re-reserve nodes and leave exec_host as is */

      if ((rc = assign_hosts(  /* inside chk_job_torun() */
            pjob,
            pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,
            0,
            FailHost,
            EMsg)) != 0)   /* O */
        {
        req_reject(PBSE_EXECTHERE,0,preq,FailHost,EMsg);

        return(NULL);
        }
      }
    }    /* END if (pjob->ji_qs.ji_svrflags & (JOB_SVFLG_CHKPT|JOB_SVFLG_StagedIn)) */ 
  else 
    {
    /* job has not run before or need not run there again */
    /* reserve nodes and set new exec_host */

    if ((prun->rq_destin == NULL) || (prun->rq_destin[0] == '\0')) 
      {
      /* it is possible for the scheduler to pass a hostlist using the rq_extend field--we should use it as the given list
       * as an alternative to rq_destin */

      rc = assign_hosts(pjob,preq->rq_extend,1,FailHost,EMsg);  /* inside chk_job_torun() */
      } 
    else 
      {
      rc = assign_hosts(pjob,prun->rq_destin,1,FailHost,EMsg);  /* inside chk_job_torun() */
      }

    if (rc != 0) 
      {
      /* FAILURE - cannot assign correct hosts */

      req_reject(rc,0,preq,FailHost,EMsg);

      return(NULL);
      }
    }

  if (setnn == 1)
    {
#ifdef TDEV
    /* what should neednodes be set to? */

    resource_def *DRes;  /* resource definition */

    resource *JRes;      /* resource on job */

    attribute *Attr;     /* 'neednodes' attribute */

    Attr = &pjob->ji_wattr[(int)JOB_ATR_resource];

    DRes = find_resc_def(svr_resc_def,"neednodes",svr_resc_size);

    JRes = find_resc_entry(Attr,DRes);

    if ((JRes == NULL) ||
       ((JRes->rs_value.at_flags & ATR_VFLAG_SET) == 0))
      {
      /* resource does not exist or value is not set */

      if (JRes == NULL)
        {
        JRes = add_resource_entry(Attr,DRes);
        }

      if (JRes != NULL)
        {
        if (DRes->rs_defin->rs_set(
              &JRes->rs_value,
              &DRes->rs_value,
              SET) == 0)
          {
          JRes->rs_value.at_flags |= ATR_VFLAG_SET;
          }
        }
      }
#endif /* TDEV */
    }    /* END if (setnn == 1) */

  return(pjob);
  }  /* END chk_job_torun() */





/*
 * assign_hosts - assign hosts (nodes) to job by the following rules:
 *	1. use nodes that are "given"; from exec_host when required by
 *		checkpoint-restart or file stage-in, or from run command.
 *	2. use nodes that match user's resource request.
 *	3. use default (local system or a single node).
 */

static int assign_hosts(

  job  *pjob,           /* I (modified) */
  char *given,          /* I (optional) list of requested hosts */
  int   set_exec_host,  /* I (boolean) */
  char *FailHost,       /* O (optional,minsize=1024) */
  char *EMsg)           /* O (optional,minsize=1024) */

  {
  unsigned int	 dummy;
  char		*list = NULL;
  char		*hosttoalloc = NULL;
  pbs_net_t	 momaddr = 0;
  resource	*pres;
  int		 rc = 0;
  extern char 	*mom_host;

  if (EMsg != NULL)
    EMsg[0] = '\0';

  if (FailHost != NULL)
    FailHost[0] = '\0';

#ifdef __TREQSCHED
  if ((given == NULL) || (given[0] == '\0'))
    {
    /* scheduler must specify node allocation for all jobs */

    return(PBSE_UNKNODEATR);
    }
#endif /* __TREQSCHED */

  if ((given != NULL) && (given[0] != '\0'))
    {	
    /* assign what was specified in run request */

    hosttoalloc = given;
    }
  else
    {
    pres = find_resc_entry(
      &pjob->ji_wattr[(int)JOB_ATR_resource],
      find_resc_def(svr_resc_def,"neednodes",svr_resc_size));

    if (pres != NULL) 
      {	
      /* assign what was in "neednodes" */

      hosttoalloc = pres->rs_value.at_val.at_str;

      if ((hosttoalloc == NULL) || (hosttoalloc[0] == '\0'))
        {
        return(PBSE_UNKNODEATR);
        }
      } 
    }

  if (hosttoalloc != NULL)
    {
    /* NO-OP */
    }
  else if (svr_totnodes == 0) 
    {	
    /* assign "local" */

    if ((server.sv_attr[(int)SRV_ATR_DefNode].at_flags & ATR_VFLAG_SET) && 
        (server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str != NULL)) 
      {
      hosttoalloc = server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str;
      } 
    else 
      {
      hosttoalloc = mom_host;
      momaddr = pbs_mom_addr;
      }
    } 
  else if ((server.sv_attr[(int)SRV_ATR_DefNode].at_flags & ATR_VFLAG_SET) && 
           (server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str != 0)) 
    {
    /* alloc server default_node */

    hosttoalloc = server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str;
    } 
  else if (svr_tsnodes != 0) 
    {
    /* find first time-shared node */

    if ((hosttoalloc = find_ts_node()) == NULL)
      { 
      /* FAILURE */

      return(PBSE_NOTSNODE);
      }
    } 
  else 
    {
    /* fall back to 1 cluster node */

    hosttoalloc = PBS_DEFAULT_NODE;
    }

  /* do we need to allocate the (cluster) node(s)? */

  if (svr_totnodes != 0) 
    {
    if ((rc = is_ts_node(hosttoalloc)) != 0) 
      {
      rc = set_nodes(pjob,hosttoalloc,&list,FailHost,EMsg);

      set_exec_host = 1;	/* maybe new VPs, must set */

      hosttoalloc = list;
      }
    } 
 
  if (rc == 0) 
    {
    /* set_nodes succeeded */

    if (set_exec_host != 0) 
      {
      job_attr_def[(int)JOB_ATR_exec_host].at_free(
        &pjob->ji_wattr[(int)JOB_ATR_exec_host]);

      job_attr_def[(int)JOB_ATR_exec_host].at_decode(
        &pjob->ji_wattr[(int)JOB_ATR_exec_host],
        NULL,
        NULL,
        hosttoalloc);  /* O */

      pjob->ji_modified = 1;
      } 
    else 
      {
      /* leave exec_host alone and reuse old IP address */

      momaddr = pjob->ji_qs.ji_un.ji_exect.ji_momaddr;
		
      hosttoalloc = pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str;
      }

    strncpy(
      pjob->ji_qs.ji_destin,
      parse_servername(hosttoalloc,&dummy),
      PBS_MAXROUTEDEST);

    if (momaddr == 0) 
      {
      momaddr = get_hostaddr(pjob->ji_qs.ji_destin);
 
      if (momaddr == 0) 
        {
        free_nodes(pjob);

        if (list != NULL) 
          free(list);

        sprintf(log_buffer,"ALERT:  job cannot allocate node '%s' (could not determine IP address for node)",
          pjob->ji_qs.ji_destin);

        log_event(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          log_buffer);

        return(PBSE_BADHOST);
        }
      }

    pjob->ji_qs.ji_un.ji_exect.ji_momaddr = momaddr;
    }  /* END if (rc == 0) */

  if (list != NULL)
    free(list);

  return(rc);
  }  /* END assign_hosts() */


/* END req_runjob.c */

