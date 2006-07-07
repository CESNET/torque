#include <pbs_config.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dis.h"
#include "net_connect.h"

/* returns the full hostname for an address, or null if it can't be
 * found.  The caller is responsible for freeing the string 
 */ 
char *get_hostnamefromaddr(pbs_net_t hostaddr) {
  struct hostent *phe;
  struct in_addr addr;
  char *ret;

  addr.s_addr = htonl((uint32_t) hostaddr);
  
  phe = gethostbyaddr(&addr, sizeof(addr), AF_INET);
  if (phe == NULL) {
    perror("gethostbyaddr");
    return NULL;
  }
  ret = malloc(sizeof(char) * (strlen(phe->h_name) + 1));
  if (ret) {
    strcpy(ret,phe->h_name);
  }
  return ret;
}
			   

