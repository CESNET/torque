#ifndef RENEW_H_
#define RENEW_H_

#include <krb5.h>
#include <kafs.h>
#include <krb525.h>
#include <krb525_convert.h>


typedef struct eexec_job_info_t {
  int         expire_time;    /* updated according to ticket lifetime */
  time_t      endtime;        /* tickets expiration time */
  krb5_creds  creds;          /* User's TGT */
  krb5_ccache ccache;         /* User's credentials cache */
  uid_t job_uid;
  char *username;
  char *princ;
  char *realm;
  char *ccache_name;
  krb5_principal client;
} eexec_job_info_t, *eexec_job_info;


typedef struct krb_holder
  {
  int got_ticket;
  eexec_job_info job_info;
  krb5_context context;
  } krb_holder_t;


int init_ticket(job *pjob, task *ptask, eexec_job_info job_info, krb5_context* context);
int free_ticket(krb5_context* context, eexec_job_info job_info);


#endif /* RENEW_H_ */
