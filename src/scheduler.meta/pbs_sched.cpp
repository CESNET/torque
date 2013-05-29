#include <pbs_config.h>

#include "SchedulerRealm.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <grp.h>
#include <errno.h>
#include <signal.h>

extern "C"
{
  #include "log.h"
  #include "libpbs.h"
  #include "server_limits.h"
}

static char    *logfile = (char *)0;
char path_log[_POSIX_PATH_MAX];
char path_acct[_POSIX_PATH_MAX];
// global graceful exit guard
bool scheduler_not_dying;
void graceful_exit_handler(int) { scheduler_not_dying = false; }

// C++ helpers
void xlog_record(int eventtype, int objclass, const char *objname, const char *text)
  {
  log_record(eventtype,objclass,const_cast<char*>(objname),const_cast<char*>(text));
  }

void xlog_err(int errnum, const char* routine, const char *text)
  {
  log_err(errnum,const_cast<char*>(routine),const_cast<char*>(text));
  }

int pid_file_fd;

/** \brief Lock and update sched.lock file
 *
 * @return 0 on success 1 on failure
 */
int update_pid_file()
  {
  int pid_file_fd = open("sched.lock", O_CREAT | O_TRUNC | O_WRONLY, 0644);

  if (pid_file_fd < 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "unable to open lock file");
    return 1;
    }

  struct flock pid_file_lock;
  pid_file_lock.l_type   = F_WRLCK;
  pid_file_lock.l_whence = SEEK_SET;
  pid_file_lock.l_start  = 0;
  pid_file_lock.l_len    = 0; /* whole file */

  if (fcntl(pid_file_fd, F_SETLK, &pid_file_lock) < 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "unable to lock the lock file for writing");
    return 1;
    }

  pid_t  pid = getpid();
  sprintf(log_buffer, "%ld\n", (long)pid);
  if (write(pid_file_fd, log_buffer, strlen(log_buffer) + 1) != (ssize_t)(strlen(log_buffer) + 1))
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "unable to write pid into lock file");
    return 1;
    }

  return 0;
  }

/** \brief Unlock the sched.lock file on exit
 *
 * @return 0 on success 1 on failure
 */
int release_pid_file()
  {
  struct flock pid_file_lock;
  pid_file_lock.l_type   = F_UNLCK;
  pid_file_lock.l_whence = SEEK_SET;
  pid_file_lock.l_start  = 0;
  pid_file_lock.l_len    = 0; /* whole file */

  if (fcntl(pid_file_fd, F_SETLK, &pid_file_lock) < 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "unable to unlock the lock file");
    return 1;
    }

  return 0;
  }

/** \brief Setup signal handlers for graceful exit
 *
 * @return 0 on success 1 on failure
 */
int setup_signals()
  {
  struct sigaction act;
  act.sa_flags = 0;
  if (sigemptyset(&act.sa_mask) != 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "unable to initialize sigset");
    return 1;
    }

  act.sa_handler = graceful_exit_handler;

  if (sigaction(SIGHUP, &act, NULL) != 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "couldn't setup handler for SIGHUP");
    return 1;
    }

  if (sigaction(SIGINT, &act, NULL) != 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "couldn't setup handler for SIGINT");
    return 1;
    }

  if (sigaction(SIGTERM, &act, NULL) != 0)
    {
    xlog_err(errno, __PRETTY_FUNCTION__, "couldn't setup handler for SIGTERM");
    return 1;
    }

  signal(SIGPIPE, SIG_IGN);

  return 0;
  }

int main(int argc, char *argv[])
  {
  // Global setup
  strcpy(pbs_current_user, "Scheduler");
  msg_daemonname = strdup("pbs_sched");

  if ((geteuid() != 0) || (getuid() != 0))
    {
    fprintf(stderr, "%s: Must be run by root\n", argv[0]);
    return 1;
    }

  const char *homedir = "/var/spool/torque";
  sprintf(log_buffer, "%s/sched_priv", homedir);

  // Security check
  int c;
  c  = chk_file_sec(log_buffer, 1, 0, S_IWGRP | S_IWOTH, 1, NULL);
  c |= chk_file_sec("/var/spool/torque/pbs_environment", 0, 0, S_IWGRP | S_IWOTH, 0, NULL);
  if (c != 0) exit(1);

  // Daemonize and change working dir to /.../sched_priv
  if (getenv("PBSDEBUG") == NULL)
  if (daemon(1,0) == -1)
    {
    fprintf(stderr,"Couldn't daemonize using \"daemon(int,int)\".");
    return 1;
    }

  if (chdir(log_buffer) == -1)
    {
    perror("chdir");
    return 1;
    }

  sprintf(path_log,   "%s/sched_logs", homedir);
  sprintf(path_acct,   "%s/%s", log_buffer, PBS_ACCT);

  umask(022);

  if (setup_env("/var/spool/torque/pbs_environment") == -1)
    return 1;

  c = getgid();
  setgroups(1, (gid_t *)&c); /* secure suppl. groups */

  struct rlimit rlimit;
  rlimit.rlim_cur = RLIM_INFINITY;
  rlimit.rlim_max = RLIM_INFINITY;
  setrlimit(RLIMIT_CPU,   &rlimit);
  setrlimit(RLIMIT_FSIZE, &rlimit);
  setrlimit(RLIMIT_DATA,  &rlimit);
  setrlimit(RLIMIT_STACK, &rlimit);
#ifdef  RLIMIT_RSS
  setrlimit(RLIMIT_RSS  , &rlimit);
#endif  /* RLIMIT_RSS */
#ifdef  RLIMIT_VMEM
  setrlimit(RLIMIT_VMEM  , &rlimit);
#endif  /* RLIMIT_VMEM */

  if (log_open(logfile, path_log) == -1)
    {
    fprintf(stderr, "%s: logfile could not be opened\n", argv[0]);
    return 1;
    }

  // setup pid file
  if (update_pid_file() != 0)
    return 1;

  // setup signal handling
  if (setup_signals() != 0)
    return 1;

  scheduler_not_dying = true;
  World world(argc,argv);

  while (scheduler_not_dying)
    {
    world.run(); // exits on unexpected exception
    if (scheduler_not_dying)
      sleep(5*60); // sleep for 5 minutes, then try again
    }

  // unlock the pid file
  if (release_pid_file() != 0)
    return 1;

  return 0;
  }
