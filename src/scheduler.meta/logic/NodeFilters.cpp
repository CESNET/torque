#include "NodeFilters.h"
using namespace std;

namespace Scheduler {
namespace Logic {

NodeSuitableForJob::NodeSuitableForJob(const job_info *jinfo) : p_jinfo(jinfo) {}

bool NodeSuitableForJob::operator ()(const node_info* node) const
  {
  if (p_jinfo->cluster_mode == ClusterUse && node->can_run_job(p_jinfo) == CheckNonFit)
    return false;
  if (p_jinfo->cluster_mode == ClusterCreate && node->can_boot_job(p_jinfo) == CheckNonFit)
    return false;
  if (node->can_run_job(p_jinfo) == CheckNonFit && node->can_boot_job(p_jinfo) == CheckNonFit)
    return false;
  return true;
  }

NodeSuitableForSpec::NodeSuitableForSpec(const job_info *jinfo, const pars_spec_node *spec, SuitableNodeFilterMode mode) : p_jinfo(jinfo), p_spec(spec), p_mode(mode) {}

bool NodeSuitableForSpec::operator()(const node_info* node) const
  {
  ScratchType scratch = ScratchNone;
  repository_alternatives *ra;

  if (p_mode == SuitableRebootMode && (node->has_assignment() || node->physical_node()->has_virtual_assignment()))
    return false;
  if (p_mode == SuitableAssignMode && (node->has_assignment() || node->physical_node()->has_virtual_assignment()))
    return false;
  if (p_mode == SuitableStarvingMode && node->get_nostarving())
    return false;
  if (p_mode == SuitableFairshareMode && node->has_fairshare_flag())
    return false;

  if (p_jinfo->cluster_mode == ClusterUse && node->can_fit_job_for_run(p_jinfo,p_spec,&scratch) == CheckNonFit)
    return false;
  if (p_jinfo->cluster_mode == ClusterCreate && node->can_fit_job_for_boot(p_jinfo,p_spec,&scratch,&ra) == CheckNonFit)
    return false;

  if (node->can_fit_job_for_run(p_jinfo,p_spec,&scratch) == CheckNonFit && node->can_fit_job_for_boot(p_jinfo,p_spec,&scratch,&ra) == CheckNonFit)
    return false;

  return true;
  }

namespace {
template < typename T >
void filter_nodes(const vector<node_info*>& nodes, vector<node_info*>& output, const T &filter)
  {
  output.reserve(nodes.size());

  for (size_t i = 0; i < nodes.size(); ++i)
    if (filter(nodes[i]))
      output.push_back(nodes[i]);
  }
}

void NodeSuitableForSpec::filter_fairshare(const vector<node_info*>& nodes, vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec)
  {
  /*
  back_insert_iterator<vector<node_info*>> it(result);
  copy_if(nodes.begin(),nodes.end(),it,[jinfo](node_info* node) {
    if (node->has_fairshare_flag())
      return false;

    if (jinfo->cluster_mode == ClusterUse && node->can_fit_job_for_run(p_jinfo,p_spec,&scratch) == CheckNonFit)
      return false;
    if (p_jinfo->cluster_mode == ClusterCreate && node->can_fit_job_for_boot(p_jinfo,p_spec,&scratch,&ra) == CheckNonFit)
      return false;
    if (node->can_fit_job_for_run(jinfo,p_spec,&scratch) == CheckNonFit && node->can_fit_job_for_boot(jinfo,p_spec,&scratch,&ra) == CheckNonFit)
      return false;
    return true;
  });*/

  filter_nodes(nodes,result,NodeSuitableForSpec(jinfo,spec,SuitableFairshareMode));
  }

void NodeSuitableForSpec::filter_starving(const vector<node_info*>& nodes, vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec)
  {
  filter_nodes(nodes,result,NodeSuitableForSpec(jinfo,spec,SuitableStarvingMode));
  }

void NodeSuitableForSpec::filter_assign(const vector<node_info*>& nodes, vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec)
  {
  filter_nodes(nodes,result,NodeSuitableForSpec(jinfo,spec,SuitableAssignMode));
  }

void NodeSuitableForSpec::filter_reboot(const vector<node_info*>& nodes, vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec)
  {
  filter_nodes(nodes,result,NodeSuitableForSpec(jinfo,spec,SuitableRebootMode));
  }

void NodeSuitableForJob::filter(const vector<node_info*>& nodes, vector<node_info*>& result, const job_info* jinfo)
  {
  filter_nodes(nodes,result,NodeSuitableForJob(jinfo));
  }

}}
