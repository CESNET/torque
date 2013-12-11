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

#include "RescInfoDb.h"

#include "NodeFilters.h"
#include "NodeSort.h"

#include <sstream>
#include <cassert>
#include <cmath>
#include <algorithm>
using namespace std;

static int assign_node(job_info *jinfo, pars_spec_node *spec, const vector<node_info*>& suitable_nodes)
  {
  ScratchType scratch = ScratchNone;
  int fit_occupied = 0;
  repository_alternatives *ra = NULL;
  CheckResult node_test;

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    if (suitable_nodes[i]->temp_assign != NULL)
      continue;

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      if ((node_test = suitable_nodes[i]->can_fit_job_for_run(jinfo,spec,&scratch)) != CheckAvailable)
        {
        ++fit_occupied;
        continue;
        }
      }
    else if (jinfo->cluster_mode == ClusterCreate)
      {
      if ((node_test = suitable_nodes[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) != CheckAvailable)
        {
        ++fit_occupied;
        continue;
        }
      }

    suitable_nodes[i]->temp_assign = clone_pars_spec_node(spec);
    suitable_nodes[i]->temp_assign_scratch = scratch;

    if (ra != NULL)
      suitable_nodes[i]->temp_assign_alternative = ra;
    else
      suitable_nodes[i]->temp_assign_alternative = NULL; /* FIXME META Prepsat do citelneho stavu */

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec not matched: %d suitable, %d occupied.", suitable_nodes.size(), fit_occupied);
  return 1;
  }


static int ondemand_reboot(job_info *jinfo, pars_spec_node *spec, const vector<node_info*>& suitable_nodes)
  {
  ScratchType scratch = ScratchNone;
  int fit_occupied = 0;
  repository_alternatives *ra = NULL;
  CheckResult node_test;

  if (jinfo->cluster_mode != ClusterNone) // only for normal jobs
    return 2;

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    if (suitable_nodes[i]->temp_assign != NULL)
      continue;

    if ((node_test = suitable_nodes[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) != CheckAvailable)
      {
      ++fit_occupied;
      continue;
      }

    suitable_nodes[i]->temp_assign = clone_pars_spec_node(spec);
    suitable_nodes[i]->temp_assign_scratch = scratch;

    if (ra != NULL)
      suitable_nodes[i]->temp_assign_alternative = ra;
    else
      suitable_nodes[i]->temp_assign_alternative = NULL; /* FIXME META Prepsat do citelneho stavu */

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec not matched: %d suitable, %d occupied.", suitable_nodes.size(), fit_occupied);
  return 1;
  }

static pars_spec_node* merge_node_specs(pars_spec_node *first, pars_spec_node *second)
  {
  pars_spec_node *result = clone_pars_spec_node(first);
  result->mem = max(result->mem,second->mem);
  result->vmem = max(result->vmem,second->vmem);
  result->procs = max(result->procs,second->procs);
  result->scratch = max(result->scratch,second->scratch);

  pars_prop *iter = second->properties;
  while (iter != NULL)
    {
    if (iter->value == NULL)
      { iter = iter->next; continue; }

    if (res_check_type(iter->name) != ResCheckNone)
      {
      pars_prop *iter2 = result->properties;
      while (iter2 != NULL && strcmp(iter2->name,iter->name) != 0)
        { iter2 = iter2->next; }

      if (iter2 == NULL)
        {
        pars_prop *prop = init_pars_prop();
        prop->name = strdup(iter->name);
        prop->value = strdup(iter->value);
        if (result->properties_end == NULL)
          {
          result->properties = prop;
          result->properties_end = prop;
          }
        else
          {
          result->properties_end->next = prop;
          prop->prev = result->properties_end;
          result->properties_end = prop;
          }
        }
      else
        {
        sch_resource_t value1 = res_to_num(iter->value);
        sch_resource_t value2 = res_to_num(iter2->value);

        if (value1 >= value2)
          {
          free(iter2->value);
          iter2->value = strdup(iter->value);
          }
        }
      }
    iter = iter->next;
    }

  return result;
  }

static int assign_all_nodes(job_info *jinfo, pars_spec_node *spec, const vector<node_info*>& nodes)
  {
  int starving = 0;

  vector<node_info*> suitable_nodes;
  NodeSuitableForSpec::filter_starving(nodes,suitable_nodes,jinfo,spec);

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    node_info *node = suitable_nodes[i];
    if (node->starving_spec != NULL)
      {
      pars_spec_node *starved = node->starving_spec;
      jinfo->unplan_from_node(node,starved);

      // join specs
      pars_spec_node *joined = merge_node_specs(spec,starved);
      jinfo->plan_on_node(node,joined);
      free_pars_spec_node(&starved);
      node->starving_spec = joined;
      }
    else
      {
      jinfo->plan_on_node(node,spec);
      pars_spec_node *joined = clone_pars_spec_node(spec);
      node->starving_spec = joined;
      }
    ++starving;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Starving job: %d total nodes, %d starved nodes.", nodes.size(), starving);
  return 1;
  }

int check_nodespec(server_info *sinfo, job_info *jinfo, int nodecount, node_info **ninfo_arr, int preassign_starving)
  {
  int missed_nodes = 0;
  CheckResult result = CheckAvailable;

  vector<node_info*> nodes(&ninfo_arr[0],&ninfo_arr[nodecount]);

  // nodes suitable for job
  vector<node_info*> suitable_nodes;
  NodeSuitableForJob::filter(nodes,suitable_nodes,jinfo);

  /* for each part of the nodespec, try to assign the requested amount of nodes */
  pars_spec_node *iter = jinfo->parsed_nodespec->nodes;
  while (iter != NULL)
    {
    // nodes suitable for nodespec
    vector<node_info*> fit_nodes, reboot_nodes;
    NodeSuitableForSpec::filter_assign(suitable_nodes,fit_nodes,jinfo,iter);
    NodeSuitableForSpec::filter_reboot(suitable_nodes,reboot_nodes,jinfo,iter);

    if (fit_nodes.size() + reboot_nodes.size() < iter->node_count)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec doesn't match enough nodes. Job held.");
      nodes_preassign_clean(ninfo_arr,nodecount);
      return REQUEST_NOT_MATCHED;
      }

    // sort nodes according to state & schedule
    sort(fit_nodes.begin(),fit_nodes.end(),NodeStateSort());
    sort(reboot_nodes.begin(),reboot_nodes.end(),NodeStateSort());

    for (unsigned i = 0; i < iter->node_count; i++)
      {
      // first try normal execution
      int ret = assign_node(jinfo,iter,fit_nodes);

      // if there are some rebootable nodes and we didn't find any normal nodes, try ondemand reboot
      if (ret != 0 && reboot_nodes.size() > 0)
        {
        ret = ondemand_reboot(jinfo,iter,reboot_nodes);
        }

      if (ret == 2)
        {
        result = CheckNonFit;
        break;
        }
      else
        {
        missed_nodes += ret;
        }
      }

    if (missed_nodes > 0)
      {
      result = CheckOccupied;
      break;
      }

    iter = iter->next;
    }

  if (result == CheckNonFit)
    {
    nodes_preassign_clean(ninfo_arr,nodecount);
    return REQUEST_NOT_MATCHED;
    }

  if (missed_nodes > 0 && result != CheckNonFit) /* some part of nodespec couldn't be assigned */
    {
    if (jinfo->queue->is_admin_queue)
      {
      return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
      }

    nodes_preassign_clean(ninfo_arr,nodecount);
    if (jinfo->is_starving) /* if starving, eat out the resources anyway */
      {
      iter = jinfo->parsed_nodespec->nodes;
      while (iter != NULL)
        {
        assign_all_nodes(jinfo, iter, suitable_nodes);
        iter = iter->next;
        }
      jinfo->plan_on_server(sinfo);
      jinfo->plan_on_queue(jinfo->queue);
      }

    return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
    }

  jinfo->calculated_fairshare = jinfo->calculate_fairshare_cost(suitable_nodes);
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
    assert(ninfo_arr[i] != NULL);

    if (ninfo_arr[i]->temp_assign != NULL)
      free_pars_spec_node(&ninfo_arr[i]->temp_assign);

    ninfo_arr[i]->temp_assign = NULL;
    ninfo_arr[i]->temp_assign_alternative = NULL;
    ninfo_arr[i]->temp_fairshare_used = false;

    if (ninfo_arr[i]->starving_spec != NULL)
      {
      free_pars_spec_node(&(ninfo_arr[i]->starving_spec));
      ninfo_arr[i]->starving_spec = NULL;
      }
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

  s << "host=" << ninfo->name << ":ppn=" << ninfo->temp_assign->procs;
  s << ":mem=" << ninfo->temp_assign->mem << "KB";
  s << ":vmem=" << ninfo->temp_assign->vmem << "KB";

  iter = ninfo->temp_assign->properties;
  while (iter != NULL)
    {
    skip = 0;

    /* avoid duplicate hostname properties */
    if (strcmp(ninfo->name,iter->name) == 0)
      {
      iter = iter->next;
      continue;
      }

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

  if (ninfo->temp_assign_scratch != ScratchNone)
    {
    s << ":scratch_type=";
    if (ninfo->temp_assign_scratch == ScratchSSD)
      s << "ssd";
    else if (ninfo->temp_assign_scratch == ScratchShared)
      s << "shared";
    else if (ninfo->temp_assign_scratch == ScratchLocal)
      s << "local";
    s << ":scratch_volume=" << ninfo->temp_assign->scratch / 1024 << "mb";
    }

  if (ninfo->alternatives != NULL && ninfo->alternatives[0] != NULL && ninfo->temp_assign_alternative != NULL)
    {
    s << ":alternative=" << ninfo->temp_assign_alternative->r_name;
    }

  }

/** Construct a full target node specification (hostname:nodespec)
 *
 * @note Expects a valid node info
 */
static void get_target_full(stringstream& s, job_info *jinfo, node_info *ninfo, bool cluster)
  {
  assert(jinfo != NULL || ninfo != NULL);

  if (ninfo->temp_assign == NULL)
    return;

  if (jinfo->cluster_mode == ClusterCreate || cluster)
    get_target(s,ninfo,1);
  else
    get_target(s,ninfo,0);
  }

/** Get the target string from preassigned nodes
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string(job_info *jinfo, node_info **ninfo_arr, int count, int &booting, double &minspec)
  {
  stringstream s;
  bool cluster = false;
  bool first = true;
  int i;

  booting = 0;
  minspec = -1;

  assert(ninfo_arr != NULL);

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL && ninfo_arr[i]->magrathea_status == MagratheaStateBooting)
      {
      booting = 1;
      return NULL;
      }
    }

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign_alternative != NULL)
      cluster = true;
    }

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL)
      {
      if (!first) s << "+";
      first = false;
      get_target_full(s,jinfo,ninfo_arr[i],cluster);

      if (minspec == -1)
        minspec = ninfo_arr[i]->node_spec;
      else
        minspec = min(minspec,ninfo_arr[i]->node_spec);
      }
    }

  if (jinfo->is_exclusive)
    s << "#excl";

  return strdup(s.str().c_str());
  }
