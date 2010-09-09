#include "site_pbs_cache_common.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>



/*
 *
 *  store_cluster_attr - store attribute 'name' with 'value' into the "cluster" metric string
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
 *  retrieve_cluster_attr - read the 'value' of attribute 'name' from the "cluster" metric string
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

#define PBS_GROUP "/etc/group.pbsclust"

static config_line_s *pbs_group_lines[10240]; /* FIX ME - dynamicaly allocated? */
static time_t pbs_group_last_modification=0;
static int pbs_group_last_line=-1;

int is_users_in_group(char *group, char *user)
  {
  int ret;
  int i;
  int name_len = strlen(user);

  ret=check_and_read_config(PBS_GROUP,pbs_group_lines,&pbs_group_last_modification,&pbs_group_last_line);
  if (ret==-1) return 0;

  for (i = 0; pbs_group_lines[i]; i++)
    {
    if (strcmp(pbs_group_lines[i]->key,group) == 0)
      {
      char *s = strstr(pbs_group_lines[i]->first,user);

      if (s != NULL) /* found something, now verify if its a full name */
        {

        if (s != pbs_group_lines[i]->first && s[-1] != ' ' && s[-1] != ',' && s[-1] != '\t')
          continue; /* we are not at the beginning of a username */

        if (s[name_len] != '\0' && s[name_len] != '\t' && s[name_len] != '\n' && s[name_len] != ' ' && s[name_len] != ',')
          continue; /* we somewhere inside another username */

        return 1; /* ok, verified */
        }
      }
    }

  return 0;
  }
