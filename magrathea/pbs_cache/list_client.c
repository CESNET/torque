#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <search.h>
#include "api.h"
#include "../net.h"


int show_host(void *hash,char *host)
{ char *s;

  s=cache_hash_find(hash,host);
  if (s) {
      printf("%s->%s\n",host,s ? s:"NULL");
      free(s);
      return 0;
  }

  printf("%s not found\n",host);
  return 1;
}

int main(int argc,char **argv)
{ 
  FILE *stream;
  void *hash;
  int i;



  if (argc!=3) {
      printf("bad number of parameters\n%s\n",
	     "list_cache server metric");
      return 1;
  }

  net_proto(NPROTO_AUTO);

  for(i=0;i<3;i++) {
  stream=cache_connect_net(argv[1],PBS_CACHE_PORT);
  if (stream!=NULL) {

     hash=cache_hash_init();
     if (cache_hash_fill(stream,argv[2],hash)) {
	  printf("cache_list failed\n");
	   cache_close(stream);
	   return -1;
     }
     cache_close(stream);

     show_host(hash,"manwe-d.ics.muni.cz");
     show_host(hash,"odin.ics.muni.cz");
     show_host(hash,"erebor.ics.muni.cz");
     show_host(hash,"neznamy host");

     cache_hash_destroy(hash);
  } else {
     printf("cache_connect failed: %s\n",strerror(errno));
     return 1;
  }
  }

  return 0;
}
