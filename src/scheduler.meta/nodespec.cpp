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

#include <sstream>
#include <cassert>
using namespace std;

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
      case MagratheaStateDownDisappeared:
      case MagratheaStateShuttingDown:
        ninfo->is_usable_for_run = 0;
        sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
                  "Node marked as incapable of running jobs, because it has bad Magrathea state.");
        return 0;
      }
    }

  if (ninfo->type == NodeCluster || ninfo->type == NodeVirtual)
    {
    if (ninfo->is_offline() || ninfo->is_down() || ninfo->is_unknown())
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
      case MagratheaStateDownDisappeared:
      case MagratheaStateShuttingDown:
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

  int jobs_present = 0;
  if (ninfo->host != NULL)
    {
    if (ninfo->host->jobs != NULL && ninfo->host->jobs[0] != NULL)
      jobs_present = 1;

    for (size_t i = 0; i < ninfo->host->hosted.size(); i++)
      {
      if (ninfo->host->hosted[i]->jobs != NULL && ninfo->host->hosted[i]->jobs[0] != NULL)
        jobs_present = 1;
      }
    }
  else
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name, "Node marked as incapable of booting jobs, because the master cloud node was not found.");
    return 0;
    }

  if (ninfo->host -> type != NodeCloud)
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name, "Node marked as incapable of booting jobs, because the master cloud node is not known as cloud.");
    return 0;
    }

  if (ninfo->host -> is_down())
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name, "Node marked as incapable of booting jobs, because the master cloud node is down.");
    return 0;
    }

  if (jobs_present)
    {
    ninfo->is_usable_for_boot = 0;
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
              "Node marked as incapable of booting jobs, because it, one of it's sisters or the cloud node already contains a running job.");
    return 0;
    }

  return 1;
  }

int node_has_enough_resource(node_info *ninfo, char *name, char *value,
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

  if (ninfo->jobs != NULL && ninfo->jobs[0] != NULL)
    {
    // if there are already jobs on this node, the magrathea state can't be free/down-bootable
    if (ninfo->magrathea_status == MagratheaStateDownBootable || ninfo->magrathea_status == MagratheaStateFree)
      {
      ninfo->magrathea_status = MagratheaStateNone;
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_NODE, ninfo->name, "Node had inconsistent magrathea state.");
      }
    }

  if (ninfo->host != NULL)
  if (ninfo->host->jobs != NULL && ninfo->host->jobs[0] != NULL)
    {
    /*
    if (ninfo->magrathea_status == MagratheaStateFree)
      ninfo->magrathea_status = MagratheaStateNone;
    if (ninfo->magrathea_status == MagratheaStateRunning)
      ninfo->magrathea_status = MagratheaStateNone;
    if (ninfo->magrathea_status == MagratheaStateRunningPreemptible)
      ninfo->magrathea_status = MagratheaStateNone;
    if (ninfo->magrathea_status == MagratheaStateRunningPriority)
      ninfo->magrathea_status = MagratheaStateNone;
    */

    // if the host already has jobs, the magrathea state can't be down-bootable
    if (ninfo->magrathea_status == MagratheaStateDownBootable)
      {
      ninfo->magrathea_status = MagratheaStateNone;
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_NODE, ninfo->name, "Node had inconsistent magrathea state.");
      }
    }
  }

/** Check basic node suitability for job */
static int is_node_suitable(node_info *ninfo, job_info *jinfo, int preassign_starving)
  {
  // Node already assigned to previous part of nodespec
  if (ninfo->temp_assign != NULL) { return 0; }

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
      return 1;
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

  resource *res_machine = find_resource(ninfo->res, "machine_cluster");
  if (jinfo->cluster_mode != ClusterUse) /* users can always go inside a cluster */
    {
    // User does not have an account on this machine - can never run
    if (site_user_has_account(jinfo->account,ninfo->name,ninfo->cluster_name) == CHECK_NO)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Node %s not used, user does not have account on this node.", ninfo->name);
      return 0;
      }

    // Machine is already allocated to a virtual cluster, only ClusterUse type of jobs allowed
    if (res_machine != NULL)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Node %s not used, allocated to a virtual cluster %s", ninfo->name, res_machine->str_avail);
      return 0;
      }
    }
  else
    {
    if (jinfo->cluster_name == NULL)
      {
      return 0; // ClusterUse job without a cluster name, shouldn't happen
      }

    // Check whether this machine is running the cluster user is requesting
    if ((res_machine == NULL) || (res_machine -> str_avail == NULL))
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Node %s not used, node doesn't belong to a virtual cluster", ninfo->name);
      return 0;
      }

    if (strcmp(res_machine -> str_avail,jinfo->cluster_name)!=0)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Node %s not used, it belongs to different virtual cluster %s", ninfo->name, res_machine->str_avail);
      return 0;
      }
    }

  return 1;
  }

static int assign_node(job_info *jinfo, pars_spec_node *spec,
                       int avail_nodes, node_info **ninfo_arr, int preassign_starving)
  {
  int i;
  ScratchType scratch = ScratchNone;
  int fit_nonfit = 0, fit_occupied = 0;
  repository_alternatives** ra = NULL;
  CheckResult node_test;
  int fit_suit = 0;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (!is_node_suitable(ninfo_arr[i],jinfo,preassign_starving)) /* check node suitability */
      {
      fit_suit++;
      continue;
      }

    node_test = ninfo_arr[i]->has_spec(jinfo, spec, &scratch);
    if (node_test == CheckNonFit)
      {
      ++fit_nonfit;
      continue;
      }
    else if (node_test == CheckOccupied)
      {
      ++fit_occupied;
      continue;
      }

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      node_test = ninfo_arr[i]->has_props_run(jinfo,spec);
      if (node_test == CheckNonFit)
        {
        ++fit_nonfit;
        continue;
        }
      else if (node_test == CheckOccupied)
        {
        ++fit_occupied;
        continue;
        }
      }
    else if (jinfo->cluster_mode == ClusterCreate)
      {
      node_test = CheckNonFit;

      for (ra = ninfo_arr[i]->alternatives; *ra != NULL; ++ra)
        {
        node_test = ninfo_arr[i]->has_props_boot(jinfo,spec,*ra);
        if (node_test == CheckNonFit)
          {
          continue;
          }
        else if (node_test == CheckOccupied)
          {
          node_test = CheckOccupied;
          continue;
          }
        else
          {
          node_test = CheckAvailable;
          break;
          }
        }

      if (node_test == CheckNonFit)
        {
        ++fit_nonfit;
        continue;
        }
      else if (node_test == CheckOccupied)
        {
        ++fit_occupied;
        continue;
        }
      }

    if (preassign_starving)
      ninfo_arr[i]->starving_job = jinfo;
    else
      {
      ninfo_arr[i]->temp_assign = clone_pars_spec_node(spec);
      ninfo_arr[i]->temp_assign_scratch = scratch;
      if (ra != NULL)
        ninfo_arr[i]->temp_assign_alternative = *ra;
      else
        ninfo_arr[i]->temp_assign_alternative = NULL; /* FIXME META Prepsat do citelneho stavu */
      }

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec not matched: %d total nodes, %d not fit, %d occupied.", avail_nodes, fit_nonfit, fit_occupied);
  return 1;
  }


static int assign_all_nodes(job_info *jinfo, pars_spec_node *spec, int avail_nodes, node_info **ninfo_arr)
  {
  int i;
  repository_alternatives** ra;
  ScratchType scratch = ScratchNone;
  int fit_nonfit = 0, starving = 0;
  int fit_suit = 0;
  CheckResult node_test;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (!is_node_suitable(ninfo_arr[i],jinfo,1)) /* check node suitability */
      {
      fit_suit++;
      continue;
      }

    node_test = ninfo_arr[i]->has_spec(jinfo, spec, &scratch);
    if (node_test == CheckNonFit)
      {
      ++fit_nonfit;
      continue;
      }

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      node_test = ninfo_arr[i]->has_props_run(jinfo,spec);
      if (node_test == CheckNonFit)
        {
        ++fit_nonfit;
        continue;
        }
      }
    else if (jinfo->cluster_mode == ClusterCreate)
      {
      node_test = CheckNonFit;

      for (ra = ninfo_arr[i]->alternatives; *ra != NULL; ++ra)
        {
        node_test = ninfo_arr[i]->has_props_boot(jinfo,spec,*ra);
        if (node_test == CheckNonFit)
          {
          continue;
          }
        else if (node_test == CheckOccupied)
          {
          node_test = CheckOccupied;
          break;
          }
        else
          {
          node_test = CheckAvailable;
          break;
          }
        }

      if (node_test == CheckNonFit)
        {
        ++fit_nonfit;
        continue;
        }
      }

    ++starving;
    jinfo->plan_on_node(ninfo_arr[i],spec);
    continue;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Starving job: %d total nodes, %d starved nodes.", avail_nodes, starving);
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
    if (jinfo->queue->is_admin_queue)
      {
      free_parsed_nodespec(spec);
      return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
      }
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

    free_parsed_nodespec(spec);
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
