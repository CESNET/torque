

#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"
#include "update.h"

int update_from_init()
{
  return 0;
}

int update_to_init() {

  return 0;
}


/* 
 * update_from_cache()
 * read this metric from remote cache 
 * calls "list metric" command and output is used by memory_load_s()
 */
static void update_from_cache(struct metric *m,char *server,int port,char *remotename)
{ char buf[1024];
  FILE *stream;
  int ret;

  stream=cache_connect_net(server,port);
  if (stream==NULL) {
      my_log(LOG_ERR,"cannot connect to server %s in update_from_cache",
	    server);
      return;
  }

  if ((ret=cache_remote_metric(stream,(remotename!=NULL)?remotename:m->name,M_ANY))!=0) {
      my_log(LOG_ERR,"remote cache %s doesn't support metric %s,ret=%d",
	     server,(remotename!=NULL)?remotename:m->name,ret);
      cache_close(stream);
      return;
  }

  fprintf(stream,"list %s\n",(remotename!=NULL)?remotename:m->name);
  fflush(stream);
  fgets(buf,1023,stream);
  if (strncmp(buf,"205 list ok",11)==0) {
      ret=memory_load_s(m,stream,1);
      if (conf.debug)
	  my_log(LOG_DEBUG,"imported metric %s from %s, %d hosts",
		 m->name,server,ret);
      m->should_dump+=ret;
  }
  cache_close(stream);
  return;
}

/* 
 * update_remote_cache()
 * send this metric to remote cache 
 */
static void update_remote_cache(struct metric *m,char *server,int port,char *remotename)
{ FILE *stream;
  int ret;

  stream=cache_connect_net(server,port);
  if (stream==NULL) {
      my_log(LOG_ERR,"cannot connect to server %s in update_to_cache",
	    server);
      return;
  }
  if ((ret=cache_remote_metric(stream,(remotename!=NULL)?remotename:m->name, M_REMOTE))!=0) {
      my_log(LOG_ERR,"remote cache %s doesn't support metric %s,ret=%d",
	     server,(remotename!=NULL)?remotename:m->name,ret);
      cache_close(stream);
      return;
  }

  ret=memory_dump_s(m,stream,1,1,(remotename!=NULL)?remotename:m->name);
  if (conf.debug)
      my_log(LOG_DEBUG,"update metric %s to %s, %d hosts",
	     m->name,server,ret);

  cache_close(stream);
  return;
}

/* 
 * update_from_proces()
 * main thread for importer from other cache servers
 * for all M_CACHE metrics, contact remote cache server, read metric
 * values and store them to memory 
 */
void *update_from_proces(void *arg)
{
  struct metric_source *s=(struct metric_source *)arg;
  struct metric *m=s->parent;

  for(;;) {
      if (conf.debug)
	   my_log(LOG_DEBUG,"cache update from %s,metric %s",
		  s->server,m->name);
      update_from_cache(m,s->server,s->port,s->remotename);
      sleep(s->freq);
  }

  /* not reached */
  my_log(LOG_ERR,"cache proces died unexpectedly");
  return NULL;
}

/* 
 * update_to_proces()
 * main thread for exporter to other cache servers
 * for all M_UPDATE metrics, contact remote cache server, 
 * and send our tuples(hostname,value,timestamp) to remote server
 */
void *update_to_proces(void *arg)
{
  struct metric_source *s=(struct metric_source *)arg;
  struct metric *m=s->parent;

  for(;;) {
      if (conf.debug)
	    my_log(LOG_DEBUG,"remote cache update to %s, metric %s, remote metrics %s",
		   s->server,m->name,s->remotename);
      update_remote_cache(m,s->server,s->port,s->remotename);
      sleep(s->freq);
  }

  /* not reached */
  my_log(LOG_ERR,"update proces died unexpectedly");
  return NULL;
}

/*
 TODO LATER partial_update type of provider - send only changes instead of the whole dump
   new source M_PARTIAL_UPDATE, with lastupdate=timestamp of last update
   sends only values with larger timestamp
   - problems with ganglia timestamps
   - memory_update could notify such proceses
   - could be used also for notifications
 */
