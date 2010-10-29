
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "api.h"
#include "../net.h"

int main(int argc,char **argv)
{ 
  int ret;
  char *rets;
  char *c=NULL;

  if (argc!=4) {
      printf("bad number of parameters\n%s\n",
	     "local_test hostname metric value");
      return 1;
  }

  ret=cache_store_local(argv[1],argv[2],argv[3]);
  if (ret!=0) {
      printf("cache_store_local failed: %s\n",strerror(errno));
      return ret;
  }
 
  rets=pbs_cache_get_local(argv[1],argv[2]);
  if (rets==NULL) {
      printf("pbs_cache_get_local failed: %s\n",strerror(errno));
      return 1;
  }
  c=cache_value_only(rets);
  if ((c==NULL) || (strcmp(argv[3],c)!=0)) {
      printf("pbs_cache_get_local returned wrong value: %s\n",rets);
  }
  if (rets)
      free(rets);
  if (c)
      free(c);

  ret=cache_remove_local(argv[1],argv[2]);
  if (ret!=0) {
      printf("cache_remove_local failed: %s\n",strerror(errno));
      return ret;
  }

  return 0;
}

