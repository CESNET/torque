
#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"
#include "ganglia.h"
#include "../net.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define BUFS 1024*8

/* 
 * ganglia_connect()
 * connect to ganglia server, used by ganglia_read()
 */
static int ganglia_connect(char *server,int port)
{
  int s;

  s=net_connect_name(server, port, 0);
  if (s == -1) {
      my_log(LOG_ERR,"ganglia_connect failed: %s",strerror(errno));
      return -1;
  }

  return s;
}

/*
 * ganglia_read()
 * read ganglia status from ganglia server
 * returns XML response and length
 */
static char *ganglia_read(int *len,char *server,int port)
{
  char *buf;
  int l;
  int s;
  int ret;


  s=ganglia_connect(server,port);
  if (s<0) 
      return NULL;
  buf=(char *)malloc(BUFS);
  if (buf==NULL) {
      my_log(LOG_ERR,"malloc failed in ganglia_read: %s",strerror(errno));
      close(s);
      exit(EX_OSERR);
  }
  l=0;
  /* 
   TODO LATER timeout when reading ganglia response too long 
   */
  while ((ret=read(s,buf+l,BUFS))>0) {
      l+=ret;
      buf=(char *)realloc(buf,l+BUFS);
      if (buf==NULL) {
         my_log(LOG_ERR,"malloc failed in ganglia_read: %s",strerror(errno));
	 close(s);
         exit(EX_OSERR);
      }
  }
  if (ret==-1) {
      my_log(LOG_ERR,"read failed in ganglia_read: %s",strerror(errno));
      close(s);
      free(buf);
      return NULL;
  }
  buf[l]='\0';
  *len=l;
  close(s);
  return buf;
}

/*
 * find_value()
 * find metric value in ganglia XML
 */
static char *find_value(struct _xmlAttr * a_node,char *name)
{ struct _xmlAttr *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
      if (strcmp((char *)cur_node->name,name)==0) 
	  return (char *)cur_node->children->content;
  }
  return NULL;
}

/*
 * check_element()
 * tranverse on ganglia XML response, find all hosts, for 
 * each host find metric value and store it to memory 
 */
static void check_element(xmlNode * a_node, struct metric *m,struct metric_source *s)
{
  xmlNode *cur_node = NULL;
  char *remotename;

  if (s->remotename!=NULL)
      remotename=s->remotename;
  else
      remotename=m->name;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
      if (cur_node->type == XML_ELEMENT_NODE) {
	  if ((strncmp((char *)cur_node->name,"METRIC",6)==0) &&
	      (strcmp((char *)cur_node->properties->children->content,remotename)==0)) {
	      //char *name=find_value(cur_node->properties,"NAME");
	      char *value=find_value(cur_node->properties,"VAL");
	      char *hostname=find_value(cur_node->parent->properties,"NAME");
	      char *timestamp=find_value(cur_node->parent->properties,"REPORTED");
	     
	      //fprintf(stderr,"%s %s %s\n",hostname,value,timestamp); 
	      memory_update_s(m,hostname,value,atol(timestamp)); 
	      }
      }
      check_element(cur_node->children,m,s);
  }

  return;
}

int ganglia_init()
{
  xmlInitParser();

  return 0;
}

/*
 * ganglia_proces()
 * ganlia importer thread
 * for all metrics, read new values from ganglia server and store
 * them to memory
 */
void *ganglia_proces(void *arg)
{
  struct metric_source *s=(struct metric_source *)arg;
  struct metric *m=s->parent;
  int len;
  char *buf=NULL;
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;

  for(;;) {
      if (conf.debug)
	  my_log(LOG_DEBUG,"ganglia cache update, server %s, metric %s",
		 s->server,m->name);
      buf=ganglia_read(&len,s->server,s->port);
      if (buf==NULL) {
	  my_log(LOG_ERR,"ganglia read failed for server %s",s->server);
      } else {
	  //xmlParserCtxtPtr *ctx;
	  //ctx=xmlNewParserCtxt();

	  doc=xmlParseMemory(buf,len);
	  free(buf);
	  root_element = xmlDocGetRootElement(doc);
	  check_element(root_element,m,s);
	  xmlFreeDoc(doc);
	  //xmlCleanupMemory();
	  //xmlFreeParserCtxt();
      }
      sleep(s->freq);
  }

  xmlCleanupParser();

  /* not reached */
  my_log(LOG_ERR,"ganglia handle proces died unexpectedly");
  return NULL;
}

