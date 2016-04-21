#ifndef _PBSGSS_H_
#define _PBSGSS_H_

#include <gssapi.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern gss_buffer_t empty_token;

void pbsgss_display_status(const char *msg, OM_uint32 maj_stat, OM_uint32 min_stat);
int pbsgss_can_get_creds();
int pbsgss_server_establish_context(int s, gss_cred_id_t server_creds, gss_cred_id_t *client_creds, gss_ctx_id_t *context, gss_buffer_t client_name, OM_uint32 *ret_flags);
int pbsgss_server_acquire_creds(char *service_name, gss_cred_id_t *server_creds);
int pbsgss_save_sec_context(gss_ctx_id_t *context, OM_uint32 flags, int fd);
const char *pbsgss_get_host_princname();
int pbsgss_client_authenticate(char *hostname, int psock, int delegate, int wrap);

#ifdef __cplusplus
}
#endif

/* Token types */
#define TOKEN_NOOP              (1<<0)
#define TOKEN_CONTEXT           (1<<1)
#define TOKEN_DATA              (1<<2)
#define TOKEN_MIC               (1<<3)

/* Token flags */
#define TOKEN_CONTEXT_NEXT      (1<<4)
#define TOKEN_WRAPPED           (1<<5)
#define TOKEN_ENCRYPTED         (1<<6)
#define TOKEN_SEND_MIC          (1<<7)


#define PBSGSS_OK 0x0
// Could not convert given name into internal form
#define PBSGSS_ERR_IMPORT_NAME 0x0001
#define PBSGSS_ERR_ACQUIRE_CREDS 0x002
#define PBSGSS_ERR_INTERNAL 0x003
#define PBSGSS_ERR_WRAPSIZE 0x004
#define PBSGSS_ERR_CONTEXT_DELETE 0x005
#define PBSGSS_ERR_CONTEXT_SAVE 0x006
#define PBSGSS_ERR_IMPORT 0x007
#define PBSGSS_ERR_IMPORTNAME 0x008
#define PBSGSS_ERR_CONTEXT_INIT 0x009
#define PBSGSS_ERR_READ 0x010
#define PBSGSS_ERR_READ_TEMP 0x011
#define PBSGSS_ERR_SENDTOKEN 0x012
#define PBSGSS_ERR_RECVTOKEN 0x013
#define PBSGSS_ERR_ACCEPT_TOKEN 0x014
#define PBSGSS_ERR_NAME_CONVERT 0x015
#define PBSGSS_ERR_NO_KRB_PRINC 0x016
#define PBSGSS_ERR_NO_USERNAME 0x017
#define PBSGSS_ERR_USER_NOT_FOUND 0x018
#define PBSGSS_ERR_CANT_OPEN_FILE 0x019
#define PBSGSS_ERR_KILL_RENEWAL_PROCESS 0x020
#define PBSGSS_ERR_GET_CREDS 0x021

#endif

