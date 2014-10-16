#include "JobInfo.h"
#include "misc.h"
#include "data_types.h"
#include "nodespec_sch.h"
#include "logic/NodeFilters.h"
#include "NodeSort.h"
#include "RescInfoDb.h"

#include "base/MiscHelpers.h"
using namespace Scheduler;
using namespace Base;

#include <algorithm>
#include <limits>
using namespace std;

void job_info::unplan_from_node(node_info* ninfo, pars_spec_node* spec)
  {
  Resource *res;

  if (is_exclusive)
    ninfo->freeup_proc(ninfo->get_cores_total());
  else
    ninfo->freeup_proc(spec->procs);

  /* mem */
  if ((res = ninfo->get_resource("mem")) != NULL)
    res->freeup_resource(spec->mem);

  if (ninfo->get_scratch_assign() == ScratchLocal)
    {
    if ((res = ninfo->get_resource("scratch_local")) != NULL)
      res->freeup_resource(spec->scratch);
    }

  if (ninfo->get_scratch_assign() == ScratchSSD)
    {
    if ((res = ninfo->get_resource("scratch_ssd")) != NULL)
      res->freeup_resource(spec->scratch);
    }

  if (ninfo->get_scratch_assign() == ScratchShared)
    {
    if ((res = ninfo->get_resource("scratch_pool")) != NULL)
      {
      string pool = res->get_str_val();
      map<string,DynamicResource>::iterator i;
      i = ninfo->get_parent_server()->dynamic_resources.find(pool);
      if (i != ninfo->get_parent_server()->dynamic_resources.end())
        {
        i->second.remove_scheduled(spec->scratch);
        }
      }
    }

  /* the rest */
  pars_prop *iter = spec->properties;
  while (iter != NULL)
    {
    if (iter->value == NULL)
      { iter = iter->next; continue; }

    if (res_check_type(iter->name) != ResCheckNone)
      {
      if ((res = ninfo->get_resource(iter->name)) != NULL)
        res->freeup_resource(iter->value);
      }

    iter = iter->next;
    }
  }

void job_info::plan_on_node(node_info* ninfo, pars_spec_node* spec)
  {
  Resource *res;

  if (this->cluster_mode == ClusterCreate)
    {
    for (size_t i = 0; i < ninfo->get_host()->get_hosted().size(); i++)
      {
      ninfo->get_host()->get_hosted()[i]->set_notusable();
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->get_name(),
                "Node marked as incapable of running and booting jobs, because it, or it's sister is servicing a cluster job.");
      }
    }

  if (this->is_exclusive)
    ninfo->deplete_proc(ninfo->get_cores_total());
  else
    ninfo->deplete_proc(spec->procs);

  /* mem */
  if ((res = ninfo->get_resource("mem")) != NULL)
    res->consume_resource(spec->mem);

  if (ninfo->get_scratch_assign() == ScratchLocal)
    {
    if ((res = ninfo->get_resource("scratch_local")) != NULL)
      res->consume_resource(spec->scratch);
    }

  if (ninfo->get_scratch_assign() == ScratchSSD)
    {
    if ((res = ninfo->get_resource("scratch_ssd")) != NULL)
      res->consume_resource(spec->scratch);
    }

  if (ninfo->get_scratch_assign() == ScratchShared)
    {
    if ((res = ninfo->get_resource("scratch_pool")) != NULL)
      {
      string pool = res->get_str_val();
      map<string,DynamicResource>::iterator i;
      i = ninfo->get_parent_server()->dynamic_resources.find(pool);
      if (i != ninfo->get_parent_server()->dynamic_resources.end())
        {
        i->second.add_scheduled(spec->scratch);
        }
      }
    }

  /* the rest */
  pars_prop *iter = spec->properties;
  while (iter != NULL)
    {
    if (iter->value == NULL)
      { iter = iter->next; continue; }

    if (res_check_type(iter->name) != ResCheckNone)
      {
      if ((res = ninfo->get_resource(iter->name)) != NULL)
        res->consume_resource(iter->value);
      }

    iter = iter->next;
    }
  }

void job_info::plan_on_queue(queue_info* qinfo)
  {
  }

void job_info::plan_on_server(server_info* sinfo)
  {
  /* count dynamic resources */
  resource_req *req = resreq;
  while (req != NULL)
    {
    map<string,DynamicResource>::iterator it = sinfo->dynamic_resources.find(string(req->name));
    if (it != sinfo->dynamic_resources.end())
      {
      it->second.add_assigned(req->amount);
      }

    req = req->next;
    }
  }

int job_info::preprocess()
  {
  const char* processed_nodespec;
  if (this->nodespec == NULL || this->nodespec[0] == '\0')
    processed_nodespec = "1:ppn=1";
  else
    processed_nodespec = this->nodespec;

  if ((this->parsed_nodespec = parse_nodespec(processed_nodespec)) == NULL)
    return SCHD_ERROR;

  /* setup some side values, that need parsed nodespec to be determined */
  this->is_exclusive = this->parsed_nodespec->is_exclusive;
  this->is_multinode = (this->parsed_nodespec->total_nodes > 1)?1:0;

  return SUCCESS;
  }

double job_info::calculate_fairshare_cost(const vector<node_info*>& nodes) const
  {
  double fairshare_cost = 0;

  pars_spec_node *iter = this->parsed_nodespec->nodes;
  while (iter != NULL)
    {
    vector<node_info*> fairshare_nodes; // construct possible nodes
    NodeSuitableForSpec::filter_fairshare(nodes,fairshare_nodes,this,iter);
    sort(fairshare_nodes.begin(),fairshare_nodes.end(),NodeCostSort(iter->procs,iter->mem,this->is_exclusive));

    unsigned i = 0;
    for (unsigned count = 0; count < iter->node_count; count++)
      {
      while (fairshare_nodes[i]->has_fairshare_flag())
        i++;

      unsigned long long node_procs = fairshare_nodes[i]->get_cores_total();
      unsigned long long node_mem   = fairshare_nodes[i]->get_mem_total();

      if (this->is_exclusive)
        fairshare_cost += node_procs*fairshare_nodes[i]->get_node_cost();
      else
        fairshare_cost += max(static_cast<double>(iter->mem)/node_mem,static_cast<double>(iter->procs)/node_procs)*node_procs*fairshare_nodes[i]->get_node_cost();

      fairshare_nodes[i]->set_fairshare_flag();
      }
    iter = iter->next;
    }

  return fairshare_cost;
  }

long job_info::get_walltime() const
  {
  resource_req *resc = find_resource_req(this->resreq,"walltime");
  if (resc == NULL)
    return 0;

  return resc->amount;
  }

time_t job_info::completion_time()
  {
  resource_req *req = find_resource_req(this->resreq,"walltime");
  if (req == NULL)
    return numeric_limits<time_t>::max();

  return stime + req->amount;
  }
