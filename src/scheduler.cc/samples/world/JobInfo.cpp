#include "JobInfo.h"
#include "misc.h"
#include "data_types.h"
#include "nodespec_sch.h"

using namespace std;

bool job_info::on_host(node_info *ninfo)
  {
  if (ninfo->host == NULL)
    return on_node(ninfo);

  for (size_t i = 0; i < ninfo->host->hosted.size(); i++)
    {
    if (on_node(ninfo->host->hosted[i]))
      return true;
    }

  return false;
  }

bool job_info::on_node(node_info *ninfo)
  {
  return (schedule.find(string(ninfo->name)) != schedule.end());
  }

void job_info::plan_on_node(node_info* ninfo, pars_spec_node* spec)
  {
  resource *res;

  if (this->cluster_mode == ClusterCreate)
    {
    for (size_t i = 0; i < ninfo->host->hosted.size(); i++)
      {
      ninfo->host->hosted[i]->is_usable_for_run = 0;
      ninfo->host->hosted[i]->is_usable_for_boot = 0;
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo->name,
                "Node marked as incapable of running and booting jobs, because it, or it's sister is servicing a cluster job.");
      }
    }

  if (is_exclusive)
    ninfo->npassigned += ninfo->np;
  else
    ninfo->npassigned += spec->procs;

  /* mem */
  res = find_resource(ninfo->res,"mem");
  if (res != NULL)
    res->assigned += spec->mem;

  /* vmem */
  res = find_resource(ninfo->res,"vmem");
  if (res != NULL)
    res->assigned += spec->vmem;

  /* the rest */
  pars_prop *iter = spec->properties;
  while (iter != NULL)
    {
    if (iter->value == NULL)
      { iter = iter->next; continue; }

    if (res_check_type(iter->name) != ResCheckNone)
      {
      res = find_resource(ninfo->res,iter->name);
      if (res != NULL)
        {
        res->assigned += res_to_num(iter->value);
        }
      }

    iter = iter->next;
    }
  }

void job_info::plan_on_queue(queue_info* qinfo)
  {
  resource_req *req;  /* used to cycle through resources to update */
  resource *res;  /* used in finding a resource to update */

  /* count total used resources */
  req = resreq;
  while (req != NULL)
    {
    res = find_resource(qinfo->qres, req->name);
    if (res)
      res->assigned += req->amount;

    req = req->next;
    }
  }

void job_info::plan_on_server(server_info* sinfo)
  {
  resource_req *req;  /* used to cycle through resources to update */
  resource *res;  /* used in finding a resource to update */

  /* count total used resources */
  req = resreq;
  while (req != NULL)
    {
    res = find_resource(sinfo->res, req->name);
    if (res)
      res->assigned += req->amount;

    req = req->next;
    }

  /* count dynamic resources */
  req = resreq;
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

