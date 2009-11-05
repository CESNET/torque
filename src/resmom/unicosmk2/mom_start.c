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

#include <sys/types.h>
#include <sys/session.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include "libpbs.h"
#include "list_link.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_job.h"
#include "mom_mach.h"
#include "mom_func.h"

static char ident[] = "@(#) $RCSfile$ $Revision$";

/* Global Variables */

extern int  exiting_tasks;
extern char  mom_host[];
extern tlist_head svr_alljobs;
extern int  termin_child;
extern time_t    time_now;

#if SRFS
char *fast_dir;
char *big_dir;
char *tmp_dir;
#endif

/*
 * tmpdirname - build a temporary directory name
 */

char *
tmpdirname(char *root, char *sequence)
  {
  static char tmpdir[MAXPATHLEN+1];

  if (root == NULL)
    return (NULL);

  sprintf(tmpdir, "%s/pbs.%s", root, sequence);

  return (tmpdir);
  }

#define DIR_RETRY 10

/*
** Try to make a directory.  If it works, return 0.  If
** the name exists, return 1.  If it fails for any other reason,
** return -1.
*/
int
makedir(name, uid, gid)
char *name;
uid_t uid;
gid_t gid;
  {
  static char id[] = "makedir";
  int i;

  for (i = 0; i < DIR_RETRY; i++)
    {
    if (mkdir(name, 0777) == -1)
      {
      if (errno == EEXIST)
        {
        sleep(1);
        continue;
        }

      sprintf(log_buffer, "mkdir: %s", name);

      log_err(errno, id, log_buffer);
      return -1;
      }

    if (chown(name, uid, gid) == -1)
      {
      sprintf(log_buffer, "chown: %s", name);
      log_err(errno, id, log_buffer);
      return -1;
      }

    return 0;
    }

  sprintf(log_buffer, "mkdir retrys exhausted: %s", name);

  log_err(errno, id, log_buffer);
  return 1;
  }

/*
 * mktmpdir - make temporary directory(s)
 * Temporary directory(s) are created and the name are
 * placed in environment variables. If a directory can not be made an
 * error code is returned.
 */

int
mktmpdir(
  job *pjob,
  struct var_table *vtab    /* pointer to variable table */
)
  {
  uid_t uid;
  gid_t gid;
  char *seq;
  char *tmpdir;
  char *pdir;
  int ret;
  task *ptask;
  int tasks;

  uid = pjob->ji_qs.ji_un.ji_momt.ji_exuid;
  gid = pjob->ji_qs.ji_un.ji_momt.ji_exgid;
  seq = pjob->ji_qs.ji_jobid;

  for (ptask = (task *)GET_NEXT(pjob->ji_tasks), tasks = 0;
       ptask != NULL;
       ptask = (task *)GET_NEXT(ptask->ti_jobtask), tasks++);

#if SRFS
  extern char *var_value(char *);

  tmpdir = tmpdirname(var_value("TMPDIR"), seq);

#else
  tmpdir = tmpdirname(TMP_DIR, seq);

#endif /* SRFS */
  if (tasks == 1)
    {
    if ((ret = makedir(tmpdir, uid, gid)) != 0)
      return ret;
    }

  bld_env_variables(vtab, "TMPDIR", tmpdir);

#if SRFS
  tmp_dir = strdup(tmpdir);

  if ((pdir = var_value("BIGDIR")) != NULL)
    {
    tmpdir = tmpdirname(pdir, seq);

    if (tasks == 1)
      {
      if ((ret = makedir(tmpdir, uid, gid)) != 0)
        return ret;
      }

    bld_env_variables(vtab, "BIGDIR", tmpdir);

    big_dir = strdup(tmpdir);
    }

  if ((pdir = var_value("FASTDIR")) != NULL)
    {
    tmpdir = tmpdirname(var_value("FASTDIR"), seq);

    if (tasks == 1)
      {
      if ((ret = makedir(tmpdir, uid, gid)) != 0)
        return ret;
      }

    bld_env_variables(vtab, "FASTDIR", tmpdir);

    fast_dir = strdup(tmpdir);
    }

#endif /* SRFS */

  return 0;
  }

void
rmonedir(char *tmpdir)
  {
  static char *id = "rmonedir";

  struct stat sb;
  int pid;
  char *rm = "/bin/rm";
  char *rf = "-rf";

  /* Hello, is any body there? */

  if (stat(tmpdir, &sb) == -1)
    {
    if (errno != ENOENT)
      {
      sprintf(log_buffer, "Unable to stat %s", tmpdir);
      log_err(errno, id, log_buffer);
      }

    return;
    }

  /* fork and exec the cleamtmp process */
  pid = fork();

  if (pid < 0)
    {
    log_err(errno, id, "Unable to fork");
    return;
    }

  if (pid > 0) return;

  execl(rm, "pbs_cleandir", rf, tmpdir, 0);

  log_err(errno, id, "This is really bad");

  exit(1);
  }

/*
 * rmtmpdir - remove the temporary directory
 * This may take awhile so the task is forked and execed to another
 * process.
 */

void
rmtmpdir(char *sequence)
#if SRFS
  {
  static char *id = "rmtmpdir";
  char *tmpdir;
  int pid, pstat;
  extern char *var_value(char *);

  pid = fork();

  if (pid < 0)
    {
    log_err(errno, id, "Unable to fork");
    return;
    }

  if (pid > 0)   /* parent */
    return;

  /* child */
  tmpdir = tmpdirname(var_value("TMPDIR"), sequence);

  rmonedir(tmpdir);

  wait(&pstat);

  tmpdir = tmpdirname(var_value("FASTDIR"), sequence);

  rmonedir(tmpdir);

  wait(&pstat);

  tmpdir = tmpdirname(var_value("BIGDIR"), sequence);

  rmonedir(tmpdir);

  wait(&pstat);

  exit(0);
  }
#else
  {
  static char *id = "rmtmpdir";
  char *tmpdir;

  /* Get tmpdir name */
  tmpdir = tmpdirname(TMP_DIR, sequence);
  rmonedir(tmpdir);
  }
#endif /* SRFS */

#define J_SETFL  2 /* Set job flags */
#define J_BATCH  000010 /* job is a batch job */

/*
 * set_job - set up a new job session
 *  Set session id and whatever else is required on this machine
 * to create a new job.
 */
int
set_job(job *pjob, struct startjob_rtn *sjr)
  {
  uid_t uid = pjob->ji_qs.ji_un.ji_momt.ji_exuid;

  if ((sjr->sj_session = setjob(uid, WJSIGNAL)) == -1)
    return (-1);

  if (sesscntl(sjr->sj_session, J_SETFL, J_BATCH) == -1)
    return (-1);

  return (sjr->sj_session);
  }

/*
** set_globid - set the global id for a machine type.
*/
void
set_globid(job *pjob, struct startjob_rtn *sjr)
  {
  extern char noglobid[];

  if (pjob->ji_globid == NULL)
    pjob->ji_globid = strdup(noglobid);
  }

/*
 * set_mach_vars - setup machine dependent environment variables
 */
int
set_mach_vars(job *pjob, struct var_table *vtab)
  {
  int  ret;

  /* Add names of temp directories to environment */
  ret = mktmpdir(pjob, vtab);

  switch (ret)
    {

    case 0:
      return 0;

    case 1:   /* old dirs exist, doing remove */
      rmtmpdir(pjob->ji_qs.ji_jobid);
      return JOB_EXEC_RETRY;

    default:
      return JOB_EXEC_FAIL1;
    }
  }

char *
set_shell(job *pjob, struct passwd *pwdp)
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

      if (cp)
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

  return (shell);
  }

/*
 * scan_for_terminated - scan the list of runnings jobs for one whose
 * session id matched that of a terminated child pid.  Mark that
 * job as Exiting.
 */

void
scan_for_terminated(void)
  {
  static char id[] = "scan_for_terminated";
  int  exiteval;
  pid_t  pid;
  int  sid;
  job  *pjob;
  task  *ptask;
  int  statloc;
  attribute *at;
  resource_def *rd;
  resource *pres;
  long  himem, himpp, hisds;


  /* update the latest intelligence about the running jobs;         */
  /* must be done before we reap the zombies, else we lose the info */

  termin_child = 0;

  if (mom_get_sample() == PBSE_NONE)
    {
    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob)
      {
      mom_set_use(pjob);
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }
    }

  while ((pid = waitpid(-1, &statloc, WNOHANG)) > 0)
    {
#ifdef DEBUG
    sprintf(log_buffer, "Waitpid = %d", pid);
    LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER,
              "PBS_MOM", log_buffer);
#endif
    /*
    ** see if process was a child doing a special function for MOM
    */
    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob)
      {
      if (pid == pjob->ji_momsubt)
        {
        if (WIFEXITED(statloc))
          statloc = WEXITSTATUS(statloc);
        else
          statloc = -1;

        if (pjob->ji_mompost)
          {
          pjob->ji_mompost(pjob, statloc);
          pjob->ji_mompost = 0;
          }

        pjob->ji_momsubt = 0;

        (void)job_save(pjob, SAVEJOB_QUICK);
        break;
        }

      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }
    }

  /* Now figure out which task(s) have terminated (are zombies) */

  while ((sid = wait_job(&himem, &himpp, &hisds)) > 0)
    {
    sprintf(log_buffer, "Waitjob = %d", sid);
    LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER,
              "PBS_MOM", log_buffer);

    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob)
      {
      ptask = (task *)GET_NEXT(pjob->ji_tasks);

      while (ptask)
        {
        if (ptask->ti_qs.ti_sid == sid)
          break;

        ptask = (task *)GET_NEXT(ptask->ti_jobtask);
        }

      if (ptask != NULL)
        break;

      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }

    if (pjob == NULL)
      {
      DBPRT(("%s: sid %d not a task\n", id, sid))
      continue;
      }

    at = &pjob->ji_wattr[(int)JOB_ATR_resc_used];

    at->at_flags |= (ATR_VFLAG_MODIFY & ATR_VFLAG_SET);

    rd = find_resc_def(svr_resc_def, "mem", svr_resc_size);
    assert(rd != NULL);
    pres = add_resource_entry(at, rd);
    pres->rs_value.at_flags |= ATR_VFLAG_SET;
    pres->rs_value.at_type = ATR_TYPE_LONG;
    pres->rs_value.at_val.at_size.atsv_shift = 10; /* in KB */
    pres->rs_value.at_val.at_long = himem << (BPCSHIFT - 10);

    rd = find_resc_def(svr_resc_def, "mppmem", svr_resc_size);
    assert(rd != NULL);
    pres = add_resource_entry(at, rd);
    pres->rs_value.at_flags |= ATR_VFLAG_SET;
    pres->rs_value.at_type = ATR_TYPE_LONG;
    pres->rs_value.at_val.at_size.atsv_shift = 10; /* in KB */
    pres->rs_value.at_val.at_long = himpp << (BPCSHIFT - 10);

    rd = find_resc_def(svr_resc_def, "sds", svr_resc_size);
    assert(rd != NULL);
    pres = add_resource_entry(at, rd);
    pres->rs_value.at_flags |= ATR_VFLAG_SET;
    pres->rs_value.at_type = ATR_TYPE_LONG;
    pres->rs_value.at_val.at_size.atsv_shift = 10; /* in KB */
    pres->rs_value.at_val.at_long = hisds << (BPCSHIFT - 10);

    /*
    ** We found task within the job which has exited.
    */

    if (WIFEXITED(statloc))
      exiteval = WEXITSTATUS(statloc);
    else if (WIFSIGNALED(statloc))
      exiteval = WTERMSIG(statloc) + 10000;
    else
      exiteval = 1;

    DBPRT(("%s: task %d sid %d exit value %d\n", id,
           ptask->ti_qs.ti_task, sid, exiteval))
    kill_task(ptask, SIGKILL, 0);

    ptask->ti_qs.ti_exitstat = exiteval;

    ptask->ti_qs.ti_status = TI_STATE_EXITED;

    task_save(ptask);

    sprintf(log_buffer, "task %d terminated", ptask->ti_qs.ti_task);

    LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB,
              pjob->ji_qs.ji_jobid, log_buffer);

    exiting_tasks = 1;
    }
  }


#define PTY_SIZE 12

/*
 * creat the master pty, this particular
 * piece of code does it the hard way, it searches
 */

int
open_master(
  char **rtn_name /* RETURN name of slave pts */
)
  {
  int  i = 0;
  int  ptc; /* master file descriptor */
  static char pty_name[PTY_SIZE+1]; /* "/dev/[pt]tyXY" */

  while (1)
    {
    (void)sprintf(pty_name, "/dev/pty/%03d", i++);

    if ((ptc = open(pty_name, O_RDWR | O_NOCTTY, 0)) >= 0)
      {
      /* Got a master, fix name to matching slave */
      pty_name[5] = 't';
      pty_name[6] = 't';
      pty_name[7] = 'y';
      pty_name[8] = 'p';
      *rtn_name = pty_name;
      return (ptc);

      }
    else if (errno == ENOENT)
      return (-1); /* tried all entries, give up */
    }
  }

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
  { "ILL", SIGILL },
  { "TRAP", SIGTRAP },
  { "ABRT", SIGABRT },
  { "IOT", SIGIOT },
  { "HWE", SIGHWE },
  { "ERR", SIGERR },
  { "EMT", SIGEMT },
  { "FPE", SIGFPE },
  { "KILL", SIGKILL },
  { "PRE", SIGPRE },
  { "BUS", SIGBUS },
  { "ORE", SIGORE },
  { "SEGV", SIGSEGV },
  { "SYS", SIGSYS },
  { "PIPE", SIGPIPE },
  { "ALRM", SIGALRM },
  { "TERM", SIGTERM },
  { "IO", SIGIO },
  { "URG", SIGURG },
  { "CHLD", SIGCHLD },
  { "PWR", SIGPWR },
  { "MT", SIGMT },
  { "MTKILL", SIGMTKILL },
  { "BUFIO", SIGBUFIO },
  { "RECOVERY", SIGRECOVERY },
  { "UME", SIGUME },
  { "DLK", SIGDLK },
  { "CPULIM", SIGCPULIM },
  { "SHUTDN", SIGSHUTDN },
  { "STOP", SIGSTOP },
  { "suspend", SIGSTOP },
  { "TSTP", SIGTSTP },
  { "CONT", SIGCONT },
  { "resume", SIGCONT },
  { "TTIN", SIGTTIN },
  { "TTOU", SIGTTOU },
  { "WINCH", SIGWINCH },
  { "RPE", SIGRPE },
  { "WRBKPT", SIGWRBKPT },
  { "NOBDM", SIGNOBDM },
  { "CRAY9", SIGCRAY9 },
  { "CRAY10", SIGCRAY10 },
  { "CRAY11", SIGCRAY11 },
  { "CRAY12", SIGCRAY12 },
  { "CRAY13", SIGCRAY13 },
  { "CRAY14", SIGCRAY14 },
  { "INFO", SIGINFO },
  { "USR1", SIGUSR1 },
  { "USR2", SIGUSR2 },
  {(char *)0, -1 }
  };

/*
 * post_suspend - post exit of child for suspending a job
 */

int post_suspend(

  job *pjob,
  int  err)

  {
  if (err == 0)
    {
    pjob->ji_qs.ji_substate = JOB_SUBSTATE_SUSPEND;
    pjob->ji_qs.ji_svrflags |= JOB_SVFLG_Suspend;
    }

  return(0);
  }




/*
 * post_resume - post exit of child for suspending a job
 */

int post_resume(

  job *pjob,
  int  err)

  {
  if (err == 0)
    {
    /*
     * adjust walltime for time suspended, ji_momstat contains
      * time when suspended, ji_stime when job started; adjust
     * stime to current time minus (back to) amount of time
     * used before suspended.
     */
    if (pjob->ji_qs.ji_svrflags & JOB_SVFLG_Suspend)
      {
      pjob->ji_qs.ji_stime = time_now -
                             (pjob->ji_momstat - pjob->ji_qs.ji_stime);
      }

    pjob->ji_qs.ji_substate = JOB_SUBSTATE_RUNNING;

    pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_Suspend;

    }

  return(0);
  }



