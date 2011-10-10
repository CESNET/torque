
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

  if (argc!=5) {
      printf("bad number of parameters\n%s\n",
	     "update_cache server hostname metric value");
      return 1;
  }

  net_proto(NPROTO_AUTO);

  stream=cache_connect_net(argv[1],PBS_CACHE_PORT);
  if (stream!=NULL) {
      ret=cache_store(stream,argv[2],argv[3],argv[4]);
      if (ret)
	  printf("cache_store failed\n");
      cache_close(stream);
      return ret;
  } else {
      printf("cache_connect failed: %s\n",strerror(errno));
      return 1;
  }

}

