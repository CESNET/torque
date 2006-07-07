/* pam_pbssimpleauth module */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>

#include "portability.h"
#include "list_link.h"
#include "pbs_ifl.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"

#define MODNAME "pam_pbssimpleauth"

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#include <security/pam_modules.h>

#define PAM_GETPWNAM_R
#include <security/_pam_macros.h>

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                        const char **argv)
{
  int retval = PAM_SERVICE_ERR;
  const char *username;
  struct passwd pwd, *user_pwd;
  char *ubuf = NULL;
  size_t ubuflen;
  struct dirent *jdent;
  DIR *jobdir=NULL;
  int fp;
  struct job xjob;
  ssize_t amt;
  char jobpath[PATH_MAX+1];
  char jobdirpath[PATH_MAX+1];
  int debug=0;

  openlog(MODNAME,LOG_PID,LOG_USER);
  strcpy(jobdirpath,PBS_SERVER_HOME "/mom_priv/jobs");

  /* step through arguments */
  for (; argc-- > 0; ++argv)
    {
    if (!strcmp(*argv,"debug"))
      debug = 1;
    else if (!strcmp(*argv,"jobdir"))
      strncpy(jobdirpath,*argv,PATH_MAX);
    else
      syslog(LOG_ERR,"unknown option: %s",*argv);
    }

  if (debug) syslog(LOG_INFO,"opening %s",jobdirpath);

  if ((jobdir = opendir(jobdirpath)) == NULL)
    {
    if (debug) syslog(LOG_INFO,"failed to open jobs dir");
    closelog();
    return PAM_IGNORE;
    }

  /* get the username and passwd, allow uid 0 */
  retval = pam_get_user(pamh, &username, NULL);
  if (retval == PAM_CONV_AGAIN)
    {
    closelog();
    return PAM_INCOMPLETE;
    }

  if ((retval != PAM_SUCCESS) || !username)
    {
    syslog(LOG_ERR,"failed to retrieve username");
    closelog();
    return PAM_SERVICE_ERR;
    }

  if (_pam_getpwnam_r(username, &pwd, &ubuf, &ubuflen, &user_pwd) != 0)
    user_pwd = NULL;

  /* no early returns from this point on because we need to free ubuf */

  if (debug) syslog(LOG_INFO,"username %s, %s",username,user_pwd ? "known" : "unknown");

  if (!user_pwd)
    {
    retval = PAM_USER_UNKNOWN;
    }
  else if (user_pwd->pw_uid == 0)
    {
    if (debug) syslog(LOG_INFO,"allowing uid 0");
    retval = PAM_SUCCESS;
    }
  else
    {
    retval = PAM_AUTH_ERR;

    while ((jdent = readdir(jobdir)) != NULL)
      {
      if (strstr(jdent->d_name,".JB") == NULL)
        continue;

      snprintf(jobpath,PATH_MAX-1,"%s/%s/%s",PBS_SERVER_HOME,"mom_priv/jobs",jdent->d_name);
      if (debug) syslog(LOG_INFO,"opening %s",jobpath);

      fp = open(jobpath, O_RDONLY, 0);
      if (fp < 0)
        {
        syslog(LOG_ERR,"error opening job file");               
        continue;
        }

      amt = read(fp, &xjob.ji_qs, sizeof(xjob.ji_qs));
      if (amt != sizeof(xjob.ji_qs))
        {
        close(fp);
        syslog(LOG_ERR,"short read of job file");               
        continue;
        }

      if (xjob.ji_qs.ji_un_type != JOB_UNION_TYPE_MOM)
        {
        /* odd, this really should be JOB_UNION_TYPE_MOM */
        close(fp);
        syslog(LOG_ERR,"job file corrupt");               
        continue;
        }

      if (debug) syslog(LOG_INFO,"state=%d, substate=%d",xjob.ji_qs.ji_state,xjob.ji_qs.ji_substate);

      if (user_pwd->pw_uid == xjob.ji_qs.ji_un.ji_momt.ji_exuid)
        {
        /* success! */
        close(fp);
        if (debug) syslog(LOG_INFO,"allowed by %s",jdent->d_name);
        retval = PAM_SUCCESS;
        break;
        }

      close(fp);
      } /* END while (readdir(jobdir)) */

      if (jobdir)
        closedir(jobdir);
    }

  if (ubuf)
    free(ubuf);
    
  if (debug) syslog(LOG_INFO,"returning %s",retval==PAM_SUCCESS ? "success" : "failed");

  closelog();

  return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
                   const char **argv)
{
  return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                     const char **argv)
{
  return pam_sm_authenticate(pamh, flags, argc, argv);
}



#ifdef PAM_STATIC

/* static module data */
/* this only exists for the unlikely event that this built *with* libpam */
struct pam_module _pam_pbssimpleauth_modstruct = {
     "pam_pbssimpleauth",
     pam_sm_authenticate,
     pam_sm_setcred,
     pam_sm_acct_mgmt,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */
