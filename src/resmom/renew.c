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

#include <krb5.h>
#include <kafs.h>
#include <krb525.h>
#include <krb525_convert.h>
#include <krb5.h>
#include <kafs.h>
#include <krb525.h>
#include <krb525_convert.h>
#include <krb5-protos.h>
#include <com_err.h>

typedef struct eexec_job_info_t {
  int         expire_time;    /* updated according to ticket lifetime */
  time_t      endtime;        /* tickets expiration time */
  krb5_creds  *creds;          /* User's TGT */
  krb5_ccache ccache;         /* User's credentials cache */
  uid_t job_uid;
  char *username;
  char *princ;
  char *realm;
  char *ccache_name;
  krb5_principal client;
} eexec_job_info_t, *eexec_job_info;

struct krb_holder
  {
  int got_ticket;
  eexec_job_info_t job_info_;
  eexec_job_info job_info;
  krb5_context context;
  };


enum {   MAXTRIES  = 60 };

enum {   TIME_RESERVE  = 1*60*60,     /* 1 hour */
	 TIME_SLEEP    = 2*60,     /* 2 minutes */
	 TIME_LIFETIME = 8*60*60   /* 8 hours   */
};

extern  char            *path_jobs; // job directory path
extern struct var_table vtable;

#define PBS_SERVICE ("pbs")

// local functions
static int get_job_info_from_job(const job *pjob, const task *ptask, eexec_job_info job_info);
static int get_job_info_from_principal(const char *principal, const char* jobid, eexec_job_info job_info);
static krb5_error_code get_ticket(struct krb_holder *ticket, char *errbuf, size_t errbufsz);
static krb5_error_code get_renewed_creds(struct krb_holder *ticket, char *errbuf, size_t errbufsz);

int init_ticket(struct krb_holder *ticket);

int init_ticket_from_req(char *principal, char *jobid, struct krb_holder *ticket)
  {
  int ret;
  char buf[512];

  if ((ret = get_job_info_from_principal(principal, jobid, ticket->job_info)) != 0)
    {
    snprintf(buf, sizeof(buf), "Could not fetch GSSAPI information from principal (get_job_info_from_principal returned %d).",ret);
    log_err(errno, "init_ticket_from_req", buf);
    return ret;
    }

  ret = init_ticket(ticket);
  if (ret == 0)
    {
    ticket->got_ticket = 1;
    }

  return ret;
  }

int init_ticket_from_job(job *pjob, const task *ptask, struct krb_holder *ticket)
  {
  int ret;
  char buf[512];

  if ((ret = get_job_info_from_job(pjob, ptask, ticket->job_info)) != 0)
    {
    snprintf(buf, sizeof(buf), "Could not fetch GSSAPI information from job (get_job_info_from_job returned %d).",ret);
    log_err(errno, "init_ticket_from_job", buf);
    return ret;
    }

  ret = init_ticket(ticket);
  if (ret == 0)
    {
    ticket->got_ticket = 1;
    }

  return ret;
  }

/** Initialize a kerberos ticket
 *
 *  @param pjob Parent job
 *  @param ptask Parent task
 *  @param job_info Job info to be filled
 *  @param context Context to be filled
 *  @return -2 if principal is present, -3 if get_job_info failed, -4 if krb5_init_context failed, -5 if get_renewed_creds failed
 */
int init_ticket(struct krb_holder *ticket)
  {
  int ret;
  char buf[512];

  //static char *id = "init_ticket";

  if (k_hasafs())
      k_setpag();

  if((ret = krb5_init_context(&ticket->context)) != 0)
    {
    log_err(ret, "krb5 - init_ticket", "Failed to initialize context.");
    return PBSGSS_ERR_CONTEXT_INIT;
    }

  if ((ret = get_renewed_creds(ticket,buf,512)) != 0)
    {
    char buf2[512];

    krb5_free_context(ticket->context);
    snprintf(buf2, sizeof(buf2), "get_renewed_creds returned %d, %s",ret, buf);
    log_err(errno, "init_ticket", buf2);
    return PBSGSS_ERR_GET_CREDS;
    }

  bld_env_variables(&vtable, "KRB5CCNAME", ticket->job_info->ccache_name);

  return 0;
  }

krb5_error_code do_afslog(krb5_context context, eexec_job_info job_info)
  {
  krb5_error_code ret = 0;

  if(k_hasafs() && (ret = krb5_afslog(context, job_info->ccache, NULL, NULL)) != 0)
    {
    /* ignore this error */
    ret = 0;
    }

  return(ret);
  }

/** @brief Store a ticket
 *
 * @param ticket Ticket with information to store
 * @return KRB5 error code
 */
krb5_error_code store_ticket(struct krb_holder *ticket, char *errbuf, size_t errbufsz)
  {
  krb5_error_code  ret;

  if((ret = krb5_cc_resolve(ticket->context, ticket->job_info->ccache_name, &ticket->job_info->ccache)))
    {
    snprintf(errbuf,errbufsz,"krb5_store_ticket - Could not resolve cache name \"krb5_cc_resolve()\" : %s.",error_message(ret));
    return(ret);
    }

  if((ret = krb5_cc_initialize(ticket->context, ticket->job_info->ccache, ticket->job_info->creds->client)))
    {
    snprintf(errbuf,errbufsz,"krb5_store_ticket - Could not initialize cache \"krb5_cc_resolve()\" : %s.",error_message(ret));
    return(ret);
    }

  if((ret = krb5_cc_store_cred(ticket->context, ticket->job_info->ccache, ticket->job_info->creds)))
    {
    snprintf(errbuf,errbufsz,"krb5_store_ticket - Could not store credentials initialize cache \"krb5_cc_resolve()\" : %s.",error_message(ret));
    return(ret);
    }

  return(0);
  }

/** Get and store renewed credentials for a given ticket
 *
 * @param ticket Ticket for which to get and store credentials
 * @param errbuf Error buffer
 * @param errbufsz Size of error buffer
 * @return 0 on success, error code on failure
 */
static krb5_error_code get_renewed_creds(struct krb_holder *ticket, char *errbuf, size_t errbufsz)
  {
  krb5_error_code ret;
  char strerrbuf[512];

  /* Get TGT for user */
  if ((ret = get_ticket(ticket,errbuf,errbufsz)) != 0)
    {
    // error message set inside of get_ticket
    return ret;
    }

   /* Go user */
  if(seteuid(ticket->job_info->job_uid) < 0)
    {
    strerror_r(errno,strerrbuf,512);
    snprintf(errbuf,errbufsz,"krb5_get_renewed_creds - Could not set uid using \"setuid()\": %s.",strerrbuf);
    return errno;
    }

   /* Store TGT */
  if((ret = store_ticket(ticket,errbuf,errbufsz)))
    {
    seteuid(0);
    return ret;
    }

   /* Login to AFS cells */
  if((ret = do_afslog(ticket->context, ticket->job_info)))
    {
    seteuid(0);
    return ret;
    }

   /* Go root */
  if(seteuid(0) < 0)
    {
    strncpy(errbuf,"krb5_get_renewed_creds - Could reset root priviledges.",errbufsz);
    return errno;
    }

  return 0;
  }

/** Acquire a user ticket from the host ticket using krb525
 *
 * @param ticket Ticket to acquire
 * @param errbuf Error buffer
 * @param errbufsz Size of error buffer
 * @return krb5 error code
 */
static krb5_error_code get_ticket(struct krb_holder *ticket, char *errbuf, size_t errbufsz)
  {
  krb5_principal  pbs_service, local_tgs, remote_tgs;
  krb5_ccache     ccache;
  krb5_keytab     keytab = NULL;
  krb5_error_code ret = 0;

  //ticket->job_info.ccache
  //ticket->job_info.creds
  //ticket->job_info.client

  char *name = NULL;

  /* get our principal */
  if((ret=krb5_sname_to_principal(ticket->context, NULL, PBS_SERVICE, KRB5_NT_SRV_HST, &pbs_service)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't get client principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    return(ret);
  }

  /* create TGS principal */
  if((ret = krb5_make_principal(ticket->context, &local_tgs, pbs_service->realm, KRB5_TGS_NAME, pbs_service->realm, NULL)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't create principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    krb5_free_principal(ticket->context,pbs_service);
    return(ret);
    }

  /* Create memory cache */
  if((ret = krb5_cc_new_unique(ticket->context, krb5_mcc_ops.prefix, NULL, &ccache)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't create credential cache - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    krb5_free_principal(ticket->context,pbs_service);
    krb5_free_principal(ticket->context,local_tgs);
    return(ret);
    }

  /* Initialize the cache */
  if((ret = krb5_cc_initialize(ticket->context, ccache, pbs_service)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't initialize credentials cache - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  if ((ret = krb5_kt_default(ticket->context, &keytab)) != 0)
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't get default keytab - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  if ((ret = krb5_unparse_name(ticket->context, local_tgs, &name)) != 0)
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't un-parse client name - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

    { // get credentials and store them in the cache
    krb5_creds *creds;
    // initialize local credential variable
    creds = malloc(sizeof(krb5_creds));
    if (creds == NULL)
      return PBSGSS_ERR_INTERNAL;
    memset(creds,0,sizeof(krb5_creds));

    creds->flags.b.forwardable =
      krb5_config_get_bool(ticket->context, NULL, "libdefaults", "forwardable", NULL);

    krb5_get_init_creds_opt *opts;
    krb5_get_init_creds_opt_alloc(ticket->context,&opts);
    krb5_get_init_creds_opt_set_tkt_life(opts,TIME_LIFETIME);
    krb5_get_init_creds_opt_set_renew_life(opts,TIME_LIFETIME);
    krb5_get_init_creds_opt_set_forwardable(opts,creds->flags.b.forwardable);

    krb5_boolean b;
    krb5_appdefault_boolean(ticket->context, NULL, NULL, "no-addresses", FALSE, &b);
    krb5_get_init_creds_opt_set_addressless(ticket->context,opts,b);

    krb5_preauthtype pa_types[] = { KRB5_PADATA_ENC_TIMESTAMP, KRB5_PADATA_NONE };
    krb5_get_init_creds_opt_set_preauth_list(opts,pa_types,2);


    if ((ret = krb5_get_init_creds_keytab(ticket->context, creds, pbs_service, keytab, 0, name, opts)) != 0)
      {
      const char *krb5_err = krb5_get_error_message(ticket->context,ret);
      snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't initialize credentials - (%s)",krb5_err);
      krb5_free_error_message(ticket->context,krb5_err);

      krb5_free_creds(ticket->context,creds);
      krb5_get_init_creds_opt_free(ticket->context,opts);
      goto out;
      }

    if ((krb5_cc_store_cred(ticket->context, ccache, creds)) != 0)
      {
      const char *krb5_err = krb5_get_error_message(ticket->context,ret);
      snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't store credentials - (%s)",krb5_err);
      krb5_free_error_message(ticket->context,krb5_err);

      krb5_free_creds(ticket->context,creds);
      krb5_get_init_creds_opt_free(ticket->context,opts);
      goto out;
      }

    krb5_free_creds(ticket->context,creds);
    krb5_get_init_creds_opt_free(ticket->context,opts);
    }


  /* convert our TGT to users TGT:
   *   source ticket: pbs/<hostname>@LOCAL_REALM for krbtgt/REALM@REALM
   *   dest.  ticket: user@REALM for krbtgt/REALM@REALM
   * we have pbs/<hostname>@LOCAL_REALM for krbtgt/LOCAL_REALM@LOCAL_REALM
   */

  krb5_creds *creds;
  // initialize local credential variable
  creds = malloc(sizeof(krb5_creds));
  if (creds == NULL)
    return PBSGSS_ERR_INTERNAL;
  memset(creds,0,sizeof(krb5_creds));

  if((ret = krb5_copy_principal(ticket->context, pbs_service, &creds->client)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't copy client principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  /* Create TGS principal (possibly in another realm) */
  if((ret = krb5_make_principal(ticket->context, &remote_tgs, ticket->job_info->realm, KRB5_TGS_NAME, ticket->job_info->realm, NULL)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't construct server principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  if((ret = krb5_copy_principal(ticket->context, remote_tgs, &creds->server)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't copy server principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  krb5_creds *my_creds = NULL;

  krb5_kdc_flags flags;
  flags.i = 0;
  flags.b.forwardable = krb5_config_get_bool(ticket->context, NULL, "libdefaults", "forwardable", NULL);

  /* Get credentials */
  if((ret = krb5_get_credentials_with_flags(ticket->context, 0, flags, ccache, creds, &my_creds)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't get server credentials - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  /* set target client */
  if((ret = krb5_copy_principal(ticket->context, ticket->job_info->client, &ticket->job_info->creds->client)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't copy client principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  /* set target server */
  if((ret = krb5_copy_principal(ticket->context, remote_tgs, &ticket->job_info->creds->server)))
    {
    const char *krb5_err = krb5_get_error_message(ticket->context,ret);
    snprintf(errbuf,errbufsz,"krb5_get_ticket - couldn't copy server principal - (%s)",krb5_err);
    krb5_free_error_message(ticket->context,krb5_err);
    goto out;
    }

  if ((ret = krb525_get_creds_ccache(ticket->context, ccache, my_creds, ticket->job_info->creds)) != 0)
    {
    snprintf(errbuf,errbufsz, "krb5_get_ticket - converting credentials; Error text: %s", krb525_convert_error);
    goto out;
    }

  ticket->job_info->endtime = ticket->job_info->creds->times.endtime;

out:
  krb5_free_creds(ticket->context,creds);
  krb5_free_creds(ticket->context,my_creds);
  free(name);
  krb5_kt_close(ticket->context, keytab);
  krb5_free_principal(ticket->context,pbs_service);
  krb5_free_principal(ticket->context,local_tgs);
  krb5_free_principal(ticket->context,remote_tgs);
  krb5_cc_destroy(ticket->context, ccache);

  return(ret);
}

/** Get ccname from ticket
 *
 * @param ticket Ticket to extract ccname from
 * @return ccname
 */
char *get_ticket_ccname(struct krb_holder *ticket)
  {
  if (ticket == NULL || ticket->job_info == NULL)
    return NULL;

  return ticket->job_info->ccache_name;
  }

static volatile sig_atomic_t received_signal = 0;

static void register_signal(int signal)
  {
  received_signal = signal;
  }

/** Kerberos ticket renewal loop
 *
 * @param ticket Ticket to be periodically renewed
 * @return PBSGSS_OK (errors are logged into syslog)
 */
static int do_renewal(struct krb_holder *ticket)
  {
  // initialize signal catcher
  struct sigaction sa;
  memset(&sa,0,sizeof(sa));
  sa.sa_handler = register_signal;
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &sa, NULL);


  int errs = 0;
  while (received_signal == 0)
    {
    time_t now;
    if (krb5_timeofday(ticket->context, &now) != 0)
      {
      syslog(LOG_ERR, "KRB5 Renewal process failed for %s. krb5_timeofday() failed in pid %d.", ticket->job_info->ccache_name, getpid());
      continue;
      }

    if(now < ticket->job_info->endtime - TIME_RESERVE)
      {
      sleep(TIME_SLEEP);
      continue;
      }

    char errbuf[512];
    size_t errbufsz = 512;

    int ret = get_renewed_creds(ticket,errbuf,errbufsz);
    if (ret != PBSGSS_OK)
      {
      syslog(LOG_ERR, "KRB5 Renewal process failed for %s. get_renewed_creds() failed in pid %d. Error: %s", ticket->job_info->ccache_name, getpid(), errbuf);

      if (errs < 5) errs++;

      static const unsigned sleep_times[] = { 5, 15, 30, 120, 300, 300 };
      sleep(sleep_times[errs]);
      }
    else
      {
      errs = 0;
      }
    }

  free_ticket(ticket);

  return PBSGSS_OK;
  }

/** @brief Start the ticket renewal process
 *
 * @param ptask Task for which to start the process
 * @param fd1 process write file descriptor
 * @param fd2 process read file descriptor
 * @return PBSGSS_OK on success, PBSGSS_ERR_* otherwise
 */
int start_renewal(const task *ptask, int fd1, int fd2)
  {
  if (ptask == NULL)
    return PBSGSS_ERR_INTERNAL;

  job *pjob = ptask->ti_job;

  char pid_file[MAXPATHLEN];
  if (*pjob->ji_qs.ji_fileprefix != '\0')
     snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid", path_jobs, pjob->ji_qs.ji_fileprefix, (long)ptask->ti_qs.ti_task);
  else
     snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid", path_jobs, pjob->ji_qs.ji_jobid, (long)ptask->ti_qs.ti_task);

  int fd = open(pid_file, O_CREAT|O_EXCL|O_WRONLY, 0600);
  if (fd == -1)
    {
    char buf[512];
    /* another renewal process is running ? */
    snprintf(buf, sizeof(buf), "opening PID file for renewal process (%s) uid = %d", pid_file, getuid());
    log_err(errno, "krb5_start_renewal", buf);
    return PBSGSS_ERR_CANT_OPEN_FILE;
    }

  struct krb_holder *ticket = alloc_ticket();
  if (ticket == NULL)
    return PBSGSS_ERR_INTERNAL;

  int ret = init_ticket_from_job(pjob,ptask,ticket);
  if (ret == PBSGSS_ERR_NO_KRB_PRINC) /* job without a principal */
    {
    close(fd); // not an error, but do not start renewal
    return 0;
    }

  if (ret != 0) /* error */
    {
    close(fd);
    return ret;
    }


  int pid = fork();
  if (pid < 0)
    {
    if(ticket->job_info->ccache)
      {
      krb5_cc_destroy(ticket->context, ticket->job_info->ccache);
      unlink(ticket->job_info->ccache_name);
      }

    log_err(errno, "krb5_start_renewal", "fork() failed");
    close(fd);
    return PBSGSS_ERR_INTERNAL;
    }

  if (pid > 0)
    {
    char buf[512];
    snprintf(buf, sizeof(buf), "%d\n", pid);
    ret = write(fd, buf, strlen(buf));
    if (ret == -1)
      {
      log_err(errno, "krb5_start_renewal", "writing pid failed");
      goto out;
      }

    log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, "krb5_start_renewal", "renewal started");
    ret = PBSGSS_OK;

out:
    if (ret == -1)
      {
      if(ticket->job_info->ccache)
        {
        krb5_cc_destroy(ticket->context, ticket->job_info->ccache);
        unlink(ticket->job_info->ccache_name);
        }
      unlink(pid_file);

      ret = PBSGSS_ERR_INTERNAL;
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
  	log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, "krb5_start_renewal", "could not setsid");

  // start the actual renewal process
  do_renewal(ticket);

  exit(0);
}

/** Stop the renewal process for a given task
 *
 * @param ptask Task for which the renewal process should be stopped
 * @return PBSGSS_OK on success, PBSGSS_ERR_* on error
 */
int stop_renewal(const task *ptask)
  {
  if (ptask == NULL)
    return PBSGSS_ERR_INTERNAL;

  job *pjob = ptask->ti_job;
  if (pjob == NULL)
    return PBSGSS_ERR_INTERNAL;

  char pid_file[1024];
  if (*pjob->ji_qs.ji_fileprefix != '\0')
    snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid", path_jobs, pjob->ji_qs.ji_fileprefix, (long)ptask->ti_qs.ti_task);
  else
    snprintf(pid_file, sizeof(pid_file), "%s%s_renew_%ld.pid", path_jobs, pjob->ji_qs.ji_jobid, (long)ptask->ti_qs.ti_task);

  FILE *fd;
  fd = fopen(pid_file, "r");
  if (fd == NULL)
    return PBSGSS_ERR_CANT_OPEN_FILE;

  int pid = 0;
  if (fscanf(fd, "%d", &pid) < 1)
    {
    pid = -1;
    }

  fclose(fd);

  if (pid >= 0)
    {
    if (kill(pid, SIGTERM) != 0)
      return PBSGSS_ERR_KILL_RENEWAL_PROCESS;
    }
  else
    {
    return PBSGSS_ERR_KILL_RENEWAL_PROCESS;
    }

  struct stat cache_info;

  if (stat(pid_file, &cache_info) == 0)
    {
    unlink(pid_file);
    }

  return PBSGSS_OK;
  }

/** @brief Determine whether the supplied ticket was initialized
 *
 * @param ticket Ticket to be tested
 * @return \c TRUE if ticket is valid, \c FALSE otherwise
 */
int got_ticket(struct krb_holder *ticket)
  {
  return ticket->got_ticket;
  }

/** @brief Allocated a new krb_holder structure
 *
 * @return Allocated structure on success, NULL otherwise
 */
struct krb_holder *alloc_ticket()
  {
  struct krb_holder *ticket = (struct krb_holder*)(calloc(1,sizeof(struct krb_holder)));
  if (ticket == NULL)
    return NULL;

  ticket->job_info = &ticket->job_info_;

  ticket->job_info->creds = malloc(sizeof(krb5_creds));
  memset(ticket->job_info->creds,0,sizeof(krb5_creds));

  return ticket;
  }

/** Free a kerberos ticket
 *
 * @param ticket Ticket with context and job info information
 * @return always 0
 */
int free_ticket(struct krb_holder *ticket)
  {
  // TODO Error Handling
  if (ticket == NULL)
    return 0;

  if (ticket->got_ticket)
    {
    if (ticket->job_info->ccache)
      {
      krb5_cc_destroy(ticket->context,ticket->job_info->ccache);
      unlink(ticket->job_info->ccache_name);
      }

    krb5_free_creds(ticket->context,ticket->job_info->creds);
    krb5_free_principal(ticket->context,ticket->job_info->client);
    krb5_free_context(ticket->context);

    if (k_hasafs())
      k_unlog();
    }

  free(ticket->job_info->ccache_name);
  free(ticket->job_info->princ);
  free(ticket->job_info->realm);
  free(ticket->job_info->username);

  free(ticket);
  return 0;
  }

/** @brief Get job information for job structure
 *
 * @param pjob Pointer to the job structure
 * @param ptask Optional pointer to the task associated with this process
 * @param job_info (out) filled job information
 * @return \c PBSGSS_OK on success respective PBSGSS_ERR_* on error
 */
static int get_job_info_from_job(const job *pjob, const task *ptask, eexec_job_info job_info)
  {
  char *principal = NULL;

  if (pjob->ji_wattr[(int)JOB_SITE_ATR_krb_princ].at_val.at_str != NULL)
    principal = strdup(pjob->ji_wattr[(int)JOB_SITE_ATR_krb_princ].at_val.at_str);
  else
    {
    log_err(-1, "krb5_get_job_info_from_job", "No ticket found on job.");
    return PBSGSS_ERR_NO_KRB_PRINC;
    }

  if (principal == NULL) // memory allocation error
    return PBSGSS_ERR_INTERNAL;

  size_t len;
  char *ccname = NULL;
  if (ptask == NULL)
    {
    len = snprintf(NULL,0,"FILE:/tmp/krb5cc_pbsjob_%s_jobwide", pjob->ji_qs.ji_jobid);
    ccname = (char*)(malloc(len+1));
    if (ccname != NULL)
      snprintf(ccname,len+1,"FILE:/tmp/krb5cc_pbsjob_%s_jobwide", pjob->ji_qs.ji_jobid);
    }
  else
    {
    len = snprintf(NULL,0,"FILE:/tmp/krb5cc_pbsjob_%s_%ld", pjob->ji_qs.ji_jobid, (long)ptask->ti_qs.ti_task);
    ccname = (char*)(malloc(len+1));
    if (ccname != NULL)
      snprintf(ccname,len+1,"FILE:/tmp/krb5cc_pbsjob_%s_%ld", pjob->ji_qs.ji_jobid, (long)ptask->ti_qs.ti_task);
    }

  if (ccname == NULL) // memory allocation error
    {
    free(principal);
    return PBSGSS_ERR_INTERNAL;
    }

  if (pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str == NULL)
    {
    free(principal);
    free(ccname);
    return PBSGSS_ERR_NO_USERNAME;
    }

  char *username = strdup(pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str);
  if (username == NULL)
    {
    free(principal);
    free(ccname);
    return PBSGSS_ERR_INTERNAL;
    }

  krb5_context context;
  krb5_init_context(&context);
  krb5_parse_name(context,principal,&job_info->client);

  const char * realm = krb5_principal_get_realm(context,job_info->client);
  if (realm == NULL || (job_info->realm = strdup(realm)) == NULL)
    {
    free(principal);
    free(ccname);
    free(username);
    krb5_free_context(context);
    return PBSGSS_ERR_INTERNAL;
    }

  krb5_free_context(context);

  job_info->princ = principal;
  job_info->ccache_name = ccname;
  job_info->username = username;

  job_info->job_uid = pjob->ji_qs.ji_un.ji_momt.ji_exuid;

  return PBSGSS_OK;
}

/** @brief Fill in job info from a principal
 *
 * @param principal Principal for which to construct job info
 * @param jobid Job ID for which to construct job info
 * @param job_info Job Info to be filled in
 * @returns \c PBSGSS_OK on success, PBSGSS_ERR_* on error
 */
static int get_job_info_from_principal(const char *principal, const char* jobid, eexec_job_info job_info)
  {
  if (principal == NULL)
    {
    log_err(-1, "krb5_get_job_info_from_principal", "No principal provided.");
    return PBSGSS_ERR_NO_KRB_PRINC;
    }

  char *princ = strdup(principal);
  if (princ == NULL)
    return PBSGSS_ERR_INTERNAL;

  char login[PBS_MAXUSER+1];
  strncpy(login,principal,PBS_MAXUSER+1);
  char *c = strchr(login,'@');
  if (c != NULL)
    *c = '\0';

  // get users uid
  struct passwd pwd;
  struct passwd *result;
  char *buf;
  long int bufsize;

  bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize == -1)          /* Value was indeterminate */
    bufsize = 16384;          /* Should be more than enough */

  if ((buf = (char*)(malloc(bufsize))) == NULL)
    {
    free(princ);
    return PBSGSS_ERR_INTERNAL;
    }

  int ret = getpwnam_r(login, &pwd, buf, bufsize, &result);
  if (result == NULL)
    {
    free(princ);
    free(buf);
    if (ret == 0)
      return PBSGSS_ERR_USER_NOT_FOUND;
    else
      return PBSGSS_ERR_INTERNAL;
    }

  uid_t uid = pwd.pw_uid;
  free(buf);

  char *username = strdup(login);
  if (username == NULL)
    {
    free(princ);
    return PBSGSS_ERR_INTERNAL;
    }

  size_t len;
  char *ccname;
  len = snprintf(NULL,0,"FILE:/tmp/krb5cc_pbsjob_%s_jobwide",jobid);
  ccname = (char*)(malloc(len+1));
  if (ccname != NULL)
    snprintf(ccname,len+1,"FILE:/tmp/krb5cc_pbsjob_%s_jobwide",jobid);

  if (ccname == NULL)
    {
    free(princ);
    free(username);
    }

  krb5_context context;
  krb5_init_context(&context);
  krb5_parse_name(context,principal,&job_info->client);

  const char * realm = krb5_principal_get_realm(context,job_info->client);
  if (realm == NULL || (job_info->realm = strdup(realm)) == NULL)
    {
    free(princ);
    free(ccname);
    free(username);
    return PBSGSS_ERR_INTERNAL;
    }

  krb5_free_context(context);

  job_info->princ = princ;
  job_info->job_uid = uid;
  job_info->username = username;
  job_info->ccache_name = ccname;

  return PBSGSS_OK;
  }