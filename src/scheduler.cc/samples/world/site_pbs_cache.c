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
int  alternative_has_property(struct  repository_alternatives *r, char *prop)
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
      if (cc[0] == '\0' || c[0] == ',')
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


