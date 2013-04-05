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

extern "C"
{
  #include "log.h"
  #include "libpbs.h"
  #include "server_limits.h"
}

static char    *logfile = (char *)0;
char path_log[_POSIX_PATH_MAX];
char path_acct[_POSIX_PATH_MAX];

// C++ helper
void xlog_record(int eventtype, int objclass, const char *objname, const char *text)
  {
  log_record(eventtype,objclass,const_cast<char*>(objname),const_cast<char*>(text));
  }

void die(int sig)
  {
  const char *id = "die";

  if (sig > 0)
    {
    sprintf(log_buffer, "caught signal %d", sig);

    xlog_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    }
  else
    {
    xlog_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, "abnormal termination");
    }

  log_close(1);

  exit(1);
  }  /* END die() */

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

  World world(argc,argv);
  world.run();

  return 0;
  }
