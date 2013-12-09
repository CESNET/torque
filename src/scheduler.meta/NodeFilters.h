#ifndef NODEFILTERS_H_
#define NODEFILTERS_H_

#include <vector>
#include "data_types.h"

/** \brief Filter nodes usable for a job */
struct NodeSuitableForJob
  {
  NodeSuitableForJob(const job_info *jinfo);
  bool operator ()(const node_info* node) const;

  static void filter(const std::vector<node_info*>& nodes, std::vector<node_info*>& result, const job_info* jinfo);

  private:
    const job_info *p_jinfo;
  };

enum SuitableNodeFilterMode { SuitableAssignMode, SuitableFairshareMode, SuitableStarvingMode, SuitableRebootMode };


/** \brief Filter nodes usable for a node specification */
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

#endif /* NODEFILTERS_H_ */
