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
     int retval = PAM_AUTH_ERR;
     const char *username;
     struct passwd pwd, *user_pwd;
     char *ubuf = NULL;
     size_t ubuflen;
     struct dirent *jdent;
     DIR *jobdir=NULL;
     int fp;
     struct job xjob;
     ssize_t amt;

       /*struct dirent *readdir(DIR *dir);*/

     if ((jobdir = opendir(PBS_SERVER_HOME "/mom_priv/jobs")) != NULL)
       {
       /* root can still log in; lusers cannot */
       if ((pam_get_user(pamh, &username, NULL) != PAM_SUCCESS)
           || !username)
         return PAM_SERVICE_ERR;

       if (_pam_getpwnam_r(username, &pwd, &ubuf, &ubuflen, &user_pwd) != 0)
	       user_pwd = NULL;
       if (user_pwd && user_pwd->pw_uid == 0)
         {
         retval = PAM_SUCCESS;
         }
       else
         {
	 if (!user_pwd)
           {
	   retval = PAM_USER_UNKNOWN;
	   }
         else
           {
	   retval = PAM_AUTH_ERR;

           while ((jdent = readdir(jobdir)) != NULL)
             {
             if (strstr(jdent->d_name,".JB") == NULL)
               continue;

             fp = open(jdent->d_name, O_RDONLY, 0);
             if (fp < 0)
               continue;

             amt = read(fp, &xjob.ji_qs, sizeof(xjob.ji_qs));
             if (amt != sizeof(xjob.ji_qs)) {
               close(fp);
               continue;
               }

             if (xjob.ji_qs.ji_un_type != JOB_UNION_TYPE_MOM)
               {
               /* odd, this really should be JOB_UNION_TYPE_MOM */
               close(fp);
               continue;
               }
 
             if (user_pwd->pw_uid == xjob.ji_qs.ji_un.ji_momt.ji_exuid)
               {
               /* success! */
               close(fp);
               retval= PAM_SUCCESS;
               break;
               }

             close(fp);
             }
           if (jobdir)
             closedir(jobdir);
	   }
         }
       }

     if (ubuf) {
	 free(ubuf);
     }
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
     return pam_sm_authenticate(pamh, 0, argc, argv);
}



#ifdef PAM_STATIC

/* static module data */

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
