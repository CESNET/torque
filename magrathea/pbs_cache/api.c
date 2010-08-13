#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <search.h>
#include <signal.h>


#include "../net.h"
#include "api.h"
#ifndef PBS_CACHE_API_STORE_ONLY
# include "ght_hash_table.h"
#endif

/* copy from memory.h */
#define M_ANY 0
#define M_REMOTE 1
#define M_GANGLIA 2
#define M_PBS 3
#define M_CACHE 4
#define M_UPDATE 5
#define M_DIST 6

/*
 * cache_connect_net 
 * connect to remote pbs_cache server, using TCP
 */
FILE *cache_connect_net(char *hostname,int port)
{ int s;
  FILE *stream;
  char buf[1024];

  s = net_connect_name(hostname, port, 0);
  if (s == -1) {
      return NULL;
  }

  stream = fdopen(s, "w+");
  if (stream!=NULL) {
      setvbuf(stream, NULL,_IONBF, 0);
      fgets(buf,1023,stream);
  }
  else
      close(s);
  return stream;
}

/*
 * cache_connect_local
 * connect to pbs_cache server listening on local unix socket
 */
FILE *cache_connect_local()
{ int s;
  FILE *stream;
  struct sockaddr_un u;
  int ret;
  char buf[1024];

  s=socket(PF_UNIX,SOCK_STREAM,0);
  if (s==-1)
     return NULL;
  u.sun_family=AF_UNIX;
  strcpy(u.sun_path,PBS_CACHE_ROOT PBS_CACHE_SOCK);
  ret=connect(s,(struct sockaddr *)&u,sizeof(u));
  if (ret==-1) {
      close(s);
      return NULL;
  }
  stream = fdopen(s, "w+");
  if (stream!=NULL) {
      if (setvbuf(stream, NULL,_IONBF, 0)!=0) {
	  close(s);
	  return NULL;
      }
      if (fgets(buf,1023,stream)==NULL) {
	  close(s);
	  return NULL;
      }
  }
  else
      close(s);
  return stream;
}

/* 
 * cache_close
 * close connection to pbs_cache, works both for local and remote connection
 */
void cache_close(FILE *stream)
{
  if (stream)
     fclose(stream);
}

/*
 * cache_store
 * update metric name, for host hostname, with value
 * connection must be setup with cache_connect_{local,net}
 */
int cache_store(FILE *stream,char *hostname,char *name,char *value)
{ char buf[1024];

  if (stream==NULL) 
      return 1;

  fprintf(stream,"add\t%s\t\%s\t\%s\n",hostname,name,value);
  fgets(buf,1023,stream);

  if (strncmp(buf,"201 OK add",10)==0)
      return 0;
  else
      return 1;
} 

/* 
 * cache_store_local
 * update metric name, for host hostname, with value
 * function opens local connection internally
 */
int cache_store_local(char *hostname,char *name,char *value)
{ FILE *stream;
  struct sigaction sa, osa;
  int ret=1;

  if (sigaction(SIGPIPE, NULL, &osa) == -1) 
      return 1;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler =SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE,&sa,NULL);

  stream=cache_connect_local();
  if (stream) {
      ret=cache_store(stream,hostname,name,value); 
      fclose(stream); 
  }
  sigaction(SIGPIPE,&osa,NULL);
  return ret;
}

/*
 * cache_remove
 * remove data for metric name and hostname
 * connection must be setup with cache_connect_{local,net}
 */
int cache_remove(FILE *stream,char *hostname,char *name)
{ char buf[1024];

  if (stream==NULL) 
      return 1;

  fprintf(stream,"del\t%s\t\%s\n",hostname,name);
  fgets(buf,1023,stream);

  if (strncmp(buf,"201 OK del",10)==0)
      return 0;
  else
      return 1;
} 

/* 
 * cache_remove_local
 * remove data for metric name and hostname
 * function opens local connection internally
 */
int cache_remove_local(char *hostname,char *name)
{ FILE *stream;
  struct sigaction sa, osa;
  int ret=1;

  if (sigaction(SIGPIPE, NULL, &osa) == -1) 
      return 1;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler =SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE,&sa,NULL);

  stream=cache_connect_local();
  if (stream) {
      ret=cache_remove(stream,hostname,name); 
      fclose(stream); 
  }
  sigaction(SIGPIPE,&osa,NULL);
  return ret;
}



/*
 * cache_get()
 * get value of metric name for host hostname
 * connection must be setup with cache_connect_{local,net}
 */
char *cache_get(FILE *stream,char *hostname,char *name)
{ char buf[1024];

  if (stream==NULL) 
      return NULL;

  fprintf(stream,"show\t%s\t\%s\n",hostname,name);
  fgets(buf,1023,stream);

  if (strncmp(buf,"201",3)!=0)
      return NULL;
  else
      return strdup(buf+4);
}

/*
 * pbs_cache_get_local()
 * get value of metric name for host hostname
 * connection to local cache is included in this function
 */
char *pbs_cache_get_local(char *hostname,char *name)
{ FILE *stream;
  struct sigaction sa, osa;
  char *rets=NULL;

  if (sigaction(SIGPIPE, NULL, &osa) == -1)
            return NULL;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler =SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE,&sa,NULL);

  stream=cache_connect_local();
  if (stream) {
      rets=cache_get(stream,hostname,name);
      fclose(stream);
  }
  sigaction(SIGPIPE,&osa,NULL);
  return rets;
}

/* 
 * cache_value_only()
 * returns only metric value from cache line (strips timestamp,newline)
 */
char *cache_value_only(char *old)
{ char *c;
  char *r=NULL;

  c=strchr(old,'\t');
  if (c!=NULL) {
      r=strdup(++c);
      c=strchr(r,'\n');
      if (c!=NULL)
	  *c='\0';
  }
  return r;
}

#ifndef PBS_CACHE_API_STORE_ONLY
/* 
 * cache_remote_metric()
 * verify, that metric is defined on pbs_cache server
 * returns whether type is expected one
 * connection must be setup with cache_connect_{local,net}
 */
int cache_remote_metric(FILE *stream,char *name,int type)
{ 
  char buf[1024];
  char rname[1024];
  char rserver[1024];
  int rtype;
  int ret;

  if (stream==NULL)
      return -1;

  fprintf(stream,"metric %s\n",name);
  fgets(buf,1023,stream);
  if (strncmp(buf,"205 metric ok",13)==0) {
      ret=-2;
      while(fgets(buf,1023,stream)!=NULL) {
	  if (strncmp(buf,"201 OK",6)==0) {
	      return ret;
	  }
	  sscanf(buf,"%s\t%d\t%s\n",rname,&rtype,rserver);

	  if (strcmp(name,rname)==0) {
	      if ((type==M_ANY) && (rtype!=M_UPDATE))
		  ret=0;
	      if ((type!=M_ANY) && (rtype==type))
		  ret=0;
	  }
      }
  }
  return -1;
}

void *cache_hash_init()
{ ght_hash_table_t *p_table;

  p_table = ght_create(128);
  return p_table;
}

char *cache_hash_find(void *p_table,char *key)
{ char *s;

  s=ght_get(p_table,sizeof(char)*strlen(key),key);
  if (s==NULL)
      return NULL;
  else
      return strdup(s);

}

void cache_hash_destroy(void *p_table)
{ ght_iterator_t iterator;
  const void *p_key;
  void *p_e;
 
  for(p_e = ght_first(p_table, &iterator, &p_key); p_e; p_e = ght_next(p_table, &iterator, &p_key)) { 
     free(p_e); 
  }
 
  ght_finalize(p_table);
}


static int cache_list_internal(FILE *stream,char *metric,FILE *ostream,void *p_table)
{ char buf[1024];

  if (stream==NULL)
            return -1;

  fprintf(stream," list %s\n",metric);
  fgets(buf,1023,stream);
  if (strncmp(buf,"205 list ok",11)==0) {
      while(fgets(buf,1023,stream)!=NULL) {
	  if (strncmp(buf,"201 OK",6)==0) {
	      return 0;
	  }
	  if (ostream) 
	      fprintf(ostream,"%s",buf);
	  if (p_table) {
	      int ret;
	      char *data;
              char *c;
              char *c1;

	      c=strchr(buf,'\t');
	      if (c==NULL) 
		  return -1;
	      *c='\0';
	      c=strchr(++c,'\t');
	      if (c==NULL) 
		  return -1;
	      c++;

	      c1=strchr(c,'\n');
	      if (c1!=NULL)
		  *c1='\0';

	      data=strdup(c);
	      if (data==NULL) {
		  /* fprintf(stderr,"error in strdup\n"); */
		  return -1;
	      }
	      /* fprintf(stderr,"ukladam %s->%s",buf,data); */
	      ret=ght_insert(p_table,data,sizeof(char)*strlen(buf),buf);
	      if (ret) {
		  /* fprintf(stderr,"error in ght_insert\n"); */
		  return -1;
	      }
	  }
      }
  }
  return -1;
}

int cache_list(FILE *stream,char *metric,FILE *ostream)
{
  return cache_list_internal(stream,metric,ostream,NULL);
}

/*
 * cache_hash_fill()
 * get value of metric name for all hosts
 * connection must be setup with cache_connect_{local,net}
 */
int cache_hash_fill(FILE *stream,char *metric, void *p_table)
{
  if (p_table==NULL)
      return -1;

  return cache_list_internal(stream,metric,NULL,p_table);
}

/*
 * pbs_cache_get_local()
 * get value of metric name for host hostname
 * connection to local cache is included in this function
 */
int cache_hash_fill_local(char *metric, void *p_table)
{ FILE *stream;
  struct sigaction sa, osa;
  int ret = -1;

  if (sigaction(SIGPIPE, NULL, &osa) == -1)
            return ret; 
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler =SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE,&sa,NULL);

  stream=cache_connect_local();
  if (stream) {
      ret=cache_hash_fill(stream,metric,p_table);
      fclose(stream);
  }
  sigaction(SIGPIPE,&osa,NULL);
  return ret;
}

#endif /* PBS_CACHE_API_STORE_ONLY */

