#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "api.h"
#include "../net.h"

int main(int argc,char **argv)
{ 
  FILE *stream;
  int ret;

  if (argc!=3) {
      printf("bad number of parameters\n%s\n",
	     "list_cache server metric");
      return 1;
  }

  net_proto(NPROTO_AUTO);

  stream=cache_connect_net(argv[1],PBS_CACHE_PORT);
  if (stream!=NULL) {
     ret=cache_list(stream,argv[2],stdout);
     if (ret)
	  printf("cache_list failed\n");
     cache_close(stream);
     return ret;
  } else {
     printf("cache_connect failed: %s\n",strerror(errno));
     return 1;
  }

  return 0;
}

