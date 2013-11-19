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

static int assign_node(job_info *jinfo, pars_spec_node *spec, int avail_nodes, node_info **ninfo_arr)
  {
  int i;
  ScratchType scratch = ScratchNone;
  int fit_nonfit = 0, fit_occupied = 0;
  repository_alternatives *ra;
  CheckResult node_test;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (ninfo_arr[i]->temp_assign != NULL)
      {
      fit_occupied++;
      continue;
      }

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      if ((node_test = ninfo_arr[i]->can_fit_job_for_run(jinfo,spec,&scratch)) == CheckNonFit)
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
      if ((node_test = ninfo_arr[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) == CheckNonFit)
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

    ninfo_arr[i]->temp_assign = clone_pars_spec_node(spec);
    ninfo_arr[i]->temp_assign_scratch = scratch;

    if (ra != NULL)
      ninfo_arr[i]->temp_assign_alternative = ra;
    else
      ninfo_arr[i]->temp_assign_alternative = NULL; /* FIXME META Prepsat do citelneho stavu */

    return 0;
    }

  if (fit_nonfit == avail_nodes)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec doesn't match any nodes. Job held.");
    return 2;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Nodespec not matched: %d total nodes, %d not fit, %d occupied.", avail_nodes, fit_nonfit, fit_occupied);
  return 1;
  }


static int assign_all_nodes(job_info *jinfo, pars_spec_node *spec, int avail_nodes, node_info **ninfo_arr)
  {
  int i;
  repository_alternatives* ra;
  ScratchType scratch = ScratchNone;
  int fit_nonfit = 0, starving = 0;
  CheckResult node_test;

  for (i = 0; i < avail_nodes; i++) /* for each node */
    {
    if (ninfo_arr[i]->no_starving_jobs || ninfo_arr[i]->temp_assign != NULL)
      {
      ++fit_nonfit;
      continue;
      }

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      if ((node_test = ninfo_arr[i]->can_fit_job_for_run(jinfo,spec,&scratch)) == CheckNonFit)
        {
        ++fit_nonfit;
        continue;
        }
      }
    else if (jinfo->cluster_mode == ClusterCreate)
      {
      if ((node_test = ninfo_arr[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) == CheckNonFit)
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
  CheckResult result = CheckAvailable;

  /* for each part of the nodespec, try to assign the requested amount of nodes */
  pars_spec_node *iter;
  iter = spec->nodes;
  while (iter != NULL)
    {
    for (unsigned i = 0; i < iter->node_count; i++)
      {
      int ret;
      if ((ret = assign_node(jinfo,iter,nodecount,ninfo_arr)) == 2)
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
    free_parsed_nodespec(spec);
    return REQUEST_NOT_MATCHED;
    }

  if (missed_nodes > 0 && result != CheckNonFit) /* some part of nodespec couldn't be assigned */
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
    assert(ninfo_arr[i] != NULL);

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
  assert(jinfo != NULL || ninfo != NULL);

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
