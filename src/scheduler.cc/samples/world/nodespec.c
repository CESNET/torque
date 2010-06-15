#include "data_types.h"
#include "assertions.h"
#include "node_info.h"
#include "job_info.h"
#include "misc.h"
#include "globals.h"
#include "nodespec.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assertions.h>
#include "site_pbs_cache.h"
#include "api.h"

/* get the number of requested virtual cpus */
int get_req_vps(pars_spec_node *spec)
  {
  pars_prop *iter = spec->properties;

  while (iter != NULL)
    {
    if ((strcmp(iter->name,"ppn") == 0) && (iter->value != NULL))
      {
      return atoi(iter->value);
      }

    iter = iter->next;
    }

  return 1;
  }

enum ResourceCheckMode { MaxOnly, Avail };

static int node_has_enough_np(node_info *ninfo, char *value, enum ResourceCheckMode mode)
  {
#if 0
  long amount = res_to_num(value);
  switch(mode)
    {
    case MaxOnly:
      if (ninfo->np >= amount)
        return 1;
      break;
    case Avail:
      if (ninfo->np - ninfo->npshared >= amount &&
          ninfo->npfree >= amount)
        return 1;
      else
        return 0;
      break;
    }
  return 0; /* default is no */
#endif
  return 1; /* np support turned of for a while */
  }

static int node_has_enough_resource(node_info *ninfo, char *name, char *value,
    enum ResourceCheckMode mode)
  {
  struct resource *res;
  int i;
  long amount = 0;

  for (i = 0; i < num_res; i++)
    {
    if (strcmp(name,res_to_check[i].name) == 0)
      break;
    }

  if (i == num_res) /* not checking this resource */
    {
    return 1;
    }

  if ((res = find_resource(ninfo->res, name)) == NULL)
    return 0;

  if (res->is_string)
    {
    /* string resources work kind of like properties right now */
    if (strcmp(res->str_avail,value) == 0)
      return 1;
    }

  amount = res_to_num(value);

  switch (mode)
    {
    case MaxOnly:
      /* there is no current limit, only maximum, and we therefore can't check suitability,
       * so lets assume it is suitable. The maximum is infinity, then its definitely suitable,
       * or the maximum is more then we need.
       */
      if (res->max == UNSPECIFIED  || res->max == INFINITY || res->max >= amount)
        return 1;
      break;
    case Avail:
      if (res->max == INFINITY || res->max == UNSPECIFIED)
        /* we only have the avail value */
        {
        if (res->avail >= amount)
          return 1;
        }
      else
        {
        /* we don't have avail - only max and assigned */
        if (res->max - res->assigned >= amount)
          return 1;
        }
      break;
    }
  return 0; /* by default, the node does not have enough */
  }

int get_node_has_prop(node_info *ninfo, pars_prop* property, int preassign_starving)
  {
  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  if (property->value == NULL) /* property */
    {
    char **iter;

    /* the property is not a resource */
    iter = ninfo->properties;

    if (iter == NULL)
      return 0;

    while (*iter != NULL)
      {
      if (strcmp(*iter,property->name) == 0)
        return 1;

      iter++;
      }
    }
  else /* resource or ppn */
    {
    if (strcmp("ppn",property->name) == 0)
      {
      return node_has_enough_np(ninfo, property->value,
               preassign_starving?MaxOnly:Avail);
      }
    else
      {
      return node_has_enough_resource(ninfo,property->name, property->value,
               preassign_starving?MaxOnly:Avail);
      }
    }

  return 0;
  }

int get_node_has_property(node_info *ninfo, const char* property)
  {
  char **iter;
  char *delim = NULL;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  printf("DEBUG: Checking property %s\n",property);

  /* resource/ppn support */
  if ((delim = strchr(property,'=')) != NULL)
    {
    long amount = 0;
    char *name;

    name = strndup(property,delim-property);
    delim++;

    amount = res_to_num(delim);
    /* TODO what about string resources? */
    dbg_consistency(amount > 0,"resource conversion to number should not fail");

    if (!(strcmp("ppn",name)))
      {
      if (ninfo->np - ninfo->npshared >= amount &&
          ninfo->npfree >= amount)
        return 1;
      else
        return 0;
      }
    else /* generic resource */
      {
      struct resource *res;
      int i;

      printf("DEBUG Checking generic resource %s\n",property);

      for (i = 0; i < num_res; i++)
        {
        if (!(strcmp(name,res_to_check[i].name)))
          break;
        }

      if (i == num_res) /* not checking this resource */
        {
        printf("DEBUG: Resource not in res_to_check\n");
        return 1;
        }

      res = find_resource(ninfo->res, res_to_check[i].name);

      if (res == NULL)
        return 0;

      if (res->max - res->assigned >= amount)
        return 1;
      }

    free(name);
    }

  /* the property is not a resource */
  iter = ninfo->properties;

  if (iter == NULL)
    return 0;

  while (*iter != NULL)
    {
    if (strcmp(*iter,property) == 0)
      {
      return 1;
      }

    iter++;
    }
  return 0;
  }

/** Refresh magrathea status for node */
int refresh_magrathea_status(node_info *ninfo, job_info *jinfo, int preassign_starving)
  {
  resource *res_machine, *res_magrathea;

  res_machine = find_resource(ninfo->res, "machine_cluster");
  res_magrathea = find_resource(ninfo->res, "magrathea");

  if (preassign_starving == 0)
    {
    if ((res_magrathea != NULL) && res_magrathea->str_avail != NULL)
      { /* update magrathea status */
      int ret;
      long int m_status, m_count, m_used, m_free, m_possible;

      ret=magrathea_decode(res_magrathea,&m_status,&m_count,&m_used,&m_free,&m_possible);
      if ((ret) || (strcmp(res_magrathea->str_avail,"external")==0))
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                  "Node %s not used, magrathea state is %s",
                  ninfo->name,res_magrathea->str_avail);
        return 0;
        }

      if ((ninfo->type == NodeVirtual) && (m_possible == 1) && (ninfo->is_down))
        { ninfo->is_bootable=1; }
      else
        { ninfo->is_bootable=0; }

      if (m_status < 1)
        {
        if ((jinfo->cluster_mode == ClusterCreate) && ninfo->is_bootable)
          {
          /* ok */
          }
        else
          {
          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                    "Node %s is not used due to magrathea: %s",
                    ninfo->name, res_magrathea->str_avail);
          return 0;
          }
        }

      }
    }

  /* cloud support
   * should be used also in simulation
   */

  if (jinfo->cluster_mode == ClusterNone)
    {
    /* user does not require cluster */
    if (res_machine!=NULL)
      {
      /* but node already belongs to cluster */
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, already belongs to a cluster %s",
                ninfo->name, res_machine->str_avail);
      return 0;
      }
    }
  else if (jinfo->cluster_mode == ClusterCreate)
    {
    /* cluster creation */
    if (res_machine != NULL)
      {
      /* but node is already in (different cluster) */
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, belongs to a (different) cluster %s",
                ninfo->name, res_machine->str_avail);
      return 0;
      }
    }
  else if ((jinfo->cluster_mode == ClusterUse) && (jinfo->cluster_name != NULL))
    {
    /* user requires already running cluster */
    if ((res_machine == NULL) || (res_machine -> str_avail == NULL))
      {
      /* but node is not in cluster */
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, it doesn't belong to any cluster",
                ninfo->name);
      return 0;
      }

    if (strcmp(res_machine -> str_avail,jinfo->cluster_name)!=0)
      {
      /* but node is in different cluster */
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, it belongs to different cluster %s",
                ninfo->name, res_machine->str_avail);
      return 0;
      }
    }

  return 1;
  }

/** Check basic node suitability for job */
static int is_node_suitable(node_info *ninfo, job_info *jinfo, int preassign_starving)
  {
  if (ninfo->is_offline && ninfo->type != NodeVirtual)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
            "Node %s not used, node is down and is not virtual.", ninfo->name);
    return 0;
    }

  if (ninfo->type == NodeTimeshared || ninfo->type == NodeCloud)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node is Timesharing or Cloud.", ninfo->name);
    return 0;
    }

  if (ninfo->type == NodeVirtual && (!jinfo->is_cluster))
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node is virtual, but job doesn't require virtual cluster.", ninfo->name);
    return 0;
    }

  if (ninfo->type == NodeCluster && jinfo->is_cluster)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node is cluster, but job does require virtual cluster.", ninfo->name);
    return 0;
    }

  if ((ninfo->starving_job != NULL) && /* already assigned to a starving job */
      (strcmp(ninfo->starving_job->name,jinfo->name) != 0) && /* and not this job */
      (ninfo->starving_job->queue->priority >= jinfo->queue->priority)) /* job priority is not higher */
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node allocated to a higher priority starving job %s.",
              ninfo->name, ninfo->starving_job->name);
    return 0;
    }

  if (jinfo->is_multinode && ninfo->no_multinode_jobs)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node does not allow multinode jobs.", ninfo->name);
    return 0;
    }

  if (ninfo->temp_assign != NULL) /* node already assigned */
    return 0;

  if (preassign_starving == 0) /* only for non-starving jobs */
    {
    if ((jinfo->is_exclusive) && (ninfo->npfree != ninfo->np))
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, job is exclusive and node is not fully empty.", ninfo->name);
      return 0; /* skip non-empty nodes for exclusive requests */
      }

    if (ninfo->is_exclusive)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, node is already running an exclusive job.", ninfo->name);
      return 0;
      }
    }

  /* refresh and check magrathea status */
  if (refresh_magrathea_status(ninfo,jinfo,preassign_starving) == 0)
    return 0;

  /* virtual clusters support */
  if (jinfo->is_cluster && jinfo->cluster_mode == ClusterCreate)
    {

    if (!ninfo->is_bootable)
      return 0;

    if (ninfo->type == NodeVirtual) /* always true, checked before */
      {
      char *cmom;
      char *cloud_mom=NULL;
      node_info *cloudnode =NULL;
      cmom=pbs_cache_get_local(ninfo->name, "host");

      if (cmom)
        {
        cloud_mom=cache_value_only(cmom);
        free(cmom);
        }

      if (cloud_mom)
        cloudnode=find_node_info(cloud_mom,jinfo->queue->server->nodes);

      if (!cloudnode)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                  "Node %s not used, cloud node reported by cache (%s) was not found.", ninfo->name, cloud_mom);
        free(cloud_mom);
        return 0;
        }

      free(cloud_mom);

      if (!cloudnode -> type == NodeCloud)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                  "Node %s not used, cloud node is not known as cloud.", ninfo->name);
        return 0;
        }

      if (cloudnode -> is_down)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                  "Node %s not used, cloud node is down.", ninfo->name);
        return 0;
        }

      if (cloudnode -> jobs != NULL && cloudnode -> jobs[0] != NULL)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                  "Node %s not used, cloud node has other jobs", ninfo->name);
        }
      }

#if 0 /* node states broken right now */
    if (node -> is_free)
      {
      /* TODO, cloud - free, already running node. Can cloud used it? */
      }
#endif

    if (ninfo->np != ninfo->npfree)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, cloud requests require whole node, but node already contains a job.", ninfo->name);
      return 0;
      }

    if ((ninfo->is_bootable) && ((ninfo->alternatives == NULL) || (ninfo->alternatives[0] == NULL)))
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, bootable node without alternatives.", ninfo->name);
      return 0;
      }
    }

  return 1;
  }

static int assign_node(job_info *jinfo, pars_spec_node *spec, int exclusivity,
                       int avail_nodes, node_info **ninfo_arr, int preassign_starving)
  {
  int i, ret = 1;
  pars_prop *iter = spec->properties;
  repository_alternatives* ra;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (!is_node_suitable(ninfo_arr[i],jinfo,preassign_starving)) /* check node suitability */
      continue;

    /* check nodespec */
    ra = NULL;
    /* has alternatives */
    if (ninfo_arr[i]->alternatives != NULL && ninfo_arr[i]->alternatives[0] != NULL)
      {
      ra = ninfo_arr[i]->alternatives[0];
      while (ra != NULL)
        {
        while (iter != NULL)
          {
          if ((get_node_has_prop(ninfo_arr[i],iter,preassign_starving) == 0) &&
              (alternative_has_property(ra,iter->name) == 0))
            break; /* break out of the cycle if not found property */
          iter = iter->next;
          }
        if (iter == NULL)
          break;
        ra++;
        }

      if (ra == NULL)
        continue; /* no alternative matching the spec */
      }
    else /* else just do simple iteration */
      {
      while (iter != NULL)
        {
        if (get_node_has_prop(ninfo_arr[i],iter,preassign_starving) == 0)
          break; /* break out of the cycle if not found property */

        iter = iter->next;
        }

      if (iter != NULL) /* one of the properties not found */
        continue;
      }

    if (site_user_has_account(jinfo->account,ninfo_arr[i]->name,ninfo_arr[i]->cluster_name) == CHECK_NO)
      continue;

    if (preassign_starving)
      ninfo_arr[i]->starving_job = jinfo;
    else
      {
      ninfo_arr[i]->temp_assign = clone_pars_spec_node(spec);
      ninfo_arr[i]->temp_assign_alternative = ra;
      }

    ret = 0;
    break;
    }

  return ret;
  }

int check_nodespec(job_info *jinfo, int nodecount, node_info **ninfo_arr, int preassign_starving)
  {
  const char *node_spec;
  resource_req *res;
  int missed_nodes = 0;

  res = find_resource_req(jinfo -> resreq, "nodes");
  if (res != NULL)
    node_spec = res->res_str;

  /* TODO provide better fix (with respect to resource requests) */
  if (res == NULL || node_spec == NULL || node_spec[0] == '\0')
    node_spec = "1"; /* if there is no nodespec,
                        then assume its a request for one node */
#if 0
  if (res != NULL && node_spec != NULL)
#endif
    {
    pars_spec *spec;
    pars_spec_node *iter;

    if ((spec = parse_nodespec(node_spec)) == NULL)
      return SCHD_ERROR;

    jinfo->is_exclusive = spec->is_exclusive;
    jinfo->is_multinode = (spec->total_nodes > 1)?1:0;

    iter = spec->nodes;
    while (iter != NULL)
      {
      unsigned i, tot_nodes;

      if (preassign_starving == 1)
        tot_nodes = nodecount;
      else
        tot_nodes = iter->node_count;

      for (i = 0; i < iter->node_count; i++)
        {
        missed_nodes += assign_node(jinfo, iter, spec->is_exclusive, nodecount,
                                    ninfo_arr, preassign_starving);
        }

      iter = iter->next;
      }

    if (preassign_starving == 0)
    if (missed_nodes > 0) /* try reassigns */
      {
      nodes_preassign_clean(ninfo_arr,nodecount);
      return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
      }
    }


  return SUCCESS; /* if we reached this point, we are done */
  }

/** Clean job assignments
 *
 * @param ninfo_arr
 */
void nodes_preassign_clean(node_info **ninfo_arr, int count)
  {
  int i;

  if (ninfo_arr == NULL)
    return;

  for (i = 0; i < count; i++)
    {
    dbg_consistency(ninfo_arr[i] != NULL, "Given count/real count mismatch");

    if (ninfo_arr[i]->temp_assign != NULL)
      free_pars_spec_node(&ninfo_arr[i]->temp_assign);

    ninfo_arr[i]->temp_assign = NULL;
    ninfo_arr[i]->temp_assign_alternative = NULL;
    }
  }

/** Construct one part of nodespec from assigned node
 *
 * @param ninfo Node for which to construct the node part
 * @param mode Type of operation 0 - normal, 1 - no properties, 2 - only integer resources
 * @return New constructed nodespec part
 */
static char *get_target(node_info *ninfo, int mode)
  {
  char *str = NULL, *cp;
  pars_prop *iter;
  int len = 0;

  len += strlen(ninfo->name) + 1;

  iter = ninfo->temp_assign->properties;

  while (iter != NULL)
    {
    len += strlen(iter->name) + 1; /* :prop */
    if (iter->value != NULL)
      len += strlen(iter->value) + 1; /* =value */
    iter = iter->next;
    }

  if (ninfo->alternatives != NULL && ninfo->alternatives[0] != NULL)
    {
    len += strlen("alternative") + 1; /* :alternative */
    if (ninfo->temp_assign_alternative != NULL)
      {
      len += strlen(ninfo->temp_assign_alternative->r_name) + 1; /* =name */
      }
    else
      {
      len += strlen(ninfo->alternatives[0]->r_name) + 1;
      }
    }

  if ((str = malloc(len)) == NULL)
    return NULL;

  iter = ninfo->temp_assign->properties;
  cp = str;

  strcpy(cp,ninfo->name);
  cp += strlen(ninfo->name);

  while (iter != NULL)
    {
    if (mode == 0 || /* in mode 0 - normal nodespec */
        (mode == 1 && iter->value != NULL) || /* in mode 1 - properties only */
        (mode == 2 && iter->value != NULL && atoi(iter->value) > 0)) /* in mode 2 - integer properties only */
      {
      strcpy(cp,":"); cp++;
      strcpy(cp,iter->name); cp += strlen(iter->name);
      if (iter->value != NULL)
        {
        strcpy(cp,"="); cp++;
        strcpy(cp,iter->value); cp += strlen(iter->value);
        }
      }
    iter = iter->next;
    }

  if (ninfo->alternatives != NULL && ninfo->alternatives[0] != NULL)
    {
    strcpy(cp,":alternative="); cp += strlen(":alternative=");

    if (ninfo->temp_assign_alternative != NULL)
      {
      strcpy(cp,ninfo->temp_assign_alternative->r_name); cp += strlen(ninfo->temp_assign_alternative->r_name);
      }
    else
      {
      strcpy(cp,ninfo->alternatives[0]->r_name); cp += strlen(ninfo->alternatives[0]->r_name);
      }
    }

  return str;
  }

/** Construct a full target node specification (hostname:nodespec)
 *
 * @note Expects a valid node info
 */
static char *get_target_full(job_info *jinfo, node_info *ninfo)
  {
  dbg_precondition(jinfo != NULL, "This function does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This function does not accept NULL.");

  if (ninfo->temp_assign == NULL)
    return NULL;

  if (jinfo->cluster_mode == ClusterCreate)
    return get_target(ninfo,1);
  else
    return get_target(ninfo,0);
  }

/** Get the target string from preassigned nodes
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string(job_info *jinfo, node_info **ninfo_arr, int count)
  {
  char *str = NULL;
  int i;

  if (ninfo_arr == NULL)
    return str;

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL)
      {
      if (str == NULL)
        {
        str = get_target_full(jinfo, ninfo_arr[i]);

        if (str == NULL) /* alloc failure */
          return NULL;
        }
      else
        {
        char *tmp;
        char *append;

        /* get the full spec for this node */
        append = get_target_full(jinfo,ninfo_arr[i]);
        if (append == NULL)
          {
          free(str);
          return NULL;
          }

        tmp = malloc(strlen(str)+strlen(append)+2);
        if (tmp == NULL) /* alloc failure */
          {
          free(append);
          free(str);
          return NULL;
          }

        sprintf(tmp,"%s+%s",str,append);
        free(append);
        free(str);
        str = tmp;
        }
      }
    }

  dbg_consistency(str != NULL, "At this point, the target must be set.");

  if (!jinfo->is_exclusive)
    {
    char *tmp = malloc(strlen(str)+strlen("#shared")+1);
    if (tmp == NULL)
      return NULL;

    sprintf(tmp,"%s%s",str,"#shared");
    free(str);
    str = tmp;
    }
  else
    {
    char *tmp = malloc(strlen(str)+strlen("#excl")+1);
    if (tmp == NULL)
      return NULL;

    sprintf(tmp,"%s%s",str,"#excl");
    free(str);
    str = tmp;

    }

  return str;
  }
