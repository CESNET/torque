#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"
#include "./distrib/main.h"

/*
 TODO LATER use rbtree instead of list, usefull mainly for M_REMOTE type
 TODO LATER create separate index for hosts
  - insert/delete will maintain it
  - used by info
 TODO LATER support for virtual clusters
  - new metric
  - list command with sorted (by value) output
  - replicas, quorum management
 TODO LATER timestamps with miliseconds
 */

#ifndef TAILQ_FIRST
#define TAILQ_EMPTY(head)               ((head)->tqh_first == NULL)
#define TAILQ_FIRST(head)               ((head)->tqh_first)
#define TAILQ_NEXT(elm, field)          ((elm)->field.tqe_next)
#endif

#ifndef CIRCLEQ_FIRST
#define CIRCLEQ_EMPTY(head)             ((head)->cqh_first == (void *)(head))
#define CIRCLEQ_FIRST(head)             ((head)->cqh_first)
#define CIRCLEQ_LAST(head)              ((head)->cqh_last)
#define CIRCLEQ_NEXT(elm, field)        ((elm)->field.cqe_next)
#define CIRCLEQ_PREV(elm, field)        ((elm)->field.cqe_prev)
#endif

#define TAILQ_NULL(head) NULL
#define CIRCLEQ_NULL(head) (void *)head

/* currently TAILQ is used. To use CIRCLEQ, use ".,$s/TAILQ_/CIRCLEQ_/g" from this line */
TAILQ_HEAD(listhead, mylist);

struct mylist {
       char *hostname;
       char *value;
       time_t timestamp;
       TAILQ_ENTRY(mylist) m_list;
};

struct metric *metrics=NULL;

int memory_init()
{
  metrics=NULL;
  return 0;
}

/*
 * memory_create_source()
 * create memory_source structure, used when creating new metric
 * or adding new source to already existing metric
 */
struct metric_source *memory_create_source(int type,int freq,char *server,int port, char*remotename,struct metric *m)
{
  struct metric_source *s;
  s=(struct metric_source*)malloc(sizeof(struct metric_source));
  if (s==NULL) {
      my_log(LOG_ERR,"malloc in memory_init failed: %s",strerror(errno));
      exit(EX_OSERR);
  }
  s->type=type;
  s->freq=freq;
  s->th=0;
  s->parent=m;
  s->next=NULL;
  if (server) {
      s->server=strdup(server);
      if (s->server==NULL) {
         my_log(LOG_ERR,"malloc in memory_init failed: %s",strerror(errno));
         exit(EX_OSERR);
      }
  } else {
      s->server=strdup("");;
  }
  s->port=port;
  if (remotename) {
      s->remotename=strdup(remotename);
      if (s->remotename==NULL) {
	  my_log(LOG_ERR,"malloc in memory_init failed: %s",strerror(errno));
	  exit(EX_OSERR);
      }
  } else {
      s->remotename=NULL;
  }
  return s;
}

/*
 * memory_create_metric()
 * create new metric, including first source
 */
struct metric *memory_create_metric(char *name,int type,char *server,int freq,int port,char *remotename)
{ int ret;
  struct metric *m;
  struct metric_source *s;

  m=(struct metric *)malloc(sizeof(struct metric));
  if (m==NULL) {
      my_log(LOG_ERR,"malloc in memory_init failed: %s",strerror(errno));
      exit(EX_OSERR);
  }

  m->head=(struct listhead *)malloc(sizeof(struct listhead));
  if (m->head==NULL) {
      my_log(LOG_ERR,"malloc in memory_init failed: %s",strerror(errno));
      exit(EX_OSERR);
  }

  TAILQ_INIT(m->head);
  if ((ret=pthread_mutex_init(&(m->lock),NULL))!= 0) {
      my_log(LOG_ERR,"pthread_rwlock_init failed: %s", strerror(ret));
      exit(EX_OSERR);
  }

  if ((m->name=strdup(name))==NULL) {
      my_log(LOG_ERR,"strdup in memory_init failed: %s",strerror(errno));
      exit(EX_OSERR);
  }

  m->should_dump=0;
  m->next=NULL;
  m->lastused=NULL;

  s=memory_create_source(type,freq,server,port,remotename,m);
  m->source=s;  
  return m;
}

static int mycmp(char *s1,char *s2)
{
  if ((s1==NULL) && (s2==NULL))
      return 0;
  if ((s1==NULL) || (s2==NULL))
      return 1;
  return strcmp(s1,s2);
}

/*
 * memory_add_metric()
 * add new metric, or add new source to already existing metric
 */
int memory_add_metric(char *name,int type,char *server,int freq,int port,char *remotename)
{
  struct metric *a;
  struct metric *prev;
  struct metric_source *s;
  struct metric_source *p;
  struct metric_source *p_prev;

  if (conf.debug)
      my_log(LOG_DEBUG,"new metric %s, type %d, server %s",
	     name,type,server);
  /*
   TODO LATER there should be lock for metrics too
   */
  if (metrics==NULL) {
      #ifndef DIST_DISABLED
      if (type == M_DIST) paxos_insert_metric(name);
      #endif
      metrics=memory_create_metric(name,type,server,freq,port,remotename);
      return 0;
  }
  for(a=metrics;a;a=a->next) {
      prev=a;
      if (strcmp(a->name,name)==0) {
	  s=memory_create_source(type,freq,server,port,remotename,a);
	  if (a->source)
		  p=a->source;
	  else {
		  a->source=s;
		  return 0;
	  }
	  for(;p;p=p->next) {
	      p_prev=p;
	      if ((strcmp(p->server,server)==0) &&((p->type=type) &&
						   mycmp(p->remotename,remotename)==0)) {
		  if (s->server)
		      free(server);
		  free(s);
		  return -1;
	      }
	  }
	  p_prev->next=s;
	  return 0;
     }
  }
  #ifndef DIST_DISABLED
  if (type == M_DIST) paxos_insert_metric(name);
  #endif
  prev->next=memory_create_metric(name,type,server,freq,port,remotename);
  return 0;
}

/*
 * memory_free()
 * deallocate one memory record
 *
 * no lock in this function, whole function must be called with lock
 * already acquired */
static void memory_free(struct mylist *a)
{
  if (!a) {
      my_log(LOG_ERR,"memory free with NULL");
      exit(EX_SOFTWARE);
  }
  if (a->hostname)
      free(a->hostname);
  if (a->value)
      free(a->value);

  free(a);
}

/*
 * memory_insert()
 * insert new host record to metric list
 *
 * no lock in this function, whole function must be called with lock
 * already acquired
 */
static struct mylist *memory_insert(struct metric *m,char *hostname,char *value,time_t timestamp)
{
  struct mylist *new;

  if ((new=malloc(sizeof(struct mylist))) == NULL) {
      my_log(LOG_ERR,"memory malloc failed: %s",strerror(errno));
      exit(EX_OSERR);
  }

  bzero((void *)new, sizeof(new));
  if ((new->hostname=strdup(hostname))==NULL) {
          my_log(LOG_ERR,"strdup in memory_insert failed: %s",strerror(errno));
          exit(EX_OSERR);
  }
  if ((new->value=strdup(value))==NULL) {
          my_log(LOG_ERR,"strdup in memory_insert failed: %s",strerror(errno));
          exit(EX_OSERR);
  }
  if (timestamp!=0)
      new->timestamp=timestamp;
  else
      new->timestamp=time(NULL);

  TAILQ_INSERT_TAIL(m->head,new,m_list);
  return new;
}

/*
 * memory_find_rec()
 * find host record in given metric
 * internal function, returns pointer into memory!
 *
 * no lock in this function, whole function must be called with lock
 * already acquired
 */
static struct mylist *memory_find_rec(struct metric *m,char *hostname)
{ struct mylist *a;

  if (conf.debug>1)
      my_log(LOG_DEBUG,"memory find start: %s %s",hostname,m->name);

  if (m->lastused) {
      if (strcmp(m->lastused->hostname,hostname)==0) {
	  //if (conf.debug)
	  //    my_log(LOG_DEBUG,"lastused success");
	  return m->lastused;
      }
      a=TAILQ_NEXT(m->lastused,m_list);
      if (a && (strcmp(a->hostname,hostname)==0)) {
	  //if (conf.debug)
	  //    my_log(LOG_DEBUG,"lastused(next) success");
	  return a;
      }
  }

  for (a=TAILQ_FIRST(m->head);a!=TAILQ_NULL(m->head);a=TAILQ_NEXT(a,m_list)) {
      if (conf.debug>3)
	   my_log(LOG_DEBUG,"memory find trace: %s %s",m->name, a->hostname);
      if (strcmp(a->hostname,hostname)==0) {
          break;
      }
  }
  return a;
}

/*
 * memory_find()
 * find metric value and timestamp for given metric name and host
 *   - check all metrics
 * on correct one
 */
char *memory_find(char *hostname,char *name,time_t *timestamp)
{ struct mylist *a;
  struct metric *m;
  char *value=NULL;

  for (m=metrics;m;m=m->next) {
      if (strcmp(m->name,name)==0)
          break;
  }
  if (m==NULL) {
      if (conf.debug)
          my_log(LOG_DEBUG,"memory find unknown metric %s",name);
      return NULL;
  }


  #ifndef DIST_DISABLED
  if(m->source->type == M_DIST) {
	  if(get_master_status()==CLIENT) {
		value = find_replicated(hostname, name, timestamp);
		return value;
	  }
  }
  #endif
  MUTEXLOCK(m->lock,m->name)
  a=memory_find_rec(m,hostname);
  if (a) {
      value=strdup(a->value);
      if (value==NULL) {
	  my_log(LOG_ERR,"strdup failed in memory_find: %s",strerror(errno));
	  exit(EX_OSERR);
      }
      if (timestamp)
	  *timestamp=a->timestamp;
  }
  MUTEXUNLOCK(m->lock,m->name)

  if (conf.debug>1) {
      if (a!=NULL)
	   my_log(LOG_DEBUG,"memory find OK: %s %s",hostname,m->name);
      else
	  my_log(LOG_DEBUG,"memory find failed: %s %s",hostname,m->name);
  }
  return value;
}

/*
 * memory_find_host()
 * find metric value and timestamp for given host, metric structure
 * is already known
 */
char *memory_find_host(struct metric *m,char *hostname,time_t *timestamp)
{ struct mylist *a;
  char *value=NULL;

  MUTEXLOCK(m->lock,m->name)
  #ifndef DIST_DISABLED
  if(m->source->type == M_DIST) {
	  if(get_master_status()==CLIENT) {
		MUTEXUNLOCK(m->lock,m->name)
		return find_replicated(hostname, m->name, timestamp);
	  }
  }
  #endif
  a=memory_find_rec(m,hostname);
  if (a) {
      value=strdup(a->value);
      if (value==NULL) {
	  my_log(LOG_ERR,"strdup failed in memory_find: %s",strerror(errno));
	  exit(EX_OSERR);
      }
      if (timestamp)
	  *timestamp=a->timestamp;
  }
  MUTEXUNLOCK(m->lock,m->name)
  if (conf.debug>1) {
      if (a!=NULL)
	   my_log(LOG_DEBUG,"memory find OK: %s %s",hostname,m->name);
      else
	  my_log(LOG_DEBUG,"memory find failed: %s %s",hostname,m->name);
  }
  return value;
}

/*
 * memory_remove()
 * remove record for hostname in given metric
 */
int memory_remove(char *hostname,char *name, int replic_commited)
{ struct mylist *a;
  struct metric *m;
  int ret=1;

  for (m=metrics;m;m=m->next) {
      if (strcmp(m->name,name)==0)
          break;
  }
  if (m==NULL) {
      if (conf.debug)
          my_log(LOG_DEBUG,"memory find unknown metric %s",name);
      return -1;
  }
  #ifndef DIST_DISABLED
  if(!replic_commited) {
      if(m->source->type == M_DIST) {      
	  if(get_master_status()==MASTER) {
		global_lock();
                MUTEXLOCK(m->lock,m->name)
		ret = remove_replicated(hostname, m->name);
		if (ret!=0) {
  		    MUTEXUNLOCK(m->lock,m->name)
                    global_unlock();
                    return -1;
                }
	  }else {
		proxy_start();
	  	ret = remove_replicated_request(hostname, m->name);
		return ret;
	  }	
     }else {
        MUTEXLOCK(m->lock,m->name)
     }
   }else {
     global_lock();
     MUTEXLOCK(m->lock,m->name)
   }
   
  #endif
  for (a=TAILQ_FIRST(m->head);a!=TAILQ_NULL(m->head);a=TAILQ_NEXT(a,m_list)) {
      if (strcmp(a->hostname,hostname)==0) {
	  if (m->lastused==a)
	      m->lastused=NULL;
          TAILQ_REMOVE(m->head,a,m_list);
          m->should_dump++;
	  ret=0;
          break;
      }
  }
  if (ret==0)
      memory_free(a);
  MUTEXUNLOCK(m->lock,m->name)
  #ifndef DIST_DISABLED
  if (m->source->type == M_DIST) {
      proxy_stop();
      global_unlock();
  }
  #endif
  if (conf.debug>1) {
      if (ret==0)
	  my_log(LOG_DEBUG,"memory remove hostname OK: %s %s",hostname,name);
      else
          my_log(LOG_DEBUG,"memory remove hostname failed: %s %s",hostname,name);
  }
  return ret;
}

/*
 * memory_dump()
 * for metric with name "name", print all (name,value,timestamp) tuples
 * used both for dump to file and metric dump to remote cache!
 */
int memory_dump(char *name, FILE *file, int flush)
{ struct metric *m;

  for (m=metrics;m;m=m->next) {
      if (strcmp(m->name,name)==0)
          break;
  }
  if (m==NULL) {
      if (conf.debug)
          my_log(LOG_DEBUG,"memory dump unknown metric %s",name);
      return -1;
  }

  return memory_dump_s(m,file,flush,0,NULL);
}

/*
 * memory_dump()
 * for given metric, print all (name,value,timestamp) tuples
 * used both for dump to file and metric dump to remote cache!
 * supports two types of dump: standard to file/network client
 * and special on form of "add" commands for update to remote cache
 */
int memory_dump_s(struct metric *m, FILE *file, int flush,int remote,char *remotename)
{ struct mylist *a;
  struct mylist *next;
  int ret=0;
  MUTEXLOCK(m->lock,m->name)
  for (a=TAILQ_FIRST(m->head);a!=TAILQ_NULL(m->head);a=next) {
      next=TAILQ_NEXT(a,m_list);
      //fprintf(stderr,"cyklus %p %s %s\n",next,remotename,a->hostname);
      if (remote) {
	   fprintf(file,"radd\t%s\t%s\t%ld\t%s\n",
		   a->hostname,remotename,a->timestamp,a->value);
      } else {
	  fprintf(file,"%s\t%ld\t%s\n",a->hostname,a->timestamp,a->value);
      }
      if (flush)
	  fflush(file);
      ret++;
  }
  MUTEXUNLOCK(m->lock,m->name)
  if (conf.debug)
      my_log(LOG_DEBUG,"memory dump OK, %s %d lines",m->name,ret);
  return ret;
}

/*
 * memory_load()
 * read file for metric name, insert tuples to memory
 * used by paxos
 */
int memory_load(char * name, FILE *file,int remote)
{ struct metric *m;

  for (m=metrics;m;m=m->next) {
      if (strcmp(m->name,name)==0)
          break;
  }
  if (m==NULL) {
      if (conf.debug)
          my_log(LOG_DEBUG,"memory dump unknown metric %s",name);
      return -1;
  }

  return memory_load_s(m,file,remote);
}

/*
 * memory_load_s()
 * read dump for given metric, insert tupes to memory
 * used both for initial load of dumps and for import
 * from remote cache
 */
int memory_load_s(struct metric *m, FILE *file,int remote)
{
  int ret=0;
  char buf[1024*8];
  char hostname[1024];
  char value[1024];
  time_t timestamp;

  while (fgets(buf,1024*8-1,file)!=NULL) {
    if ((remote) && (strncmp(buf,"201 OK",6)==0))
	break;
        sscanf(buf,"%s\t%ld\t%s",hostname,&timestamp,value);
	if (remote)
		memory_update_s(m,hostname,value,timestamp, 1);
	else {
		MUTEXLOCK(m->lock,m->name)
		memory_insert(m,hostname,value,timestamp);
		MUTEXUNLOCK(m->lock,m->name)
	}
	ret++;
  }

  if (conf.debug)
      my_log(LOG_DEBUG,"memory load OK, %s %d lines",m->name,ret);
  return ret;
}

/*
 * memory_update()
 * update memory for given metric and hostname.
 * if it is new hostname, create new record, otherwise replace old values
 */
int memory_update(char *hostname,char *name,char *value,time_t timestamp, int replic_commited)
{  struct metric *m;

   for (m=metrics;m;m=m->next) {
      if (strcmp(m->name,name)==0)
          break;
   }
   if (m==NULL) {
      if (conf.debug)
          my_log(LOG_DEBUG,"memory update unknown metric %s",name);       
      return -1;
   }

   return memory_update_s(m,hostname,value,timestamp, replic_commited);
}

/*
 * memory_update_s()
 * update memory for given metric and hostname. Metric list is already known.
 * if it is new hostname, create new record, otherwise replace old
 * values
 */
int memory_update_s(struct metric *m,char *hostname,char *value,time_t timestamp, int replic_commited)
{  struct mylist *a;
   int ret=0;
   time_t my_timestamp;

   if (conf.debug>1)
       my_log(LOG_DEBUG,"memory update start: %s %s",hostname,m->name);
   if (timestamp==0) {
       my_timestamp=time(NULL);
   } else {
       my_timestamp=timestamp;
   }
   #ifndef DIST_DISABLED
   if(!replic_commited) {
      if(m->source->type == M_DIST) {          
	  if(get_master_status()==MASTER) {
		global_lock();
	        MUTEXLOCK(m->lock,m->name)
		ret = update_replicated(hostname, m->name, value, my_timestamp);
		if (ret!=0) {
  		    MUTEXUNLOCK(m->lock,m->name)
                    global_unlock();
                    return -1;
                }
	  }else {
		proxy_start();
	  	ret = update_replicated_request(hostname, m->name, value);
                if (ret!=0) {
                }
		return ret;
	  }	
     }else {
        MUTEXLOCK(m->lock,m->name)
     }
   }else {
     global_lock();
     MUTEXLOCK(m->lock,m->name)
   }
   #endif
   a=memory_find_rec(m,hostname);
   if (a !=NULL){
       if (a->timestamp<=my_timestamp) {
         free(a->value);
         if ((a->value=strdup(value))==NULL) {
	       my_log(LOG_ERR,"strdup in memory_update failed: %s",strerror(errno));
	       exit(EX_OSERR);
         }
         a->timestamp=my_timestamp;
         m->should_dump++;
         m->lastused=a;
       }
       MUTEXUNLOCK(m->lock,m->name)
       #ifndef DIST_DISABLED
       if (m->source->type == M_DIST) {
        proxy_stop();       
        global_unlock();
       }
       #endif
       if (conf.debug>1)
	   my_log(LOG_DEBUG,"memory update, real update: %s %s",hostname,m->name);
   } else {
       a=memory_insert(m,hostname,value,timestamp);
       m->should_dump++;
       m->lastused=a;
       MUTEXUNLOCK(m->lock,m->name)
       #ifndef DIST_DISABLED
       if (m->source->type == M_DIST) {
            proxy_stop();
            global_unlock();
       }
       #endif
       if (conf.debug>1)
	   my_log(LOG_DEBUG,"memory update, insert %s %s",hostname,m->name);
   }
   return ret;
}

/*
 * memory_timeout()
 * for all metrics, find all tuples older then conf.timeout
 * and remove them
 * called from dumper thread
 */
int memory_timeout()
{
  int ret=0;
  time_t now;
  struct mylist *a;
  struct mylist *next;
  struct metric *m;

  now=time(NULL);
  if (conf.timeout>0) {
      for (m=metrics;m;m=m->next) {
	  MUTEXLOCK(m->lock,m->name)
          for (a=TAILQ_FIRST(m->head);a!=TAILQ_NULL(m->head);a=next) {
            next=TAILQ_NEXT(a,m_list);
            if (now - a->timestamp > conf.timeout) {
               ret++;
	       m->should_dump++;
	       if (m->lastused==a)
		   m->lastused=NULL;
	       TAILQ_REMOVE(m->head,a,m_list);
	       if (conf.debug>1)
		           my_log(LOG_DEBUG,"memory remove:%s %s",a->hostname,m->name);
	       memory_free(a);
            }
          }
          MUTEXUNLOCK(m->lock,m->name)
          if (conf.debug)
             my_log(LOG_DEBUG,"memory timeout, removed %d for %s",ret,m->name);
      }
  }
  return 0;
}
