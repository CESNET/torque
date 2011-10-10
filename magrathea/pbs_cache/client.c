
#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"

struct conf conf;

int main(int argc,char **argv) 
{
    int s;
  struct sockaddr_un u;
  FILE *stream;
  char buf[1024];
  char *c;
  int count=0;
  int i;

  s=socket(PF_UNIX,SOCK_STREAM,0);
  u.sun_family=AF_UNIX;
  strcpy(u.sun_path,PBS_CACHE_ROOT PBS_CACHE_SOCK);
  connect(s,(struct sockaddr *)&u,sizeof(u));
  stream = fdopen(s, "w+");

  printf("Helo:\n");
  fgets(buf,1023,stream);
  printf("%s",buf);

  /* store magrathea status */
  printf("Add:\n");
  fprintf(stream,"add\t%s\t\%s\t\%s\n","erebor","magrathea","1:3");
  fgets(buf,1023,stream);
  printf("%s",buf);

  /* store magrathea status */
  printf("Add:\n");
  fprintf(stream,"add\t%s\t\%s\t\%s\n","odin","magrathea","0:53");
  fgets(buf,1023,stream);
  printf("%s",buf);

  /* get value of magrathea status */
  printf("Show:\n");
  fprintf(stream,"show\t%s\t%s\n","erebor","magrathea");
  fgets(buf,1023,stream);
  printf("%s",buf);

  /* list of all hosts */
  printf("List:\n");
  fprintf(stream,"list\t%s\n","magrathea");
  fgets(buf,1023,stream);
  printf("%s",buf);

  while(strncmp(buf,"201 OK",5)!=0) {
	fgets(buf,1023,stream);
	printf("%s",buf);
  }

  /* print help */
  printf("Help:\n");
  fprintf(stream,"help\n");
  fgets(buf,1023,stream);
  printf("%s",buf);

  c=strchr(buf,',');
  if (c!=NULL) {
      count=atoi(++c);
  }

  if (count>0) {
      for(i=0;i<count;i++) {
	  fgets(buf,1023,stream);
	  printf("%s",buf);
      }
      fgets(buf,1023,stream);
      printf("%s",buf);
  }

  printf("End:\n");
  fprintf(stream,"quit\n");
  //fgets(buf,1023,stream);
  //printf("%s",buf);
  fclose(stream);

  return 0;
}
