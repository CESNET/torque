#include <pbs_config.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <com_err.h>
#include <krb5.h>
#include <kafs.h>
#include <krb525.h>
#include <krb525_convert.h>
#include <krb5-protos.h>
#include <math.h>

#include "libpbs.h"
#include "portability.h"
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "resmon.h"
#include "pbs_job.h"
#include "log.h"
#include "rpp.h"
#include "mom_mach.h"
#include "mom_func.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "net_connect.h"
#include "dis.h"
#include "batch_request.h"
#include "md5.h"
#include "mcom.h"
#include "resource.h"
#include "renew.h"

enum {   MAXTRIES  = 60 };

enum {   TIME_RESERVE  = 1*60*60,     /* 1 hour */
	 TIME_SLEEP    = 2*60,     /* 2 minutes */
	 TIME_LIFETIME = 8*60*60   /* 8 hours   */
};

extern  char            *path_jobs;
extern struct var_table vtable;

#define PBS_SERVICE ("pbs")

char     *error_msg = NULL;
static int received_signal = -1;


int krb_log_file;

int
get_job_info_from_job(job *pjob, task *ptask, eexec_job_info job_info)
{
  char                 *qsub_msg = NULL;
  char                 *id = "get_job_info_from_job";
  char                 *ccname;
  krb5_context         context;

  qsub_msg = pjob->ji_wattr[(int)JOB_SITE_ATR_krb_princ].at_val.at_str;
  if (qsub_msg == NULL) {
     /* no ticket found */
     log_err(-1, id, "no ticket found");
     return -2; 
  }

  /* job_info.pid = getpid(); */
  job_info->job_uid = pjob->ji_qs.ji_un.ji_momt.ji_exuid;
  job_info->username = strdup(pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str);
  if (ptask == NULL)
    {
    asprintf(&ccname, "FILE:/tmp/krb5cc_pbsjob_%s_jobwide",
        pjob->ji_qs.ji_jobid);
    }
  else
    {
    asprintf(&ccname, "FILE:/tmp/krb5cc_pbsjob_%s_%ld",
        pjob->ji_qs.ji_jobid, (long)ptask->ti_qs.ti_task);
    }
  job_info->ccache_name = ccname;

  krb5_init_context(&context);
  krb5_parse_name(context,qsub_msg,&job_info->client);
  job_info->princ=strdup(qsub_msg);
  job_info->realm=strdup(krb5_principal_get_realm(context,job_info->client));
  krb5_free_context(context);

  return(0);
}

int
get_job_info_from_principal(char *principal, char* jobid, task *ptask, eexec_job_info job_info)
  {
  char                 *qsub_msg = NULL;
  char                 *id = "get_job_info_from_principal";
  char                 *ccname;
  krb5_context         context;
  char                 login[PBS_MAXUSER+1];
  char                 *c;
  struct passwd *pw;

  memset(login,0,PBS_MAXUSER+1);
  if ((qsub_msg = principal) == NULL)
  { log_err(-1, id, "no ticket provided"); return -2; }

  strcpy(login,principal);
  if ((c = strchr(login,'@')) != NULL)
  { c[0] = '\0'; }

  if ((pw = getpwnam(login)) == NULL)
  { log_err(-1,id,"couldn't find user in /etc/passwd"); return -2; }

  job_info->job_uid = pw->pw_uid;
  job_info->username = strdup(login);
  asprintf(&ccname, "FILE:/tmp/krb5cc_pbsjob_%s_jobwide",jobid);
  job_info->ccache_name = ccname;

  krb5_init_context(&context);
  krb5_parse_name(context,qsub_msg,&job_info->client);
  job_info->princ=strdup(qsub_msg);
  job_info->realm=strdup(krb5_principal_get_realm(context,job_info->client));
  krb5_free_context(context);

  return(0);
  }

krb5_error_code
get_ticket(krb5_context context, eexec_job_info job_info)
{
  static char error_text[1024];
  krb5_error_code ret = 0;
  krb5_principal  client, server;
  krb5_creds      creds, *my_creds;
  krb5_ccache     ccache;
  time_t         now;

  memset(&creds, 0, sizeof(creds));
  memset(&my_creds, 0, sizeof(my_creds));
  memset(&job_info->creds, 0, sizeof(job_info->creds));

  job_info->endtime = 0;

  /* get our principal */
  if((ret=krb5_sname_to_principal(context, NULL, PBS_SERVICE, KRB5_NT_SRV_HST, &client))) {
    error_msg = "getting client principal";
    return(ret);
  }

  /* create TGS principal */
  if((ret = krb5_make_principal(context, &server, client->realm, 
			       KRB5_TGS_NAME, client->realm, NULL))) {
    error_msg = "creating TGS principal";
    return(ret);
  }

  /* Initialize memory cache */
  if((ret = krb5_cc_gen_new(context, &krb5_mcc_ops, &ccache))) {
    error_msg = "creating credentials cache";
    return(ret);
  }
  if((ret = krb5_cc_initialize(context, ccache, client))) {
    error_msg = "initializing credentials cache";
    goto out;
  }

  /* get TGT using krb5_get_in_tkt_with_keytab() */
  if((ret=krb5_copy_principal(context, client, &creds.client))) {
    error_msg = "copying client principal";
    goto out;
  }
  creds.server = server;

  creds.flags.b.forwardable = krb5_config_get_bool(context, NULL,
                              "libdefaults", "forwardable", NULL);

  /* default ticket lifetime will be TIME_LIFETIME */
  krb5_timeofday(context, &now);
  creds.times.endtime = now + TIME_LIFETIME;
  {
    krb5_preauthtype pa_types[] = { KRB5_PADATA_ENC_TIMESTAMP, KRB5_PADATA_NONE };
    int b;
    krb5_addresses no_addrs = {0, NULL};

    krb5_appdefault_boolean(context, NULL, NULL, "no-addresses", FALSE, &b);

    if((ret=krb5_get_in_tkt_with_keytab(context,
				       creds.flags.i,    /* options */
				       b ? &no_addrs : NULL, /* addresses */
				       NULL, /* encryption types */
				       pa_types, /* preauthentication types */
				       NULL, /* keytab */
				       ccache,
				       &creds,
				       NULL))) {  /* returned AS_REP */
      error_msg = "getting TGT";
      goto out;
    }
  }


  /* convert our TGT to users TGT:
   *   source ticket: pbs/<hostname>@LOCAL_REALM for krbtgt/REALM@REALM
   *   dest.  ticket: user@REALM for krbtgt/REALM@REALM
   * we have
   *       pbs/<hostname>@LOCAL_REALM for krbtgt/LOCAL_REALM@LOCAL_REALM
   */

  
  memset(&creds, 0, sizeof(creds));
  creds.client = client;

  /* Create TGS principal (possibly in another realm) */
  if((ret = krb5_make_principal(context, &server, job_info->realm, 
			       KRB5_TGS_NAME, job_info->realm, NULL))) {
    error_msg = "creating TGS principal";
    goto out;
  }
  if((ret = krb5_copy_principal(context, server, &creds.server))) {
    error_msg = "copying server principal";
    goto out;
  }

  {
     krb5_kdc_flags flags;

     flags.i = 0;
     flags.b.forwardable = krb5_config_get_bool(context, NULL,
	                              "libdefaults", "forwardable", NULL);

     /* Get credentials */
     if((ret = krb5_get_credentials_with_flags(context, 0, flags,
					      ccache, &creds, &my_creds))) {
       error_msg = "getting credentials to be converted";
       goto out;
     }
  }

  /* set target client */
  job_info->creds.client = job_info->client;

  /* set target server */
  job_info->creds.server = server;

  ret = krb525_get_creds_ccache(context, ccache, my_creds, &job_info->creds);
  if(ret) {
    error_msg = error_text;
    snprintf(error_text, sizeof(error_text), "converting credentials\nError text: %s", krb525_convert_error);
    goto out;
  }
  
  job_info->endtime = job_info->creds.times.endtime;

 out:
  krb5_cc_destroy(context, ccache);

  return(ret);
}


krb5_error_code
store_ticket(krb5_context context, eexec_job_info job_info)
{
  krb5_error_code  ret;

  if((ret = krb5_cc_resolve(context, job_info->ccache_name, &job_info->ccache))) {
    error_msg = "resolving cache name";
    return(ret);
  }

  if((ret = krb5_cc_initialize(context, job_info->ccache, job_info->creds.client))) {
    error_msg = "initializing cache";
    return(ret);
  }

  if((ret = krb5_cc_store_cred(context, job_info->ccache, &job_info->creds))) {
    error_msg = "storing credentials";
    return(ret);
  }

  return(0);
}


krb5_error_code
do_afslog(krb5_context context, eexec_job_info job_info)
{
  krb5_error_code ret = 0;

  if(k_hasafs())
    if((ret = krb5_afslog(context, job_info->ccache, NULL, NULL))) {
      error_msg = "logging into AFS";
      /* ignore this error */
      ret = 0;
    }
  return(ret);
}

static krb5_error_code
get_renewed_creds(krb5_context context, eexec_job_info job_info)
{  krb5_error_code ret;
  
   /* sleep(100); */
   /* Get TGT for user */
   ret = get_ticket(context, job_info);
   if(ret) {
      /* error_msg set inside*/
      return ret;
      }

   /* Go user */
   if(seteuid(job_info->job_uid) < 0) {
     error_msg = "setting user privileges";
     return errno;
   }

   /* Store TGT */
   if((ret = store_ticket(context, job_info))) {
      seteuid(0);return ret;
   } 
   /* Login to AFS cells */
   if((ret = do_afslog(context, job_info))) {
      seteuid(0);return ret;
   } 
   /* Go root */
   if(seteuid(0) < 0) {
     error_msg = "setting root privileges";
     return errno;
   }
   return 0;
}

static void
register_signal(int signal)
{
   received_signal = signal;
}

/** Initialize a kerberos ticket
 *
 *  @param pjob Parent job
 *  @param ptask Parent task
 *  @param job_info Job info to be filled
 *  @param context Context to be filled
 *  @return -2 if principal is present, -3 if get_job_info failed, -4 if krb5_init_context failed, -5 if get_renewed_creds failed
 */
int init_ticket(job *pjob, char* principal, char *jobid, task *ptask, eexec_job_info job_info, krb5_context* context)
  {
  int ret;
  char buf[512];
  static char *id = "init_ticket";

  memset(job_info, 0, sizeof(*job_info));

  if (pjob != NULL)
    {
    if ((ret = get_job_info_from_job(pjob, ptask, job_info)) != 0)
      {
      snprintf(buf, sizeof(buf), "get_job_info_from_job returned %d",ret);
      log_err(errno, id, buf);
      if (ret == -2)
        return -2;
      else
        return -3;
      }
    }
  else
    {
    if ((ret = get_job_info_from_principal(principal, jobid, ptask, job_info)) != 0)
      {
      snprintf(buf, sizeof(buf), "get_job_info_from_principal returned %d",ret);
      log_err(errno, id, buf);
      if (ret == -2)
        return -2;
      else
        return -3;
      }
    }


  if (k_hasafs())
      k_setpag();

  if((ret = krb5_init_context(context)) != 0)
    {
    log_err(-1, id, "initializing krb5 context");
    return -4;
    }

  if ((ret = get_renewed_creds(*context, job_info)) != 0)
    {
    krb5_free_context(*context);
    snprintf(buf, sizeof(buf), "get_renewed_creds returned %d, %s, %s",ret, error_message(ret), error_msg);
    log_err(errno, id, buf);
    return -5;
    }

  bld_env_variables(&vtable, "KRB5CCNAME", job_info->ccache_name);

  return 0;
  }

/** Free a kerberos ticket
 *
 * @param context Context of the ticket
 * @param job_info Job info (for ticketfile name)
 * @return always 0
 */
int free_ticket(krb5_context* context, eexec_job_info job_info)
  {
  if(job_info->ccache)
    {
    krb5_cc_destroy(*context, job_info->ccache);
    unlink(job_info->ccache_name);
    }

  if(k_hasafs())
    k_unlog();

  return 0;
  }

int
do_renewal(krb5_context context, eexec_job_info job_info)
{
   krb5_error_code      ret = 0;
   time_t              now;
   char *id = "do_renewal";
   struct sigaction sa;
   char  log_msg[256];
   int errs = 0;

   memset(&sa,0,sizeof(sa));
   sa.sa_handler = register_signal;
   sigaction(SIGTERM, &sa, NULL);
   sa.sa_handler = SIG_IGN;
   sigaction(SIGHUP, &sa, NULL);

   while (received_signal == -1) {
     krb5_timeofday(context, &now);

     if(now < job_info->endtime - TIME_RESERVE) {
        /* syslog(LOG_DEBUG, "Now is %u, ticket valid till %u, sleeping", now, 
	       job_info->endtime); */
        sleep(TIME_SLEEP);
     } else {
      
        /* syslog(LOG_DEBUG, "Now is %u, ticket valid till %u, renewing", now, 
	      job_info->endtime); */
      
        ret = get_renewed_creds(context, job_info);

        if (ret != 0)
          {
          if (errs < 5) errs++;
          sleep((unsigned)pow(5,errs));
          }
        else
          {
          errs = 0;
          }
     }
   }

  if(ret) {
     snprintf(log_msg, sizeof(log_msg), 
              "credential renewal failed, exiting renewal: %d (%s) when %s", ret, 
	      error_message(ret), error_msg);
     log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, id, log_msg);
  }
  else {
     snprintf(log_msg, sizeof(log_msg), "received signal %d, exiting renewal",
	      received_signal);
     log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, id, log_msg);
  }

  free_ticket(&context, job_info);

  return(ret);
}

int
check_user_creds(eexec_job_info job_info, char *filename)
{
   struct stat cache_info;

   if(stat(filename, &cache_info) < 0) {
     /* no tickets found */
     return -1;
   }

   if (job_info->job_uid != cache_info.st_uid) {
      unlink(filename); /* XXX */
      return -1;
   }

  return 0;

}

int 
start_renewal(task *ptask, int fd1, int fd2)
{
  eexec_job_info_t     _job_info;
  eexec_job_info       job_info = &_job_info;
  krb5_context         context;
  char                 *id = "start_renewal";
  int                  pid;
  int		       ret;
  int fd = -1;
  char pid_file[MAXPATHLEN];
  char buf[512];
  job                  *pjob = ptask->ti_job;

  if (*pjob->ji_qs.ji_fileprefix != '\0')
     snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid",
           path_jobs, pjob->ji_qs.ji_fileprefix,
	   (long)ptask->ti_qs.ti_task);
  else
     snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid",
           path_jobs, pjob->ji_qs.ji_jobid, 
	   (long)ptask->ti_qs.ti_task);

  fd = open(pid_file, O_CREAT|O_EXCL|O_WRONLY, 0600);
  if (fd == -1) {
     /* another renewal process is running ? */
     snprintf(buf, sizeof(buf), "opening PID file for renewal process (%s) uid = %d",
             pid_file, getuid());
     log_err(errno, id, buf);
     return -1;
  }

  ret = init_ticket(pjob, NULL, NULL, ptask, job_info, &context);
  if (ret == -2) /* job without a principal */
    {
    close(fd);
    return 0;
    }
  if (ret) /* error */
    {
    close(fd);
    return ret;
    }
     
  pid = fork();
  if (pid < 0) {
     if(job_info->ccache) {
	krb5_cc_destroy(context, job_info->ccache);
	unlink(job_info->ccache_name);
     }
     log_err(errno, id, "fork() failed");
     close(fd);
     return -1;
  }
  if (pid > 0) {
    snprintf(buf, sizeof(buf), "%d\n", pid);
    ret = write(fd, buf, strlen(buf));
    if (ret == -1) {
       log_err(errno, id, "writing pid failed");
       goto out;
    }

    log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, 
	       id, "renewal started");
    ret = 0;

out:
    if (ret == -1) {
       if(job_info->ccache) {
	  krb5_cc_destroy(context, job_info->ccache);
	  unlink(job_info->ccache_name);
       }
       unlink(pid_file);
    }
    if (fd != -1)
      {
      fsync(fd);
      close(fd);
      }
       
    return ret;
  }

  close(fd);
  if (fd1 >= 0)
	  close(fd1);
  if (fd2 >= 0)
	  close(fd2);
  if (setsid() != 0)
  	log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, id, "could not setsid");
  do_renewal(context, job_info);

  exit(0);
}

void
stop_renewal(task *ptask)
{
   char pid_file[1024];
   struct stat cache_info;
   FILE *fd;
   int pid;
   job *pjob = ptask->ti_job;
   
   if (pjob == NULL)
   /* || pjob->ji_renewal_pid == 0 */
      return;

   if (*pjob->ji_qs.ji_fileprefix != '\0')
      snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid",
	    path_jobs, pjob->ji_qs.ji_fileprefix,
	    (long)ptask->ti_qs.ti_task);
   else
      snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid",
	    path_jobs, pjob->ji_qs.ji_jobid, 
	    (long)ptask->ti_qs.ti_task);

   fd = fopen(pid_file, "r");
   if (fd == NULL)
      return;

   if (fscanf(fd, "%d", &pid) < 1) {
     pid = -1;
   }

   fclose(fd);

   if (pid >= 0)
     kill(pid, SIGTERM);

   if (stat(pid_file, &cache_info) == 0) {
      unlink(pid_file);
   }
}
