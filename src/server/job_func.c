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
 * Functions which provide basic operation on the job structure
 *
 * Included public functions are:
 *
 *   job_abt	  abort (remove from server) a job
 *   job_alloc    allocate job struct and initialize defaults
 *   job_free	  free space allocated to the job structure and its
 *		  childern structures.
 *   job_purge	  purge job from server
 *
 * Include private function:
 *   job_init_wattr() initialize job working attribute array to "unspecified"
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#ifndef SIGKILL
#include <signal.h>
#endif
#if __STDC__ != 1
#include <memory.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "work_task.h"
#include "attribute.h"
#include "resource.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "job.h"
#include "log.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "acct.h"
#include "net_connect.h"


/* External functions */

void on_job_exit A_((struct work_task *));

#ifdef PBS_MOM
#if IBM_SP2==2		/* IBM SP PSSP 3.1 */
void unload_sp_switch A_((job *pjob));
#endif			/* IBM SP */
#endif	/*  PBS_MOM */

/* Local Private Functions */

static void job_init_wattr A_((job *));

/* Global Data items */

#ifndef PBS_MOM
extern struct server   server;
#else
extern gid_t pbsgroup;
#endif	/* PBS_MOM */
extern char *msg_abt_err;
extern char *path_jobs;
extern char *path_spool;
extern char  server_name[];
extern time_t time_now;
extern list_head svr_newjobs;
extern list_head svr_alljobs;


#ifdef PBS_MOM
void nodes_free A_((job *));
int TTmpDirName A_((job *,char *));


void tasks_free(

  job *pj)

  {
  task	*tp = (task *)GET_NEXT(pj->ji_tasks);
  obitent	*op;
  infoent *ip;

  while (tp != NULL) 
    {
    op = (obitent *)GET_NEXT(tp->ti_obits);

    while (op != NULL) 
      {
      delete_link(&op->oe_next);

      free(op);

      op = (obitent *)GET_NEXT(tp->ti_obits);
      }  /* END while (op != NULL) */

    ip = (infoent *)GET_NEXT(tp->ti_info);

    while (ip != NULL) 
      {
      delete_link(&ip->ie_next);

      free(ip->ie_name);
      free(ip->ie_info);
      free(ip);

      ip = (infoent *)GET_NEXT(tp->ti_info);
      }

    close_conn(tp->ti_fd);

    delete_link(&tp->ti_jobtask);

    free(tp);

    tp = (task *)GET_NEXT(pj->ji_tasks);
    }  /* END while (tp != NULL) */

  return;
  }  /* END tasks_free() */





/*
 * remtree - remove a tree (or single file)
 *
 *	returns	 0 on success
 *		-1 on failure
 */

int remtree(

  char *dirname)

  {
	static	char	id[] = "remtree";
	DIR		*dir;
	struct dirent	*pdir;
	char		namebuf[MAXPATHLEN], *filnam;
	int		i;
	int		rtnv = 0;
	struct stat	sb;

	if (stat(dirname, &sb) == -1) {
		if (errno != ENOENT)
			log_err(errno, id, "stat");
		return -1;
	}
	if (S_ISDIR(sb.st_mode)) {
	    if ((dir = opendir(dirname)) == NULL) {
		if (errno != ENOENT)
			log_err(errno, id, "opendir");
		return -1;
	    }

	    (void)strcpy(namebuf, dirname);
	    (void)strcat(namebuf, "/");
	    i = strlen(namebuf);
	    filnam = &namebuf[i];

	    while ((pdir = readdir(dir)) != NULL) {
		if ( pdir->d_name[0] == '.' &&
		    (pdir->d_name[1] == '\0' || pdir->d_name[1] == '.'))
			continue;

		(void)strcpy(filnam, pdir->d_name);

		if (stat(namebuf, &sb) == -1) {
			log_err(errno, id, "stat");
			rtnv = -1;
			continue;
		}
		if (S_ISDIR(sb.st_mode)) {
			rtnv = remtree(namebuf);
		} else if (unlink(namebuf) < 0) {
			if (errno != ENOENT) {
			    sprintf(log_buffer, "unlink failed on %s", namebuf);
			    log_err(errno, id, log_buffer);
			    rtnv = -1;
			}
		}
	    }
	    (void)closedir(dir);
	    if (rmdir(dirname) < 0) {
		if (errno != ENOENT) {
			sprintf(log_buffer, "rmdir failed on %s", dirname);
			log_err(errno, id, log_buffer);
			rtnv = -1;
		}
	    }
	} else if (unlink(dirname) < 0) {
		sprintf(log_buffer, "unlink failed on %s", dirname);
		log_err(errno, id, log_buffer);
		rtnv = -1;
	}

  return(rtnv);
  }  /* END remtree() */


#else	/* PBS_MOM */




/*
 * job_abt - abort a job
 *
 *	The job removed from the system and a mail message is sent
 *	to the job owner.
 */

/* NOTE:  this routine is called under the following conditions:  ??? */

int job_abt(

  job  **pjobp, /* I (modified/freed) */
  char  *text)  /* I (optional) */

  {
  char *myid = "job_abt";
  int	old_state;
  int	old_substate;
  int	rc = 0;

  job *pjob = *pjobp;

  void job_purge A_((job *));

  /* save old state and update state to Exiting */

  old_state = pjob->ji_qs.ji_state;
  old_substate = pjob->ji_qs.ji_substate;

  /* notify user of abort if notification was requested */

  if (text != NULL) 
    {	
    /* req_delete sends own mail and acct record */

    account_record(PBS_ACCT_ABT,pjob,"");
    svr_mailowner(pjob,MAIL_ABORT,MAIL_NORMAL,text);
    }

  if (old_state == JOB_STATE_RUNNING) 
    {
    svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_ABORT);

    if ((rc = issue_signal(pjob,"SIGKILL",release_req,0)) != 0) 
      {
      sprintf(log_buffer,msg_abt_err, 
        pjob->ji_qs.ji_jobid,old_substate);

      log_err(-1,myid,log_buffer);

      if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
        {
        /* notify creator that job is exited */

        pjob->ji_wattr[(int)JOB_ATR_state].at_val.at_char = 'E';
   
        issue_track(pjob);
        }

      if (pjob->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
        {
        depend_on_term(pjob);
        }

      job_purge(pjob);

      *pjobp = NULL;
      }
    } 
  else if ((old_state == JOB_STATE_TRANSIT) && 
           (old_substate == JOB_SUBSTATE_TRNOUT)) 
    {
    /* I don't know of a case where this could happen */
  
    sprintf(log_buffer, msg_abt_err, 
      pjob->ji_qs.ji_jobid, 
      old_substate);

    log_err(-1,myid,log_buffer);
    } 
  else 
    {
    svr_setjobstate(pjob,JOB_STATE_EXITING,JOB_SUBSTATE_ABORT);

    if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
      {
      /* notify creator that job is exited */

      issue_track(pjob);
      }

    if (pjob->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
      {
      depend_on_term(pjob);
      }

    job_purge(pjob);

    *pjobp = NULL;
    }

  return(rc);
  }  /* END job_abt() */

#endif	/* else PBS_MOM */





/*
 * job_alloc - allocate space for a job structure and initialize working
 *	attribute to "unset"
 *
 *	Returns: pointer to structure or null is space not available.
 */

job *job_alloc()

  {
  job *pj;

  pj = (job *)calloc(1,sizeof(job));

  if (pj == NULL) 
    {
    log_err(errno,"job_alloc","no memory");

    return(NULL);
    }

  CLEAR_LINK(pj->ji_alljobs);
  CLEAR_LINK(pj->ji_jobque);

#ifdef	PBS_MOM
  CLEAR_HEAD(pj->ji_tasks);
  pj->ji_taskid = TM_NULL_TASK + 1;
  pj->ji_numnodes = 0;
  pj->ji_numvnod  = 0;
  pj->ji_hosts = NULL;
  pj->ji_vnods = NULL;
  pj->ji_resources = NULL;
  pj->ji_obit = TM_NULL_EVENT;
  pj->ji_preq = NULL;
  pj->ji_nodekill = TM_ERROR_NODE;
  pj->ji_flags = 0;
  pj->ji_globid = NULL;
  pj->ji_stdout = 0;
  pj->ji_stderr = 0;
#else	/* SERVER */
  CLEAR_HEAD(pj->ji_svrtask);
  CLEAR_HEAD(pj->ji_rejectdest);
#endif  /* else PBS_MOM */

  pj->ji_momhandle = -1;		/* mark mom connection invalid */

  /* set the working attributes to "unspecified" */

  job_init_wattr(pj);

  return(pj);
  }  /* END job_alloc() */





/*
 * job_free - free job structure and its various sub-structures
 */

void job_free(

  job *pj)

  {
  int			 i;

#ifndef PBS_MOM
  struct work_task	*pwt;
  badplace		*bp;
#endif /* PBS_MOM */

  /* remove any malloc working attribute space */

  for (i = 0;i < (int)JOB_ATR_LAST;i++) 
    {
    job_attr_def[i].at_free(&pj->ji_wattr[i]);
    }

#ifndef PBS_MOM

  /* delete any work task entries associated with the job */

  while ((pwt = (struct work_task *)GET_NEXT(pj->ji_svrtask)) != NULL) 
    {
    delete_task(pwt);
    }

  /* free any bad destination structs */

  bp = (badplace *)GET_NEXT(pj->ji_rejectdest);

  while (bp != NULL) 
    {
    delete_link(&bp->bp_link);

    free(bp);

    bp = (badplace *)GET_NEXT(pj->ji_rejectdest);
    }

#else
 
  if (pj->ji_grpcache)
    free(pj->ji_grpcache);

  assert(pj->ji_preq == NULL);

  nodes_free(pj);
  tasks_free(pj);

  if (pj->ji_resources)
    free(pj->ji_resources);

  if (pj->ji_globid)
    free(pj->ji_globid);

#endif	/* PBS_MOM */

  /* now free the main structure */

  free((char *)pj);

  return;
  }  /* END job_free() */




/*
 * job_init_wattr - initialize job working attribute array
 *	set the types and the "unspecified value" flag
 */
static void job_init_wattr(

  job *pj)

  {
  int i;

  for (i = 0;i < (int)JOB_ATR_LAST;i++) 
    {
    clear_attr(&pj->ji_wattr[i],&job_attr_def[i]);
    }

  return;
  }   /* END job_init_wattr() */ 





/*
 * job_purge - purge job from system
 *
 * The job is dequeued; the job control file, script file and any spooled
 * output files are unlinked, and the job structure is freed.
 * If we are MOM, the task files and checkpoint files are also
 * removed.
 */

void job_purge(

  job *pjob)

  {
  static char	id[] = "job_purge";

  char		namebuf[MAXPATHLEN + 1];
  extern char	*msg_err_purgejob;
#ifdef PBS_MOM
  int		rc;
#endif


#ifdef PBS_MOM

  if (pjob->ji_flags & MOM_HAS_TMPDIR)
    {
    if (TTmpDirName(pjob,namebuf))
      {
      sprintf(log_buffer,"Removing transient job directory %s",namebuf);
      
      log_record(PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        log_buffer);
      
#if defined(HAVE_SETEUID) && defined(HAVE_SETEGID)

    /* most systems */

    if ((setegid(pjob->ji_qs.ji_un.ji_momt.ji_exgid) == -1) ||
        (seteuid(pjob->ji_qs.ji_un.ji_momt.ji_exuid) == -1))
      {
      return;
      }

    rc = remtree(namebuf);

    seteuid(0);
    setegid(pbsgroup);

#elif defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)

    /* HPUX and the like */

    if ((setresgid(-1,pjob->ji_qs.ji_un.ji_momt.ji_exgid,-1) == -1) ||
        (setresuid(-1,pjob->ji_qs.ji_un.ji_momt.ji_exuid,-1) == -1))
      {
      return;
      }
    
    rc = remtree(namebuf);

    setresuid(-1,0,-1);
    setresgid(-1,pbsgroup,-1);

#endif  /* HAVE_SETRESUID */

      if (rc != 0)
        {
        sprintf(log_buffer,
          "recursive remove of job transient tmpdir %s failed",
          namebuf);
    
        log_err(errno, "recursive (r)rmdir", log_buffer);
        }
        
        pjob->ji_flags &= ~MOM_HAS_TMPDIR;
      }
    }

  delete_link(&pjob->ji_jobque);
  delete_link(&pjob->ji_alljobs);

#else /* PBS_MOM */

  if ((pjob->ji_qs.ji_substate != JOB_SUBSTATE_TRANSIN) &&
      (pjob->ji_qs.ji_substate != JOB_SUBSTATE_TRANSICM))
    {
    svr_dequejob(pjob);
    }

#endif  /* PBS_MOM */

  strcpy(namebuf,path_jobs);	/* delete script file */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_SCRIPT_SUFFIX);

  if (unlink(namebuf) < 0)
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }

#ifdef PBS_MOM
#if IBM_SP2==2        /* IBM SP PSSP 3.1 */
  unload_sp_switch(pjob);
#endif			/* IBM SP */
  strcpy(namebuf,path_jobs);      /* job directory path */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_TASKDIR_SUFFIX);
  remtree(namebuf);

#if MOM_CHECKPOINT == 1
  {
  extern char *path_checkpoint;

  strcpy(namebuf,path_checkpoint);	/* delete any checkpoint file */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_CKPT_SUFFIX);
  remtree(namebuf);
  }
#endif	/* MOM_CHECKPOINT */
#else	/* PBS_MOM */
  strcpy(namebuf,path_spool);	/* delete any spooled stdout */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_STDOUT_SUFFIX);

  if (unlink(namebuf) < 0) 
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }

  strcpy(namebuf,path_spool);	/* delete any spooled stderr */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_STDERR_SUFFIX);

  if (unlink(namebuf) < 0) 
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }
#endif	/* PBS_MOM */

  strcpy(namebuf,path_jobs);	/* delete job file */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_FILE_SUFFIX);

  if (unlink(namebuf) < 0)
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }

  job_free(pjob);

  return;
  }  /* END job_purge() */





/*
 * find_job() - find job by jobid
 *
 *	Search list of all server jobs for one with same job id
 *	Return NULL if not found or pointer to job struct if found
 */

job *find_job(

  char *jobid)

  {
  char *at;
  job  *pj;

  if ((at = strchr(jobid,(int)'@')) != NULL)
    *at = '\0';	/* strip off @server_name */

  pj = (job *)GET_NEXT(svr_alljobs);

  while (pj != NULL) 
    {
    if (!strcmp(jobid,pj->ji_qs.ji_jobid))
      break;

    pj = (job *)GET_NEXT(pj->ji_alljobs);
    }

  if (at)
    *at = '@';	/* restore @server_name */

  return(pj);  /* may be NULL */
  }   /* END find_job() */ 

/* END job_func.c */

