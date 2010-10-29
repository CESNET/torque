#include <stdio.h>
#include <stdlib.h>
#include "api.h"
#include "../net.h"

int main(int argc,char **argv) 
{ 
  FILE *stream;
  char *value;

  net_proto(NPROTO_AUTO);

  /* connect to local pbs_cache, store value, get value back, close */
  stream=cache_connect_local();

  if (stream!=NULL) {
     cache_store(stream,"erebor","magrathea","1:3");
     value=cache_get(stream,"erebor","magrathea");
     printf("%s\n",value);
     cache_close(stream);
  } else {
      printf("cache_connect failed\n");
  }


  /* and the same on network interface */
  stream=cache_connect_net("odin.ics.muni.cz",PBS_CACHE_PORT);

  if (stream!=NULL) {
     cache_store(stream,"erebor","magrathea","2:3");
     value=cache_get(stream,"erebor","magrathea");
     printf("%s\n",value);
     free(value);
     cache_close(stream);
  } else {
      printf("cache_connect failed\n");
  }

  return 0;
}

