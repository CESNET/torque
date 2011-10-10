
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

  if (argc!=4) {
      printf("bad number of parameters\n%s\n",
	     "delete_cache server|local hostname metric");
      return 1;
  }

  if (strcmp(argv[1],"local")==0) {
      stream=cache_connect_local();
  } else { 
      net_proto(NPROTO_AUTO); 
      stream=cache_connect_net(argv[1],PBS_CACHE_PORT);
  }
  if (stream!=NULL) {
      ret=cache_remove(stream,argv[2],argv[3]);
      if (ret)
	  printf("cache_remove failed\n");
      cache_close(stream);
      return ret;
  } else {
      printf("cache_connect failed: %s\n",strerror(errno));
      return 1;
  }

}

