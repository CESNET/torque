#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>

#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>

#include "data_types.h"
extern "C" {
#include "site_pbs_cache.h"
}

/* New magrathea decode */
int magrathea_decode_new(resource *res, MagratheaState *state, bool &is_rebootable)
  {
  char *s, *c;

  if (state != NULL)
    *state = MagratheaStateNone;

  if (res == NULL || res->str_avail == NULL)
    return -1;

  if (strstr(res->str_avail,"dynamic_reboot=1") != NULL)
    is_rebootable = true;
  else
    is_rebootable = false;

  if ((s = strdup(res->str_avail)) == NULL)
    return -1;
  if ((c = strpbrk(s,":;")) != NULL)
    *c = '\0';

  if (state == NULL)
    goto done;

  if (strcmp(s,"removed") == 0)
    *state = MagratheaStateRemoved;
  else if (strcmp(s,"down") == 0)
    *state = MagratheaStateDown;
  else if (strcmp(s,"down-bootable") == 0)
    *state = MagratheaStateDownBootable;
  else if (strcmp(s,"booting") == 0)
    *state = MagratheaStateBooting;
  else if (strcmp(s,"free") == 0)
    *state = MagratheaStateFree;
  else if (strcmp(s,"free-bootable") == 0)
    *state = MagratheaStateFreeBootable;
  else if (strcmp(s,"occupied-would-preempt") == 0)
    *state = MagratheaStateOccupiedWouldPreempt;
  else if (strcmp(s,"occupied") == 0)
    *state = MagratheaStateOccupied;
  else if (strcmp(s,"running-preemptible") == 0)
    *state = MagratheaStateRunningPreemptible;
  else if (strcmp(s,"running-priority") == 0)
    *state = MagratheaStateRunningPriority;
  else if (strcmp(s,"running") == 0)
    *state = MagratheaStateRunning;
  else if (strcmp(s,"running-cluster") == 0)
    *state = MagratheaStateRunningCluster;
  else if (strcmp(s,"preempted") == 0)
    *state = MagratheaStatePreempted;
  else if (strcmp(s,"frozen") == 0)
    *state = MagratheaStateFrozen;
  else if (strcmp(s,"down-disappeared") == 0)
    *state = MagratheaStateDownDisappeared;
  else if (strcmp(s,"shutting-down") == 0)
    *state = MagratheaStateShuttingDown;
  else
    {
    free(s);
    return -1; /* unknown state */
    }

done:
  free(s);
  return 0;
  }

struct repository_alternatives **dup_bootable_alternatives(struct repository_alternatives **old)
{ struct repository_alternatives **r;
  int num;

  if (old==NULL) return NULL;

  for(num=0;old[num]!=NULL;num++){}
  if ((r = (struct repository_alternatives **)
             malloc( sizeof(struct repository_alternatives*)
		                   * (num + 1) ) ) == NULL ) {
            log_err(errno, (char*)"get_bootable_alternatives", (char*)"Error allocating memory");
            return NULL;
  }
  for(num=0;old[num]!=NULL;num++){
      r[num]=(repository_alternatives*) malloc(sizeof(struct repository_alternatives));
      if (r[num]==NULL) { 
	  log_err(errno, (char*)"get_bootable_alternatives", (char*)"Error allocating memory");
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

  if (list == NULL)
    return;

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

/* TODO rewrite using char** - change repository alternatives */
int alternative_has_property(struct  repository_alternatives *r, char *prop)
{ char *c;
  char *cc;
  char *list;

  if (r->r_proplist==NULL)
      return 0;

  list = r->r_proplist;
  while ((c = strstr(list,prop)))
    {
    /* it can only begin on the start of the string, or after another property */
    if (c == list || c[-1] == ',')
      {
      cc = c+strlen(prop);
      if (cc[0] == '\0' || cc[0] == ',')
        return 1;
      }
    list = c+1; /* move one char past the occurence */
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


