#include "cloud.h"
#include "resource.h"
#include "assertions.h"
#include "nodespec.h"
#include "api.h"
#include "pbs_job.h"
#include "pbs_nodes.h"
#include "site_pbs_cache.h"

#include <string.h>


extern struct pbsnode *find_nodebyname(char *);

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

static char *construct_mapping(char* old, char* new, char* alternative)
  {
  /* cloud=virtual[alternative]; */
  char *result = malloc(strlen(old)+1+strlen(new)+3+strlen(alternative)+1);
  sprintf(result,"%s=%s[%s];",old,new,alternative);
  return result;
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

  char mapping[1024] = { 0 }; /* FIXME META Needs a dynamic array */

  if ((ps = parse_nodespec(nodespec)) == NULL)
    return NULL;

  iter = ps->nodes;

  while (iter != NULL) /* for each node */
    {
    char *ret; /* FIXME META ported from original PATCH, needs checking */

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");

    ret=pbs_cache_get_local(iter->properties->name,"host");
    if (ret!=NULL)
      {
      char *c, *cloud, *mapped;
      pars_prop *it2;

      /* cut out the cloud node name from the cache value */
      c=strchr(ret,'\t');
      cloud = strdup(++c);
      c=strchr(cloud,'\n');
      if (c)
        *c = '\0';
      free(ret);

      it2 = iter->properties;
      while (it2 != NULL)
        {
        if ((strcmp(it2->name,"alternative") == 0) && (it2->value != NULL))
          break;

        it2 = it2->next;
        }

      if (it2 != NULL)
        mapped = construct_mapping(cloud,iter->properties->name,it2->value);
      else
        mapped = construct_mapping(cloud,iter->properties->name,"default");
      strcat(mapping,mapped);
      free(mapped);

      /* interchange virtual node name for its cloud master */
      free(iter->properties->name);
      iter->properties->name=cloud;
      }
    iter = iter->next;
    }

  /* store the constructed mapping into job attribute */
  job_attr_def[(int)JOB_ATR_cloud_mapping].at_decode(&pjob->ji_wattr[(int)JOB_ATR_cloud_mapping],
                                                      (char *)0, (char *)0, mapping);

  sprintf(log_buffer,"Source: %s Target: %s",nodespec,concat_nodespec(ps));
  log_record(PBSEVENT_SCHED,PBS_EVENTCLASS_REQUEST,"switch_nodespec_to_cloud",log_buffer);
  return concat_nodespec(ps); /* FIXME META needs fortification and fix of memory leak */
  }

void set_alternative_on_node(char *nodename, char *alternative)
  {
#if 0
  char * c;
  struct pbsnode *np;

  np = find_nodebyname(nodename);
  if ((np = find_nodebyname(nodename)) == NULL)
    return;

  c = get_alternative_properties(alternative);

  free(np->nd_adprop); /* parse prop list */
  np->nd_adprop = c;

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_NODE, np->nd_name, "additional properties set on node");
#endif
  }

void clear_alternative_on_node(char *nodename)
  {
#if 0
  struct pbsnode *np;

  if ((np = find_nodebyname(nodename)) == NULL)
    return;

  free(np->nd_adprop);
  np->nd_adprop = 0; /* free_prop_list */

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_NODE, np->nd_name, "additional properties cleared from node");
#endif
  }

char *get_alternative_name(char *mapping, char *machine)
  {
  char *mpos, *begin, *end, *result;

  if ((mpos = strstr(mapping,machine)) == NULL)
    return NULL;

  if ((begin = strchr(mpos,'[')) == NULL)
    return NULL;

  begin++;

  if ((end = strchr(mpos,']')) == NULL)
    return NULL;

  end--;

  result = malloc(end-begin+1);
  memset(result,0,end-begin+1);
  strncpy(result,begin,end-begin);

  return result;
  }


void cloud_transition_into_running(job *pjob)
  {
  pars_spec *ps;
  pars_spec_node *iter;

  if (is_cloud_job(pjob) && pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN_CLOUD)
    svr_setjobstate(pjob, JOB_STATE_RUNNING, JOB_SUBSTATE_RUNNING);

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  iter = ps->nodes;

  while (iter != NULL)
    {
    char *c;

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    /* update cache information that this machine now belongs to the following vcluster */
    cache_store_local(iter->properties->name,"machine_cluster",pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str);

    /* dig up the alternative from cloud mapping */
    dbg_consistency((pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_flags & ATR_VFLAG_SET) != 0,
        "Cloud mapping has to be set at this point.");

    c = get_alternative_name(pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_val.at_str,iter->properties->name);
    if (c != NULL)
      {
      sprintf(log_buffer,"Determined alternative is: %s", c);
      log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, iter->properties->name,log_buffer);
      set_alternative_on_node(iter->properties->name,c);
      free(c);
      }

    iter = iter->next;
    }
  free_parsed_nodespec(ps);
  }

void cloud_transition_into_stopped(job *pjob)
  {
  pars_spec *ps;
  pars_spec_node *iter;

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  iter = ps->nodes;

  while (iter != NULL)
    {
    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    /* update cache information that this machine now belongs to the following vcluster */
    cache_remove_local(iter->properties->name,"machine_cluster");

    /* remove any alternative stored on the node */
    clear_alternative_on_node(iter->properties->name);

    iter = iter->next;
    }

  free_parsed_nodespec(ps);
  }
