#include "cloud.h"
#include "resource.h"
#include "assertions.h"
#include "nodespec.h"

#include "api.h"

#include <string.h>

/* test resource if it is a cloud create request */
int is_cloud_create(resource *res)
{
  dbg_precondition(res != NULL, "This function does not accept null.");
  if (strcmp(res->rs_defin->rs_name,"cluster") == 0 &&
      strcmp(res->rs_value.at_val.at_str,"create") == 0)
    return 1;

  return 0;
}

/* determine whether job is cloud */
int is_cloud_job(job *pjob)
  {
  resource *pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
                   find_resc_def(svr_resc_def, "cluster", svr_resc_size));

  if(pres != NULL && (pres->rs_value.at_flags & ATR_VFLAG_SET) &&
     is_cloud_create(pres))
    return(1);

  return(0);
}

/* switch virtual nodes in the nodespec to their cloud masters
 * only used for cloud jobs support
 */
char *switch_nodespec_to_cloud(job  *pjob, char *nodespec)
  {
  /* The nodespec is expected in the following format:
   * node1:res1=val1:res2=val2+node2:res3=val3...
   */
  pars_spec * ps;
  pars_spec_node *iter;

  if ((ps = parse_nodespec(nodespec)) == NULL)
    return NULL;

  iter = ps->nodes;

  while (iter != NULL)
    {
    char *ret; /* XXX ported from original PATCH, needs checking */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    ret=pbs_cache_get_local(iter->properties->name,"host");
    if (ret!=NULL)
      {
      char *c;
      free(iter->properties->name);
      c=strchr(ret,'\t');
      iter->properties->name=strdup(++c);
      free(ret);
      c=strchr(iter->properties->name,'\n');
      if (c)
        *c = '\0';
      }
    iter = iter->next;
    }

  sprintf(log_buffer,"from: %s to: %s",nodespec,concat_nodespec(ps));
  log_record(PBSEVENT_DEBUG2,PBS_EVENTCLASS_SERVER,"Nodespec translated: ",log_buffer);
  return concat_nodespec(ps); /* TODO needs fortification */
  }
