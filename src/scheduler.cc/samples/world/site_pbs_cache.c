#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>

#include "data_types.h"
#include "site_pbs_cache.h"
#include "log.h"

/* 
 * magrathea_decode - decode magrathea status string, return
 * magrathea status 
 * magrathea counter
 */
int magrathea_decode(resource *res,long int *status,long int *count,long int *used,long int *m_free,long int *m_possible)
{ char *s;
  char *c;
  char *ss;
  char delim[2];
  long int val;



  if (status !=NULL) 
      *status=0;
  if (count != NULL) 
      *count=0;
  if (m_possible != NULL) 
      *m_possible=0;

  if ((res==NULL) || (res -> str_avail == NULL)) {
      return -1;
  }

  /* old status (one number) - only nodes with status >=1* are usable */
  if (!res -> is_string) {
       if (status != NULL) 
	   *status=res -> avail;
       return 0;
  }
  /* new status - status;count:free_num:used_num;changed=date */
  if (res -> is_string) {
      delim[0]=';'; 
      delim[1]='\0';
      s=strdup(res -> str_avail);
      ss=s;
      c=strsep(&s,delim); 
      if (c==NULL) {
	  free(ss);
	  return -1;
      }
      if (status != NULL) {
	  if (strncmp(c,"free",strlen("free"))==0)
	      *status=2;
	  else if (strncmp(c,"occupied-would-preempt",strlen("occupied-would-preempt"))==0)
	      *status=1;
	  else if (strncmp(c,"running-preemptible",strlen("running-preemptible"))==0)
	      *status=3;
	  else if (strncmp(c,"running-priority",strlen("running-priority"))==0)
	      *status=1;
	  else if (strncmp(c,"running",strlen("running"))==0)
	      *status=3;

	  /* "down" "booting" "occupied-booting" "occupied" "preempted" "frozen" returns status=0 */
      }
      if (m_possible != NULL) { 
	  if (strncmp(c,"down-possible",strlen("down-possible"))==0) 
	      *m_possible=1;
	  if (strncmp(c,"down-bootable",strlen("down-bootable"))==0) 
	      *m_possible=1;
      }
      if ((m_free!=NULL) && (used!=NULL)) {
	  char *c1;

	  c1=strchr(c,':');
	  if (c1!=NULL) {
	      c1++;
	      errno=0;
	      val=strtol(c1,NULL,10);
	      if (errno==0)
		  *used=val;

	      c1=strchr(c1,':');
	      if (c1!=NULL) {
		  c1++;
		  errno=0;
		  val=strtol(c1,NULL,10);
		  if (errno==0)
		      *m_free=val;
	      }
	  }
      }
      if (s!=NULL) {
         c=strsep(&s,delim);
	 if (count != NULL) {
	     errno=0;
	     val=strtol(c,NULL,10);
	     if (errno==0) 
		 *count=val;
	 }
      }
      free(ss);
      return 0;
  }
  return -1;
}

#define PBS_REPLICA_HOSTS   "/var/local/repository/hosts"
#define PBS_REPLICA_IMAGES   "/var/local/repository/images"

/* format of PBS_REPLICA_LIST file could be
 * hostname=nam1
 * hostname=nam2
 * ...
 * proplist=nam1
 * ...
 * ?? or different file ??
 * nam1=prop1,prop2
 * ...
 */
static config_line_s *replica_hosts_lines[10240]; /* FIX ME - dynamicaly allocated? */
static time_t replica_hosts_last_modification=0;
static int replica_hosts_last_line=-1;

static config_line_s *replica_images_lines[10240]; /* FIX ME - dynamicaly allocated? */
static time_t replica_images_last_modification=0;
static int replica_images_last_line=-1;

struct repository_alternatives **get_bootable_alternatives(char *hostname,char *properties)
{ struct repository_alternatives **r;
  int num;
  int ret;
  int lineno;

  ret=check_and_read_config(PBS_REPLICA_HOSTS,replica_hosts_lines,&replica_hosts_last_modification,&replica_hosts_last_line);
  if (ret==-1) return NULL;
  ret=check_and_read_config(PBS_REPLICA_IMAGES,replica_images_lines,&replica_images_last_modification,&replica_images_last_line);
  if (ret==-1) return NULL;

  lineno=0;num=0;
  while(replica_hosts_lines[lineno]) {
      if (strcmp(replica_hosts_lines[lineno]->key,hostname)==0)
	  num++;
      lineno++;
  }

  if ((r = (struct repository_alternatives **) 
      malloc( sizeof(struct repository_alternatives*)  
	      * (num + 1) ) ) == NULL ) { 
     log_err(errno, "get_bootable_alternatives", "Error allocating memory");
     return NULL;
  }

  lineno=0;num=0;
  while(replica_hosts_lines[lineno]) {
      if (strcmp(replica_hosts_lines[lineno]->key,hostname)==0) {
	  r[num]=malloc(sizeof(struct repository_alternatives));
	  if (r[num]==NULL) {
	      log_err(errno, "get_bootable_alternatives", "Error allocating memory");
	      return NULL; 
	  }
	  r[num]->r_name=strdup(replica_hosts_lines[lineno]->first);
	  r[num]->r_proplist=NULL;
	  r[num]->r_mark=1;
	  num++;
      }
      lineno++;
  }
  r[num]=NULL;

  num=0;
  while (r[num]) {
      int i=0;
      while (replica_images_lines[i]) { 
	  if (strcmp(replica_images_lines[i]->key,r[num]->r_name)==0) { 
	      r[num]->r_proplist=strdup(replica_images_lines[i]->first);
	  }
	  i++;
      }
      num++;
  }

  return r;
}

char *get_alternative_properties(char *name)
{ int ret;
  int i;

  ret=check_and_read_config(PBS_REPLICA_IMAGES,replica_images_lines,&replica_images_last_modification,&replica_images_last_line);
  if (ret==-1) return NULL;

  for(i=0;replica_images_lines[i];i++)
      if (strcmp(replica_images_lines[i]->key,name)==0) 
	  return(replica_images_lines[i]->first);
  return NULL;
}

struct repository_alternatives **dup_bootable_alternatives(struct repository_alternatives **old)
{ struct repository_alternatives **r;
  int num;

  if (old==NULL) return NULL;

  for(num=0;old[num]!=NULL;num++){}
  if ((r = (struct repository_alternatives **)
             malloc( sizeof(struct repository_alternatives*)
		                   * (num + 1) ) ) == NULL ) {
            log_err(errno, "get_bootable_alternatives", "Error allocating memory");
            return NULL;
  }
  for(num=0;old[num]!=NULL;num++){
      r[num]=malloc(sizeof(struct repository_alternatives));
      if (r[num]==NULL) { 
	  log_err(errno, "get_bootable_alternatives", "Error allocating memory"); 
	  return NULL; 
      }
      r[num]->r_name=strdup(old[num]->r_name);
      if (old[num]->r_proplist)
	  r[num]->r_proplist=strdup(old[num]->r_proplist);
      else
	  r[num]->r_proplist=NULL;
      r[num]->r_mark=old[num]->r_mark;
  }
  r[num]=NULL;

  return r;
}

void free_bootable_alternatives(struct  repository_alternatives **list) 
{ int i;

  for(i=0;list[i]!=NULL;i++) {
    if (list[i]->r_name)
	free(list[i]->r_name);
    if (list[i]->r_proplist)
	free(list[i]->r_proplist);
    free(list[i]);
  }
  free(list);
  return;
}  

int  alternative_has_property(struct  repository_alternatives *r, char *prop)
{ char *c;
  char *cc;

  if (r->r_proplist==NULL)
      return 0;
  /* TODO rewrite using strtok */
  c=strstr(r->r_proplist,prop);
  if (c) {
      if ((c==r->r_proplist) || (*(c-1)==',')) {
	  cc=c+strlen(prop);
	  if ((*cc == '\0') || (*cc==',')) {
	      return 1;
	  }
      }
  }
  return 0;
}

struct repository_alternatives *find_alternative_with_property(struct  repository_alternatives **list,char *prop)
{ int i;

  for(i=0;list[i]!=NULL;i++) {
      if (alternative_has_property(list[i],prop))
	  return list[i]; 
  }
  return NULL;
}

/*
 *
 *	store_cluster_attr - store attribute 'name' with 'value' into the "cluster" metric string
 *
 */

void store_cluster_attr(char **metric, char *name, char *value) {
	char *pos = NULL, *rest;
	char *pattern;
	char *new_metric = NULL;
	int len;

	pattern = malloc(strlen(name)+2);
	sprintf(pattern, "%s=",name);

	if (*metric) pos = strstr(*metric, pattern);

	if(pos) { /*attribute already set. Needs replacing */
		rest = strchr(pos,';');
		pos[0] = 0;
		/* Put together the front and rear part of the metric */
		if (rest == NULL)
		  rest = "";
		new_metric = malloc(strlen(*metric)+strlen(rest)+1);
		sprintf(new_metric,"%s%s", *metric, rest);
		free (*metric);
	}
	else (new_metric = *metric);

	len = new_metric?(strlen(new_metric)+strlen(";")):0;
	len += strlen(name) + strlen("=") + strlen(value);

	*metric = malloc(len+1);
	sprintf(*metric,"%s%s%s=%s", new_metric ? new_metric : "", new_metric ? ";" : "", name, value);

	free (new_metric);
	free (pattern);
}

/*
 *
 *	retrieve_cluster_attr - read the 'value' of attribute 'name' from the "cluster" metric string
 *
 */

void retrieve_cluster_attr(char *metric, char *name, char **value) {
	char *pos = NULL;
	char *pattern;
	size_t length;

	pattern = malloc(strlen(name)+2);
	sprintf(pattern,"%s=",name);

	if (metric) pos = strstr(metric, pattern);

	if(pos) { /* attribute found */
		length = strcspn(pos, ";:\n"); /* A colon should not appear among the attributes anyway. */
		*value = (char *)calloc(1, length - strlen(name));
		strncpy(*value, pos + strlen(name) + 1, length - strlen(name) - 1);
	}
	else (*value=NULL);

	free (pattern);
}

