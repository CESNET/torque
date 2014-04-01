#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdlib>
#include <cstring>


enum ResourceCheckMode { MaxOnly, Avail };
enum CheckResult { CheckAvailable, CheckOccupied, CheckNonFit };

#include "base/NodeData.h"
using namespace Scheduler;
using namespace Base;
#include "JobInfo.h"

struct assigned_nodespec
  {
  pars_spec_node *temp_assign; /**< Temporary job assignment */
  repository_alternatives *temp_assign_alternative; /**< Alternative assignment */
  ScratchType temp_assign_scratch;
  bool temp_fairshare_used;
  };

struct node_info : public NodeData // change to protected inheritance
  {
  bool p_is_notusable;

  /// Test not-usable state
  bool is_notusable() const { return p_is_notusable; }

  /// Mark state as not usable
  void set_notusable() { p_is_notusable = true; }

  bool is_rebootable;
  char *name;   /* name of the node */
  char **jobs;   /* the jobs currently running on the node */

  server_info *server;  /* server that the node is associated with */
  queue_info *excl_queue; /**< pointer to queue the node is exclusive to */

  char *cluster_name;

  MagratheaState magrathea_status;
  struct repository_alternatives ** alternatives;

  bool is_building_cluster;
  std::string virtual_cluster;
  std::string virtual_image;

  pars_spec_node *temp_assign; /**< Temporary job assignment */
  repository_alternatives *temp_assign_alternative; /**< Alternative assignment */
  ScratchType temp_assign_scratch;
  bool temp_fairshare_used;

  node_info* host; /*< the physical host of this node */
  std::vector< node_info* > hosted; /*< virtual nodes hosted on this node */

  pars_spec_node *starving_spec;

  std::string scratch_pool;

  CheckResult has_prop(const pars_prop* property, bool physical_only) const;
  bool has_prop(const char* property) const;

public:
  // CPU, admin slot, exclusively assigned

  /** \brief Check whether the job will fit on the node processors
   *
   * Check whether there are enough processors to match the specified job/nodespec to this node.
   * Also handles admin slots and exclusive requests.
   */
  CheckResult has_proc(const job_info *job, const pars_spec_node *spec) const;
  CheckResult has_mem(const job_info *job, const pars_spec_node *spec) const;
  CheckResult has_scratch(const job_info *job, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult has_spec(const job_info *job, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult has_resc(const pars_prop *prop) const;

  CheckResult has_props_boot(const job_info *job, const pars_spec_node *spec, const repository_alternatives *virt_conf) const;
  CheckResult has_props_run(const job_info *job, const pars_spec_node *spec) const;

  CheckResult has_bootable_state(enum ClusterMode mode) const;
  CheckResult has_runnable_state() const;

  CheckResult can_run_job(const job_info *jinfo) const;
  CheckResult can_boot_job(const job_info *jinfo) const;

  CheckResult can_fit_job_for_run(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult can_fit_job_for_boot(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch, repository_alternatives **alternative) const;

  void deplete_admin_slot();
  void deplete_exclusive_access();

  void deplete_proc(int count) { p_core_assigned += count; }
  void freeup_proc(int count) { p_core_assigned -= count; }

  unsigned long long get_mem_total() const;
  void set_exclusively_assigned(const char* value) { p_exclusively_assigned = !strcmp(value,"True"); }

  void fetch_bootable_alternatives();

  bool operator < (const node_info& right);

  void process_magrathea_status();
  void process_machine_cluster();

private:
  // CPU related section
  int p_core_assigned;
  bool p_exclusively_assigned;

public:
  node_info() : p_is_notusable(false), is_rebootable(false), p_core_assigned(0), p_exclusively_assigned(false) {}
  };

#endif /* NODEINFO_H_ */
