#include "data_types.h"
#include "assertions.h"
#include "node_info.h"
#include "job_info.h"
#include "misc.h"
#include "globals.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assertions.h>
#include "site_pbs_cache_scheduler.h"
extern "C" {
#include "nodespec.h"
}

#include "api.hpp"

#include <sstream>
#include <cassert>
using namespace std;

reschecksource res_check_type(const char * res_name)
  {
  int i;
  for (i = 0; i < num_res; i++)
    {
    if (strcmp(res_name,res_to_check[i].name) == 0)
      return res_to_check[i].source;
    }

  return ResCheckNone;
  }

static int node_is_suitable_for_run(node_info *ninfo)
  {
  if (!ninfo->is_usable_for_run)
    return 0;

  if (ninfo->type == NodeTimeshared || ninfo->type == NodeCloud)
    {
    ninfo->is_usable_for_run = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as incapable of running jobs, because Timesharing or Cloud.");
    return 0;
    }

  if (ninfo->type == NodeVirtual)
    {
    switch (ninfo->magrathea_status)
      {
      case MagratheaStateBooting:
      case MagratheaStateFree:
      case MagratheaStateOccupiedWouldPreempt:
      case MagratheaStateRunning:
      case MagratheaStateRunningPreemptible:
      case MagratheaStateRunningPriority:
      case MagratheaStateRunningCluster:
        break;

      case MagratheaStateNone:
      case MagratheaStateDown:
      case MagratheaStateDownBootable:
      case MagratheaStateFrozen:
      case MagratheaStateOccupied:
      case MagratheaStatePreempted:
      case MagratheaStateRemoved:
        ninfo->is_usable_for_run = 0;
        sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
                  "Node marked as incapable of running jobs, because it has bad Magrathea state.");
        return 0;
      }
    }

  if (ninfo->type == NodeCluster || ninfo->type == NodeVirtual)
    {
    if (ninfo->is_offline || ninfo->is_down || ninfo->is_unknown)
      {
      ninfo->is_usable_for_run = 0;
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
                "Node marked as incapable of running jobs, because it has wrong state.");
      return 0;
      }
    }

  return 1;
  }

static int node_is_suitable_for_boot(node_info *ninfo)
  {
  if (!ninfo->is_usable_for_boot)
    return 0;

  if (ninfo->type == NodeTimeshared || ninfo->type == NodeCloud || ninfo->type == NodeCluster)
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as incapable of booting jobs, because Timesharing or Cloud or Cluster.");
    return 0;
    }

  if (ninfo->type == NodeVirtual)
    {
    switch (ninfo->magrathea_status)
      {
      case MagratheaStateDownBootable:
        break;

      case MagratheaStateBooting:
      case MagratheaStateFree:
      case MagratheaStateOccupiedWouldPreempt:
      case MagratheaStateRunning:
      case MagratheaStateRunningPreemptible:
      case MagratheaStateRunningPriority:
      case MagratheaStateRunningCluster:
      case MagratheaStateNone:
      case MagratheaStateDown:
      case MagratheaStateFrozen:
      case MagratheaStateOccupied:
      case MagratheaStatePreempted:
      case MagratheaStateRemoved:
        ninfo->is_usable_for_boot = 0;
        sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
                  "Node marked as incapable of booting jobs, because it has bad Magrathea state.");
        return 0;
      }
    }

  if (ninfo->alternatives == NULL || ninfo->alternatives[0] == NULL)
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as incapable of booting jobs, because it doesn't have alternatives.");
    return 0; 
    }

  return 1;
  }

static int node_is_not_full(node_info *ninfo)
  {
  if (ninfo->is_full)
    return 0;

  if (ninfo->is_exclusively_assigned)
    {
    ninfo->is_full = 1;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as full, because it is already exclusively assigned.");
    return 0;
    }

  if (ninfo->npfree - ninfo->npassigned <= 0)
    {
    ninfo->is_full = 1;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as full, because it has no free slots.");
    }

  return 1;
  }

enum ResourceCheckMode { MaxOnly, Avail };

static int node_has_enough_np(node_info *ninfo, int ppn, enum ResourceCheckMode mode)
  {
  switch(mode)
    {
    case MaxOnly:
      if (ninfo->np >= ppn)
        return 1;
      break;
    case Avail:
      if (ninfo->npfree - ninfo->npassigned >= ppn)
        return 1;
      else
        return 0;
      break;
    }
  return 0; /* default is no */
  }

static int node_has_enough_resource(node_info *ninfo, char *name, char *value,
    enum ResourceCheckMode mode)
  {
  struct resource *res;
  long amount = 0;

  if (res_check_type(name) == ResCheckNone) /* not checking this resource */
    return 1;

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
        if (res->avail - res->assigned >= amount)
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

int get_node_has_mem(node_info *ninfo, pars_spec_node* spec, int preassign_starving)
  {
  struct resource *mem, *vmem;

  mem = find_resource(ninfo->res, "mem");
  vmem = find_resource(ninfo->res, "vmem");

  if (mem == NULL && spec->mem != 0) /* no memory on node, but memory requested */
      return 0;

  if (vmem == NULL && spec->vmem != 0) /* no virtual memory on node, but memory requested */
    return 0;

  if (preassign_starving)
    {
    if (mem != NULL && spec->mem > mem->max) /* memory on node, but not enough */
      return 0;

     if (vmem != NULL && spec->vmem > vmem->max) /* virtual memory on node, but not enough */
      return 0;
    }
  else
    {
    if (mem != NULL && spec->mem > mem->max - mem->assigned) /* memory on node, but not enough */
      return 0;

    if (vmem != NULL && spec->vmem > vmem->max - vmem->assigned) /* virtual memory on node, but not enough */
      return 0;
    }

  return 1;
  }

int get_node_has_ppn(node_info *ninfo, unsigned ppn, int preassign_starving)
  {
  return node_has_enough_np(ninfo, ppn, preassign_starving?MaxOnly:Avail);
  }

int get_node_has_prop(node_info *ninfo, pars_prop* property, int preassign_starving)
  {
  unsigned negative = 0;
  char *name, *value;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  name = property->name;
  value = property->value;

  if (name[0] == '^')
    {
    negative = 1;
    name++;
    }

  if (property->value == NULL) /* property, not a resource */
    {
    char **iter;

    /* first check if it is node name */
    if (strcmp(ninfo->name,name) == 0 ||
        (value != NULL && strcmp(name,"host") && strcmp(value,ninfo->name)))
      {
      return !negative;
      }

    iter = ninfo->properties;

    if (iter != NULL)
    while (*iter != NULL)
      {
      if (strcmp(*iter,name) == 0)
        return !negative;

      iter++;
      }

    iter = ninfo->adproperties;

    if (iter == NULL)
      return negative;

    while (*iter != NULL)
      {
      if (strcmp(*iter,name) == 0)
        return !negative;

      iter++;
      }

    }
  else /* resource or ppn */
    {
    return node_has_enough_resource(ninfo, name, value, preassign_starving?MaxOnly:Avail);
    }

  return negative; /* if negative property and not found -> return 1 */
  }

int get_node_has_property(node_info *ninfo, const char* property)
  {
  char *buf;
  pars_prop *prop;
  int ret;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  buf = strdup(property);
  prop = parse_prop(buf);
  ret = get_node_has_prop(ninfo,prop,0);
  free_pars_prop(&prop);
  free(buf);

  return ret;
  }

/** Phony test, real test done in check.c */
int is_dynamic_resource(pars_prop* property)
  {
  if (res_check_type(property->name) == ResCheckDynamic)
    return 1;
  else
    return 0;
  }

void node_set_magrathea_status(node_info *ninfo)
  {
  resource *res_magrathea;
  res_magrathea = find_resource(ninfo->res, "magrathea");

  if (res_magrathea != NULL)
    {
    if (magrathea_decode_new(res_magrathea,&ninfo->magrathea_status) != 0)
      ninfo->magrathea_status = MagratheaStateNone;
    }
  else
    {
    ninfo->magrathea_status = MagratheaStateNone;
    }
  }

/** Refresh magrathea status for node */
int refresh_magrathea_status(node_info *ninfo, job_info *jinfo, int preassign_starving)
  {
  resource *res_machine = find_resource(ninfo->res, "machine_cluster");

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
  if (jinfo->cluster_mode == ClusterCreate)
    {
    if (!node_is_suitable_for_boot(ninfo))
      return 0;
    }
  else
    {
    if (!node_is_suitable_for_run(ninfo))
      return 0;

    /* quick-skip for admin jobs */
    if (jinfo->queue->is_admin_queue)
      return ninfo->admin_slot_available;

    if (preassign_starving == 0 && (!node_is_not_full(ninfo)))
      return 0;
    }

  if (ninfo->type == NodeCluster && jinfo->is_cluster)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node is cluster, but job does require virtual cluster.", ninfo->name);
    return 0;
    }

  if (ninfo->no_starving_jobs && preassign_starving)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s set not to be used for starving jobs.",ninfo->name);
    return 0;
    }

  if (jinfo->is_multinode && ninfo->no_multinode_jobs)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node does not allow multinode jobs.", ninfo->name);
    return 0;
    }

  if (ninfo->temp_assign != NULL) /* node already assigned */
  {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, node already assigned.", ninfo->name);
    return 0;
  }

  if (preassign_starving == 0) /* only for non-starving jobs */
    {
    if ((jinfo->is_exclusive) && (ninfo->npfree - ninfo->npassigned != ninfo->np))
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
                "Node %s not used, job is exclusive and node is not fully empty.", ninfo->name);
      return 0; /* skip non-empty nodes for exclusive requests */
      }
    }

  if (jinfo->cluster_mode != ClusterUse && /* do not check user accounts inside virtual clusters */
      site_user_has_account(jinfo->account,ninfo->name,ninfo->cluster_name) == CHECK_NO)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, user does not have account on this node.", ninfo->name);
    return 0;
    }

  /* refresh and check magrathea status */
  if (refresh_magrathea_status(ninfo,jinfo,preassign_starving) == 0)
  {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
              "Node %s not used, could not refresh magrathea status.", ninfo->name);
    return 0;
  }

  /* virtual clusters support */
  if (jinfo->is_cluster && jinfo->cluster_mode == ClusterCreate)
    {
    if (ninfo->type == NodeVirtual) /* always true, checked before */
      {
      char *cmom;
      char *cloud_mom=NULL;
      node_info *cloudnode =NULL;
      cmom=xpbs_cache_get_local(ninfo->name, "host");

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
	      return 0;
        }
      }
    }

  return 1;
  }

static int assign_node(job_info *jinfo, pars_spec_node *spec,
                       int avail_nodes, node_info **ninfo_arr, int preassign_starving)
  {
  int i;
  pars_prop *iter = NULL;
  repository_alternatives** ra;
  int fit_suit = 0, fit_ppn = 0, fit_mem = 0, fit_prop = 0;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (!is_node_suitable(ninfo_arr[i],jinfo,preassign_starving)) /* check node suitability */
      {
      fit_suit++;
      continue;
      }

    if (!jinfo->queue->is_admin_queue)
    if (get_node_has_ppn(ninfo_arr[i],spec->procs,preassign_starving) == 0)
      {
      fit_ppn++;
      continue;
      }

    if (get_node_has_mem(ninfo_arr[i],spec,preassign_starving) == 0)
      {
      fit_mem++;
      continue;
      }

    /* check nodespec */
    ra = NULL;

    /* has alternatives */
    if (jinfo->cluster_mode==ClusterCreate && ninfo_arr[i]->alternatives != NULL
        && ninfo_arr[i]->alternatives[0] != NULL)
      {
      ra = ninfo_arr[i]->alternatives;
      while (*ra != NULL)
        {
        iter = spec->properties;
        while (iter != NULL)
          {
          if ((get_node_has_prop(ninfo_arr[i],iter,preassign_starving) == 0) &&
              (alternative_has_property(*ra,iter->name) == 0) &&
              (is_dynamic_resource(iter) == 0))
            break; /* break out of the cycle if not found property */

          iter = iter->next;
          }

        if (iter == NULL)
          break;
        ra++;
        }

      if (*ra == NULL)
        {
        fit_prop++;
        continue; /* no alternative matching the spec */
        }
      }
    else /* else just do simple iteration */
      {
      iter = spec->properties;
      while (iter != NULL)
        {
        if (get_node_has_prop(ninfo_arr[i],iter,preassign_starving) == 0 &&
            is_dynamic_resource(iter) == 0)
          break; /* break out of the cycle if not found property */

        iter = iter->next;
        }

      if (iter != NULL) /* one of the properties not found */
        {
        fit_prop++;
        continue;
        }
      }

    if (preassign_starving)
      ninfo_arr[i]->starving_job = jinfo;
    else
      {
      ninfo_arr[i]->temp_assign = clone_pars_spec_node(spec);
      if (ra != NULL)
        ninfo_arr[i]->temp_assign_alternative = *ra;
      else
        ninfo_arr[i]->temp_assign_alternative = NULL; /* FIXME META Prepsat do citelneho stavu */
      }

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
            "Nodespec not matched: [%d/%d] nodes are not suitable for job, [%d/%d] don't have enough free CPU, [%d/%d] don't have enough memory, [%d/%d] nodes don't match some properties/resources requested.",
            fit_suit, avail_nodes, fit_ppn, avail_nodes, fit_mem, avail_nodes, fit_prop, avail_nodes);
  return 1;
  }


static int assign_all_nodes(job_info *jinfo, pars_spec_node *spec, int avail_nodes, node_info **ninfo_arr)
  {
  int i;
  pars_prop *iter = NULL;
  repository_alternatives** ra;
  int fit_suit = 0, fit_ppn = 0, fit_mem = 0, fit_prop = 0;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (!is_node_suitable(ninfo_arr[i],jinfo,1)) /* check node suitability */
      {
      fit_suit++;
      continue;
      }

    if (!jinfo->queue->is_admin_queue)
    if (get_node_has_ppn(ninfo_arr[i],spec->procs,1) == 0)
      {
      fit_ppn++;
      continue;
      }

    if (get_node_has_mem(ninfo_arr[i],spec,1) == 0)
      {
      fit_mem++;
      continue;
      }

    /* check nodespec */
    ra = NULL;

    /* has alternatives */
    if (jinfo->cluster_mode==ClusterCreate && ninfo_arr[i]->alternatives != NULL
        && ninfo_arr[i]->alternatives[0] != NULL)
      {
      ra = ninfo_arr[i]->alternatives;
      while (*ra != NULL)
        {
        iter = spec->properties;
        while (iter != NULL)
          {
          if ((get_node_has_prop(ninfo_arr[i],iter,1) == 0) &&
              (alternative_has_property(*ra,iter->name) == 0) &&
              (is_dynamic_resource(iter) == 0))
            break; /* break out of the cycle if not found property */

          iter = iter->next;
          }

        if (iter == NULL)
          break;
        ra++;
        }

      if (*ra == NULL)
        {
        fit_prop++;
        continue; /* no alternative matching the spec */
        }
      }
    else /* else just do simple iteration */
      {
      iter = spec->properties;
      while (iter != NULL)
        {
        if (get_node_has_prop(ninfo_arr[i],iter,1) == 0 &&
            is_dynamic_resource(iter) == 0)
          break; /* break out of the cycle if not found property */

        iter = iter->next;
        }

      if (iter != NULL) /* one of the properties not found */
        {
        fit_prop++;
        continue;
        }
      }

    jinfo->plan_on_node(ninfo_arr[i],spec);
    continue;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name,
            "Nodespec not matched: [%d/%d] nodes are not suitable for job, [%d/%d] don't have enough free CPU, [%d/%d] don't have enough memory, [%d/%d] nodes don't match some properties/resources requested.",
            fit_suit, avail_nodes, fit_ppn, avail_nodes, fit_mem, avail_nodes, fit_prop, avail_nodes);
  return 1;
  }


int check_nodespec(server_info *sinfo, job_info *jinfo, int nodecount, node_info **ninfo_arr, int preassign_starving)
  {
  /* read the nodespec, has to be sent from server */
  const char *node_spec = jinfo->nodespec;
  if ( node_spec == NULL || node_spec[0] == '\0')
    {
    sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER,
        jinfo->name,"No nodespec was provided for this job, assuming 1:ppn=1.");
    node_spec = "1:ppn=1";
    }

  /* re-parse the nodespec */
  pars_spec *spec;
  if ((spec = parse_nodespec(node_spec)) == NULL)
    return SCHD_ERROR;

  /* setup some side values, that need parsed nodespec to be determined */
  jinfo->is_exclusive = spec->is_exclusive;
  jinfo->is_multinode = (spec->total_nodes > 1)?1:0;

  int missed_nodes = 0;

  /* for each part of the nodespec, try to assign the requested amount of nodes */
  pars_spec_node *iter;
  iter = spec->nodes;
  while (iter != NULL)
    {
    for (unsigned i = 0; i < iter->node_count; i++)
      {
      missed_nodes += assign_node(jinfo, iter, nodecount, ninfo_arr, 0);
      }
    if (missed_nodes > 0)
      break;

    iter = iter->next;
    }

  if (missed_nodes > 0) /* some part of nodespec couldn't be assigned */
    {
    nodes_preassign_clean(ninfo_arr,nodecount);
    if (jinfo->is_starving) /* if starving, eat out the resources anyway */
      {
      iter = spec->nodes;
      while (iter != NULL)
        {
        assign_all_nodes(jinfo, iter, nodecount, ninfo_arr);
        iter = iter->next;
        }
      jinfo->plan_on_server(sinfo);
      jinfo->plan_on_queue(jinfo->queue);
      }
    return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
    }

  free_parsed_nodespec(spec);

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
static void get_target(stringstream& s, node_info *ninfo, int mode)
  {
  int skip;
  pars_prop *iter;

  s << ninfo->name << ":ppn=" << ninfo->temp_assign->procs;
  s << ":mem=" << ninfo->temp_assign->mem << "KB";
  s << ":vmem=" << ninfo->temp_assign->vmem << "KB";

  iter = ninfo->temp_assign->properties;
  while (iter != NULL)
    {
    skip = 0;

    if (res_check_type(iter->name) == ResCheckDynamic)
      skip = 1;
    if (res_check_type(iter->name) == ResCheckCache)
      skip = 1;

    if ((!skip) && (mode == 0 || /* in mode 0 - normal nodespec */
        (mode == 1 && iter->value != NULL) || /* in mode 1 - properties only */
        (mode == 2 && iter->value != NULL && atoi(iter->value) > 0))) /* in mode 2 - integer properties only */
      {
      s << ":" << iter->name;
      if (iter->value != NULL)
        s << "=" << iter->value;
      }
    iter = iter->next;
    }

  if (ninfo->alternatives != NULL && ninfo->alternatives[0] != NULL)
    {
    s << ":alternative=";

    if (ninfo->temp_assign_alternative != NULL)
      s << ninfo->temp_assign_alternative->r_name;
    else
      s << ninfo->alternatives[0]->r_name;
    }

  }

/** Construct a full target node specification (hostname:nodespec)
 *
 * @note Expects a valid node info
 */
static void get_target_full(stringstream& s, job_info *jinfo, node_info *ninfo)
  {
  dbg_precondition(jinfo != NULL, "This function does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This function does not accept NULL.");

  if (ninfo->temp_assign == NULL)
    return;

  if (jinfo->cluster_mode == ClusterCreate)
    get_target(s,ninfo,1);
  else
    get_target(s,ninfo,0);
  }

/** Get the target string from preassigned nodes
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string(job_info *jinfo, node_info **ninfo_arr, int count, int *booting)
  {
  stringstream s;
  bool first = true;
  int i;

  assert(ninfo_arr != NULL);

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL && ninfo_arr[i]->magrathea_status == MagratheaStateBooting)
      {
      *booting = 1;
      return NULL;
      }
    }

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL)
      {
      if (!first) s << "+";
      first = false;
      get_target_full(s,jinfo,ninfo_arr[i]);
      }
    }

  if (jinfo->is_exclusive)
    s << "#excl";

  return strdup(s.str().c_str());
  }
