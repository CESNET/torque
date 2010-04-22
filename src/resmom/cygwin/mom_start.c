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
#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
#include "libpbs.h"
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifndef USEOLDTTY
#include <pty.h>
#endif /* USEOLDTTY */

#include "list_link.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_job.h"
#include "mom_mach.h"
#include "mom_func.h"

/* Global Variables */

extern int  exiting_tasks;
extern char  mom_host[];
extern tlist_head svr_alljobs;
extern int  termin_child;

extern int       LOGLEVEL;

extern char     *AllocParCmd;

extern int      multi_mom;

/* Private variables */

#define TMAX_TJCACHESIZE 128
job *TJCache[TMAX_TJCACHESIZE];

/*
 * set_job - set up a new job session
 *  Set session id and whatever else is required on this machine
 * to create a new job.
 *
 * NOTE:  This routine is run as root after fork for both parallel and serial jobs
 * NOTE:  This routine is called by TMOMFinalizeChild()
 *
 *      Return: session/job id or if error:
 *  -1 - if setsid() fails
 *  -2 - if other, message in log_buffer
 */

int set_job(

  job                 *pjob,  /* I (not used) */
  struct startjob_rtn *sjr)   /* I (modified,optional) */

  {
  char id[] = "set_job";

  char *PPtr;
  char *CPtr;

  int   rc;

  long  sid;

  sid = setsid();

  if (sjr != NULL)
    sjr->sj_session = sid;

  /* NOTE:  only activate partition create script for XT4+ environments */

  if (((PPtr = get_job_envvar(pjob, "BATCH_PARTITION_ID")) != NULL) &&
      ((CPtr = get_job_envvar(pjob, "BATCH_ALLOC_COOKIE")) != NULL) &&
      !strcmp(CPtr, "0"))
    {
    char  tmpLine[1024];

    if (AllocParCmd == NULL)
      AllocParCmd = strdup("/opt/moab/default/tools/partition.create.xt4.pl");

#ifdef ENABLE_CSA

    if (pjob->ji_wattr[(int)JOB_ATR_pagg_id].at_flags & ATR_VFLAG_SET)
      {
      snprintf(tmpLine, sizeof(tmpLine), "%s --confirm -p %s -j %s -a %lld",
               AllocParCmd,
               PPtr,
               pjob->ji_qs.ji_jobid,
               pjob->ji_wattr[(int)JOB_ATR_pagg_id].at_val.at_ll);
      }
    else
#endif
      {
      snprintf(tmpLine, sizeof(tmpLine), "%s --confirm -p %s -j %s -a %ld",
             AllocParCmd,
             PPtr,
             pjob->ji_qs.ji_jobid,
             sid);
      }

    log_err(
      -1,
      id,
      tmpLine);

    rc = system(tmpLine);

    if (WEXITSTATUS(rc) != 0)
      {
      snprintf(log_buffer, 1024, "cannot create alloc partition");

      sid = -3;

      if (sjr != NULL)
        sjr->sj_session = sid;

      log_err(
        -1,
        id,
        log_buffer);

      return(sid);
      }
    }    /* END if (((PPtr = get_job_envvar(pjob,"BATCH_PARTITION_ID")) != NULL) && ...) */

  return(sid);
  }  /* END set_job() */





/*
** set_globid - set the global id for a machine type.
*/

void set_globid(

  job                 *pjob,  /* I (modified) */
  struct startjob_rtn *sjr)   /* I (not used) */

  {
  extern char noglobid[];

  if (pjob->ji_globid == NULL)
    pjob->ji_globid = strdup(noglobid);

  return;
  }  /* END set_globid() */




/*
 * set_mach_vars - setup machine dependent environment variables
 */

int set_mach_vars(

  job              *pjob,    /* pointer to the job */
  struct var_table *vtab)    /* pointer to variable table */

  {
  return(0);
  }





char *set_shell(

  job *pjob,
  struct passwd *pwdp)

  {
  char *cp;
  int   i;
  char *shell;

  struct array_strings *vstrs;

  /*
   * find which shell to use, one specified or the login shell
   */

  shell = pwdp->pw_shell;

  if ((pjob->ji_wattr[(int)JOB_ATR_shell].at_flags & ATR_VFLAG_SET) &&
      (vstrs = pjob->ji_wattr[(int)JOB_ATR_shell].at_val.at_arst))
    {
    for (i = 0; i < vstrs->as_usedptr; ++i)
      {
      cp = strchr(vstrs->as_string[i], '@');

      if (cp != NULL)
        {
        if (!strncmp(mom_host, cp + 1, strlen(cp + 1)))
          {
          *cp = '\0'; /* host name matches */

          shell = vstrs->as_string[i];

          break;
          }
        }
      else
        {
        shell = vstrs->as_string[i]; /* wildcard */
        }
      }
    }

  return(shell);
  }  /* END set_shell() */






/**
 * scan_for_terminated - scan the list of running jobs for one whose
 * session id matches that of a terminated child pid.  Mark that
 * job as Exiting.
 *
 * @see finish_loop() - parent
 * @see scan_for_exiting() - peer - called later to harvest jobs with exiting tasks
 *
 */

void scan_for_terminated(void)

  {
  static char id[] = "scan_for_terminated";

  int  exiteval = 0;
  pid_t  pid;
  job *pjob;
  task *ptask = NULL;
  int  statloc;
  unsigned int momport = 0;

  int    tcount;

  if (LOGLEVEL >= 7)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      id,
      "entered");
    }

  /* update the latest intelligence about the running jobs;         */
  /* must be done before we reap the zombies, else we lose the info */

  termin_child = 0;

  if (mom_get_sample() == PBSE_NONE)
    {
    pjob = (job *)GET_PRIOR(svr_alljobs);

    while (pjob != NULL)
      {
      mom_set_use(pjob);

      pjob = (job *)GET_PRIOR(pjob->ji_alljobs);
      }
    }

  /* Now figure out which task(s) have terminated (are zombies) */

  /* NOTE:  does a job's tasks include its epilog? */

  while ((pid = waitpid(-1, &statloc, WNOHANG)) > 0)
    {
    pjob = (job *)GET_PRIOR(svr_alljobs);

    while (pjob != NULL)
      {
      /*
       * see if process was a child doing a special
       * function for MOM
       */

      if (LOGLEVEL >= 7)
        {
        snprintf(log_buffer, 1024, "checking job w/subtask pid=%d (child pid=%d)",
          pjob->ji_momsubt,
          pid);

        LOG_EVENT(
          PBSEVENT_DEBUG,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          log_buffer);
        }

      if (pid == pjob->ji_momsubt)
        {
        if (LOGLEVEL >= 7)
          {
          snprintf(log_buffer, 1024, "found match with job subtask for pid=%d",
            pid);

          LOG_EVENT(
            PBSEVENT_DEBUG,
            PBS_EVENTCLASS_JOB,
            pjob->ji_qs.ji_jobid,
            log_buffer);
          }

        break;
        }

      /* look for task */

      ptask = (task *)GET_NEXT(pjob->ji_tasks);

      /* locate task with associated process id */

      tcount = 0;

      while (ptask != NULL)
        {
        if (ptask->ti_qs.ti_sid == pid)
          {
          if (LOGLEVEL >= 7)
            {
            snprintf(log_buffer, 1024, "found match with job task %d for pid=%d",
              tcount,
              pid);

            LOG_EVENT(
              PBSEVENT_DEBUG,
              PBS_EVENTCLASS_JOB,
              pjob->ji_qs.ji_jobid,
              log_buffer);
            }

          break;
          }

        ptask = (task *)GET_NEXT(ptask->ti_jobtask);

        tcount++;
        }  /* END while (ptask) */

      if (ptask != NULL)
        {
        /* pid match located - break out of job loop */

        break;
        }

      pjob = (job *)GET_PRIOR(pjob->ji_alljobs);
      }  /* END while (pjob != NULL) */

    if (pjob == NULL)
      {
      if (LOGLEVEL >= 1)
        {
        sprintf(log_buffer, "pid %d not tracked, exitcode=%d",
          pid,
          statloc);

        log_record(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }

      continue;
      }  /* END if (pjob == NULL) */

    if (WIFEXITED(statloc))
      exiteval = WEXITSTATUS(statloc);
    else if (WIFSIGNALED(statloc))
      exiteval = WTERMSIG(statloc) + 0x100;
    else
      exiteval = 1;

    if (pid == pjob->ji_momsubt)
      {
      /* PID matches job mom subtask */

      /* NOTE:  both ji_momsubt and ji_mompost normally set in routine
                preobit_reply() after epilog child is successfully forked */

      if (pjob->ji_mompost != NULL)
        {
        if (pjob->ji_mompost(pjob, exiteval) == 0)
          {
          /* success */

          pjob->ji_mompost = NULL;
          }

        }  /* END if (pjob->ji_mompost != NULL) */
      else
        {
        log_record(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          "job has no postprocessing routine registered");
        }

      /* clear mom sub-task */

      pjob->ji_momsubt = 0;

      if(multi_mom)
        {
        momport = pbs_rm_port;
        }

      job_save(pjob, SAVEJOB_QUICK, momport);

      continue;
      }  /* END if (pid == pjob->ji_momsubt) */

    /* what happens if mom PID is reaped before subtask? */

    if (LOGLEVEL >= 2)
      {
      sprintf(log_buffer, "pid %d harvested for job %s, task %d, exitcode=%d",
              pid,
              pjob->ji_qs.ji_jobid,
              ptask->ti_qs.ti_task,
              exiteval);

      log_record(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        id,
        log_buffer);
      }

    /* where is job purged?  How do we keep job from progressing in state until the obit is sent? */

    kill_task(ptask, SIGKILL, 0);

    ptask->ti_qs.ti_exitstat = exiteval;

    ptask->ti_qs.ti_status   = TI_STATE_EXITED;

    task_save(ptask);

    sprintf(log_buffer, "%s: job %s task %d terminated, sid=%d",
            id,
            pjob->ji_qs.ji_jobid,
            ptask->ti_qs.ti_task,
            ptask->ti_qs.ti_sid);

    LOG_EVENT(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);

    exiting_tasks = 1;
    }  /* END while ((pid = waitpid(-1,&statloc,WNOHANG)) > 0) */

  return;
  }  /* END scan_for_terminated() */




/* SNL patch to use openpty() system call rather than search /dev */

/*
 * create the master pty, this particular
 * piece of code depends on multiplexor /dev/ptc aka /dev/ptmx
 */

#define PTY_SIZE 64

#ifndef USEOLDTTY

int open_master(

  char **rtn_name) /* RETURN name of slave pts */

  {
  int master;
  int slave;
  static char slave_name[PTY_SIZE];

  int status = openpty(&master, &slave, slave_name, 0, 0);

  if (status < 0)
    {
    log_err(errno, "open_master",
            "failed in openpty()");

    return(-1);
    }

  close(slave);

  /* open_master has no way to return this, must return slave_name instead */

  *rtn_name = slave_name;

  return(master);
  }  /* END open_master() */

#else /* USEOLDTTY */

int open_master(

  char **rtn_name)      /* RETURN name of slave pts */

  {
  char         *pc1;
  char         *pc2;
  int  ptc; /* master file descriptor */
  static char ptcchar1[] = "pqrs";
  static char ptcchar2[] = "0123456789abcdef";
  static char pty_name[PTY_SIZE + 1]; /* "/dev/[pt]tyXY" */

  strncpy(pty_name, "/dev/ptyXY", PTY_SIZE);

  for (pc1 = ptcchar1;*pc1 != '\0';++pc1)
    {
    pty_name[8] = *pc1;

    for (pc2 = ptcchar2;*pc2 != '\0';++pc2)
      {
      pty_name[9] = *pc2;

      if ((ptc = open(pty_name, O_RDWR | O_NOCTTY, 0)) >= 0)
        {
        /* got a master, fix name to matching slave */

        pty_name[5] = 't';

        *rtn_name = pty_name;

        return(ptc);
        }
      else if (errno == ENOENT)
        {
        return(-1); /* tried all entries, give up */
        }
      }
    }

  return(-1); /* tried all entries, give up */
  }  /* END open_master() */

#endif /* USEOLDTTY */



/*
 * struct sig_tbl = map of signal names to numbers,
 * see req_signal() in ../requests.c
 */

struct sig_tbl sig_tbl[] =
  {
    { "NULL", 0
    },

  { "HUP", SIGHUP },
  { "INT", SIGINT },
  { "QUIT", SIGQUIT },
  { "ILL",  SIGILL },
  { "TRAP", SIGTRAP },
  { "IOT", SIGIOT },
  { "ABRT", SIGABRT },
  { "FPE", SIGFPE },
  { "KILL", SIGKILL },
  { "BUS", SIGBUS },
  { "SEGV", SIGSEGV },
  { "PIPE", SIGPIPE },
  { "ALRM", SIGALRM },
  { "TERM", SIGTERM },
  { "URG", SIGURG },
  { "STOP", SIGSTOP },
  /* { "suspend", SIGSTOP }, - NOTE: changed for MPI jobs - NORWAY */
  { "suspend", SIGTSTP },
  { "TSTP", SIGTSTP },
  { "CONT", SIGCONT },
  { "resume", SIGCONT },
  { "CHLD", SIGCHLD },
  { "CLD",  SIGCHLD },
  { "TTIN", SIGTTIN },
  { "TTOU", SIGTTOU },
  { "IO", SIGIO },
  { "POLL", SIGPOLL },
  { "XCPU", SIGXCPU },
  { "XFSZ", SIGXFSZ },
  { "VTALRM", SIGVTALRM },
  { "PROF", SIGPROF },
  { "WINCH", SIGWINCH },
  { "USR1", SIGUSR1 },
  { "USR2", SIGUSR2 },
  {(char *)0, -1 }
  };
