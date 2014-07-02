#ifndef NODEFILTERS_H_
#define NODEFILTERS_H_

#include <vector>
#include "data_types.h"
#include "LogicFwd.h"

namespace Scheduler {
namespace Logic {

/** \brief Filter nodes usable for a job
 *
 * General test for initial filter of nodes that can fit a job.
 * Currently only filters out bad matches of physical/virtual machines vs. normal/cloud jobs
 */
struct NodeSuitableForJob
  {
  NodeSuitableForJob(const job_info *jinfo);
  bool operator ()(const node_info* node) const;

  static void filter(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo);

  private:
    const job_info *p_jinfo;
  };

/** \brief Filter nodes usable for a node specification
 *
 * General test for whether a node is suitable for the presented nodespec.
 * Supports testing for assignment, rebooting, starving and fairshare calculation.
 */
struct NodeSuitableForSpec
  {
  NodeSuitableForSpec(const job_info *jinfo, const pars_spec_node *spec, SuitableNodeFilterMode mode);
  bool operator()(const node_info* node) const;

  /** \brief Filter for purposes of fairshare */
  static void filter_fairshare(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec);
  /** \brief Filter for purposes of starvation */
  static void filter_starving(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec);
  /** \brief Filter for purposes of scheduling */
  static void filter_assign(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec);
  /** \brief Filter for purposes of rebooting nodes */
  static void filter_reboot(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo, const pars_spec_node* spec);

  private:
    const job_info *p_jinfo;
    const pars_spec_node *p_spec;
    SuitableNodeFilterMode p_mode;
  };

/** \brief Filter nodes depending on a particular "placement" resource
 *
 * Filter out nodes that are capable of providing a particular resource=value combination.
 * IDs are routed through the PropRegistry
 */
struct NodeSuitableForPlace
  {
  NodeSuitableForPlace(size_t place_id, size_t value_id);
  bool operator()(const node_info* node) const;

  static void filter(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, size_t place_id, size_t value_id);

  private:
    const size_t p_place;
    const size_t p_value;
  };

}}

#endif /* NODEFILTERS_H_ */
