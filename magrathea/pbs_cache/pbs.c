
#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"
#include "pbs.h"
#include "../net.h"

#include <pbs_ifl.h>
#include <rm.h>

static pthread_mutex_t pbs_mutex;

/* find node status in pbs node structure */
char *get_nstate(struct batch_status *pbs)
{
        struct attrl *pat;

        for (pat = pbs->attribs; pat; pat = pat->next) {
                if (strcmp(pat->name, ATTR_NODE_state) == 0)
                        return pat->value;
        }
        return "";
}

/* 
 * pbs_nodes()
 * connect to pbs server, return list of nodes 
 */
static struct batch_status *pbs_nodes(char *server)
{ struct batch_status *bstatus = NULL;
  int     con;

  con= pbs_connect(server);
  if (con<0) {
      my_log(LOG_ERR,"connect to pbs server failed: %s",strerror(errno));
      return NULL;
  }
  bstatus = pbs_statnode(con, "", NULL, NULL);
  if (bstatus==NULL) {
      my_log(LOG_ERR,"statnode to pbs server failed: %s",strerror(errno));
  }

  pbs_disconnect(con);
  return bstatus;
}

/* 
 * try_connect()
 * try connect to pbs mom, return 0 if pbs_mom is running
 */
int try_connect(char *hostname,int port)
{
  net_addr_t *ips;
  int count;
  int i;
  int p = 0;
  int s;
  net_sockaddr_t a1;
  int ret = -1;
  struct timeval tv = { 1 , 0 };
  fd_set wdfs;


  s=net_socket(net_inaddr_any, &p, 0, 0);
  if (s == -1) {
      return 1;
  }

  ips = net_addr_resolve(hostname, &count);
  if (ips==NULL || count<=0) {
      if (conf.debug)
          my_log(LOG_DEBUG,"pbs host %s doesn't resolve",hostname);
      close(s);
      return 1;
  }

  // try all addresses returned by resolver
  for (i = 0; i < count; i++) {
      net_sockaddr_set_ip(&a1, ips[i]);
      net_sockaddr_set_port(&a1, port);
      ret=fcntl(s, F_SETFL,O_NONBLOCK);
      if (ret==-1) {
          if (conf.debug)
	      my_log(LOG_DEBUG,"cannot set non blocking I/O with %s",net_addr_str(ips[i]));
          continue;
      }
      ret=connect(s,(struct sockaddr *)&a1,sizeof(a1));
      if ((ret==-1) && (errno==EINPROGRESS)) {

          int err = 0;
          socklen_t sz = sizeof(err);

          //fprintf(stderr,"select test %s\n",hostname);
          FD_ZERO(&wdfs);
          FD_SET(s,&wdfs);
          if ((ret = select(s+1,NULL,&wdfs, NULL,&tv))== -1) {
	      if (conf.debug)
	          my_log(LOG_DEBUG,"select failed :%s",strerror(errno));
	      continue;
          }
          if (ret==0) {
	      //if (conf.debug)
	      //    my_log(LOG_DEBUG,"select timeout");
	      continue;
          }

          getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &sz);
          if (err!=0) {
	      //if (conf.debug)
	      //    my_log(LOG_DEBUG,"connect timeout for pbs host %s :%s",
	      //	     hostname,strerror(errno));
	      continue;
          }
      }
  }
     
  if (ret==-1) {
      if (conf.debug)
	  my_log(LOG_DEBUG,"cannot do test connect to pbs host %s",hostname);
      close(s);
      return 1;
  }
  close(s);
  return 0;
}


/* 
 * check_nodes()
 * for given list of nodes (output of pbs_nodes()), query pbs_mom
 * for metric value and store update to memory
 */
static int check_nodes(struct metric_source *s, struct metric *m,struct batch_status *bstatus) 
{ int con;
  struct batch_status *pbstat;
  char *req;
  char *c;
  int num=0;
  int should_be_down;


  for (pbstat = bstatus; pbstat; pbstat = pbstat->next) {

      /* 
       TODO LATER when querying pbs_mom, use shorter RPP timeout instead of try_connect
       */

      should_be_down=0;

      if (try_connect(pbstat->name,s->port)!=0) {
	  if (conf.debug>1)
	      my_log(LOG_DEBUG,"pbs host %s is down (connect)",pbstat->name);
	  should_be_down=1;
	  continue;
      }

      /*
      if (strstr(get_nstate(pbstat), ND_down)!=NULL){
	  if (conf.debug)
	      my_log(LOG_DEBUG,"pbs host %s is down",pbstat->name);
	  continue;
      }

      if ((conf.debug) && (should_be_down==1)) {
	  my_log(LOG_DEBUG,"pbs host %s is down, but server is not reporting it",
		 pbstat->name);
      }
      */

      MUTEXLOCK(pbs_mutex,"pbs mutex");
      con=openrm(pbstat->name,s->port);
      if (con<0) {
	  if (conf.debug)
	      my_log(LOG_DEBUG,"connect to pbs host %s failed",pbstat->name);
          MUTEXUNLOCK(pbs_mutex,"pbs mutex");
	  continue;
      }

      if (s->remotename!=NULL)
	  addreq(con,s->remotename);
      else 
	  addreq(con,m->name);
      req = getreq(con);
      closerm(con);
      MUTEXUNLOCK(pbs_mutex,"pbs mutex");
      if (req==NULL) {
	  if (conf.debug)
	      my_log(LOG_DEBUG,"getreq to pbs host %s failed",pbstat->name);
	  goto out;
      }

      if (strstr(req,"=?")!=NULL)  {
	  if (conf.debug)
	      my_log(LOG_DEBUG,"unknown metric %s on pbs host %s",
		     (s->remotename!=NULL)?s->remotename:m->name,pbstat->name);
	  goto out;
      }

      c=strchr(req,'=');
      if (c==NULL) {
	  if (conf.debug)
	      my_log(LOG_DEBUG,"unknown response from pbs host %s",pbstat->name);
	  goto out;
      }

      //printf("%s %s %s\n",pbstat->name,m->name,c+1);
      memory_update_s(m,pbstat->name,c+1,time(NULL));
      num++;
out:
      if (req) 
	  free(req);
  }

  return num;
}

int pbs_init()
{ int ret;

  if ((ret=pthread_mutex_init(&pbs_mutex,NULL))!= 0) { 
      my_log(LOG_ERR,"pthread_mutex_init failed: %s", strerror(ret)); 
      exit(EX_OSERR); 
  }

  return 0;
}

/* 
 TODO LATER two threads for pbs source, first will handle hosts with fast response, second the rest
 pbs proces 
  zamkne novyzamek;seznam=NULL;odemkne;
  pusti novy proces;
  pri prochazeni hostu najde pomaly stroj, pod zamkem ho prida do seznamu, da cond_signal;
  na konci prida do seznamu"KONEC"
  join_thread;
  uklidi seznam
  - novy proces
    while true {
      pod zamkem
        while (seznam je prazdny) {
	  cond_wait;
	} 
      mame_novy_stroj;
      odemknout;
      if KONEC thread_exit();
      zpracuje stroj;
    }      
      
 */

/*
 * pbs_proces()
 * PBS importer thread
 * for all PBS metrics, connect to pbs server to get list of nodes
 * and then connect to pbs_moms to get actual value of metric 
 */
void *pbs_proces(void *arg)
{
  struct metric_source *s=(struct metric_source *)arg;
  struct metric *m=s->parent;
  struct batch_status *bstatus;
  int ret;

  for(;;) {
      ret=0;
      if (conf.debug)
	  my_log(LOG_DEBUG,"pbs cache update, server %s, metric %s",
		  s->server,m->name);
      MUTEXLOCK(pbs_mutex,"pbs mutex");
      bstatus=pbs_nodes(s->server);
      MUTEXUNLOCK(pbs_mutex,"pbs mutex");
      if (bstatus!=NULL) {
	    ret=check_nodes(s,m,bstatus);
            pbs_statfree(bstatus);
      }
      if (conf.debug)
	  my_log(LOG_DEBUG,"pbs cache update done, server %s, metric %s, num=%d",
		  s->server,m->name,ret);
      sleep(s->freq);
  }

  /* not reached */
  my_log(LOG_ERR,"pbs proces died unexpectedly");
  return NULL;
}
