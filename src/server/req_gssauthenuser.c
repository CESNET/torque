#include <time.h>
#include <sys/utsname.h>
#include <pbs_config.h>   /* the master config generated by configure */
#include "dis.h"
#include "log.h"
#include "pbsgss.h"
#include "pbs_ifl.h"
#include "server_limits.h"
#include "net_connect.h"
#include "credential.h"
#include "batch_request.h"

extern struct connection svr_conn[PBS_NET_MAX_CONNECTIONS];
extern struct credential conn_credent[PBS_NET_MAX_CONNECTIONS];
gss_cred_id_t server_creds = GSS_C_NO_CREDENTIAL;
char *service_name = NULL;
time_t lastcredstime = 0;

/* this should be called on a socket after readauth() (in net_server.c) but
 * goes before process request.  It copies the principal from the svr_conn 
 * structure (in net_server) to conn_credent
 *
 * returns 0 on success and -1 on failure
 */
int gss_conn_credent (int sock) {
  char *client_name;
  int i, length;
  client_name = svr_conn[sock].principal;
  if (! client_name) {
    return -1;
  }
  for (i = 0; client_name[i] != '\0' && i < PBS_MAXUSER; i++) {
    if (client_name[i] == '@') {break;}
  }
  length = strlen(client_name);
  strncpy(conn_credent[sock].username,client_name,i);
  conn_credent[sock].username[i] = '\0';
  strncpy(conn_credent[sock].hostname,client_name + i + 1, length - i -1);
  conn_credent[sock].hostname[length - i -1] = '\0';
  return 0;
}

/* returns 0 on success and other values on failure */
int req_gssauthenuser (struct batch_request *preq, int sock) {
  gss_ctx_id_t context;
  gss_cred_id_t client_creds;
  gss_buffer_desc client_name;
  OM_uint32 ret_flags;
  time_t now;
  int status;

  /* if credentials are old, try to get new ones.  If we can't, keep the old
     ones since they're probably still valid and hope that we can get new credentials next time 
  */
  now = time((time_t *)NULL);
  if (now - lastcredstime > 60*60) {
    gss_cred_id_t new_creds;
    if (service_name == NULL) {
      struct utsname buf;
      if (uname(&buf) != 0) {
	perror("couldn't uname");
	req_reject(PBSE_BADCRED,0,preq,NULL,"couldn't get server nodename");
	return -1;
      }
      service_name = malloc(sizeof(buf.nodename) + 6);
      sprintf(service_name,"host@%s",buf.nodename);
    }

    /* if we can't get new creds, try again in a few minutes */
    if (pbsgss_server_acquire_creds(service_name,&new_creds) < 0) {
      log_err(0,"req_gssauthenuser","acquire creds didn't work");
      lastcredstime += 120; 
    } else {
      /* if we got new creds, free the old ones and use the new ones */
      lastcredstime = now;
      sprintf(log_buffer,"renewing server tickets at %d\n",now);
      log_event(PBSEVENT_DEBUG,
		PBS_EVENTCLASS_SERVER,"req_gssauthenuser",log_buffer);
      if (server_creds != GSS_C_NO_CREDENTIAL) {
	gss_release_cred(&ret_flags,server_creds);
      }
      server_creds = new_creds;
    }
    
  }
  if ((status = pbsgss_server_establish_context(sock, server_creds, &client_creds, &context,
						&client_name, &ret_flags)) < 0) {
    sprintf(log_buffer,"server_establish_context failed : %d",status);
    log_event(PBSEVENT_DEBUG,
              PBS_EVENTCLASS_SERVER,"req_gssauthenuser",log_buffer);
    req_reject(PBSE_BADCRED,0,preq,NULL,"establish context didn't work");
    return -1;    
  }
  if (context == GSS_C_NO_CONTEXT) {
    log_event(
	      PBSEVENT_DEBUG,
	      PBS_EVENTCLASS_SERVER,"req_gssauthenuser","Accepted unauthenticated connection.");
    req_reject(PBSE_BADCRED,0,preq,NULL,"got no context");
    return -1;
  }
  if (svr_conn[sock].principal != NULL) {
    free(svr_conn[sock].principal);
  }
  svr_conn[sock].principal = (char *)malloc(sizeof(char) * (1 + client_name.length));
  strncpy(svr_conn[sock].principal,client_name.value,client_name.length);
  svr_conn[sock].principal[client_name.length] = '\0';
  memcpy(&(svr_conn[sock].creds),&client_creds,sizeof(client_creds));
  svr_conn[sock].cn_authen = PBS_NET_CONN_AUTHENTICATED;
  gss_delete_sec_context(&ret_flags,&context,GSS_C_NO_BUFFER);
  return gss_conn_credent(sock);
}
