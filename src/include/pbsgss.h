#ifndef _PBSGSS_H_
#define _PBSGSS_H_

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>
#include <stdio.h>

extern gss_buffer_t empty_token;

void pbsgss_display_status
	(char *msg, OM_uint32 maj_stat, OM_uint32 min_stat);
int pbsgss_send_token
	(int s, int flags, gss_buffer_t tok);
int pbsgss_recv_token
	(int s, int *flags, gss_buffer_t tok);
int pbsgss_can_get_creds();
int pbsgss_client_establish_context(int s,
				    char *service_name,
				    gss_cred_id_t creds,
				    gss_OID oid,
				    OM_uint32 gss_flags,
				    gss_ctx_id_t *gss_context,
				    OM_uint32 *ret_flags);
int pbsgss_client_authenticate(char *hostname, int psock, int delegate);
int pbsgss_server_establish_context(int s,
				    gss_cred_id_t server_creds, 
				    gss_cred_id_t *client_creds,
				    gss_ctx_id_t *context,
				    gss_buffer_t client_name,
				    OM_uint32 *ret_flags);

char *pbsgss_get_host_princname();
int pbsgss_server_acquire_creds(char *service_name,
				gss_cred_id_t *server_creds);

int pbsgss_save_creds (gss_cred_id_t client_creds,
		       char *principal,
		       char *ccname);

char *ccname_for_job(char *jobnamem, char *prefix);
int authenticate_as_job(char *jobname,int setpag);
int pbsgss_renew_creds (char *jobname, char *prefix);

/* Token types */
#define TOKEN_NOOP		(1<<0)
#define TOKEN_CONTEXT		(1<<1)
#define TOKEN_DATA		(1<<2)
#define TOKEN_MIC		(1<<3)

/* Token flags */
#define TOKEN_CONTEXT_NEXT	(1<<4)
#define TOKEN_WRAPPED		(1<<5)
#define TOKEN_ENCRYPTED		(1<<6)
#define TOKEN_SEND_MIC		(1<<7)


#endif
