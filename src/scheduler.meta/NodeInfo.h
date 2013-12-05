#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdlib>
#include <cstring>

typedef enum node_type { NodeTimeshared, NodeCluster, NodeVirtual, NodeCloud } node_type;
enum ResourceCheckMode { MaxOnly, Avail };
enum CheckResult { CheckAvailable, CheckOccupied, CheckNonFit };

#include "NodeState.h"

struct node_info : public NodeState
  {
unsigned no_multinode_jobs: 1; /* no multinode jobs on this node */
unsigned no_starving_jobs:  1; /* no starving jobs no this node */

  long node_priority;

  node_type type; /**<type of the node (cluster,timeshared,virtual,cloud) */

  char *name;   /* name of the node */

  std::set<std::string> physical_properties;  /* the node properties */
  std::set<std::string> virtual_properties;   /* additional properties */

  char **jobs;   /* the jobs currently running on the node */
  char **big_status; /**< List of status strings */

  struct resource *res;  /* list of resources */

  server_info *server;  /* server that the node is associated with */
  char *queue; /**< queue the node is assigned to (attribute) */
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

  // fairshare
  double node_cost;
  double node_spec;

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

  CheckResult has_bootable_state() const;
  CheckResult has_runnable_state() const;

  CheckResult can_run_job(const job_info *jinfo) const;
  CheckResult can_boot_job(const job_info *jinfo) const;

  CheckResult can_fit_job_for_run(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult can_fit_job_for_boot(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch, repository_alternatives **alternative) const;

  void deplete_admin_slot();
  void deplete_exclusive_access();

  int get_proc_total() const { return p_core_total; }
  void deplete_proc(int count) { p_core_assigned += count; }
  void freeup_proc(int count) { p_core_assigned -= count; }

  unsigned long long get_mem_total() const;

  void set_proc_total(const char* value) { p_core_total = atoi(value); } // TODO add validity checking
  void set_proc_free(const char* value) { p_core_free = atoi(value); } // TODO add validity checking
  void set_admin_slot_enabled(const char* value) { p_admin_slot_enabled = !strcmp(value,"True"); }
  void set_admin_slot_avail(const char* value) { p_admin_slot_avail = !strcmp(value,"True"); }
  void set_exclusively_assigned(const char* value) { p_exclusively_assigned = !strcmp(value,"True"); }

  void fetch_bootable_alternatives();

  bool operator < (const node_info& right);

  void process_magrathea_status();
  void process_machine_cluster();

private:
  // CPU related section
  int p_core_total;
  int p_core_free;
  int p_core_assigned;

  bool p_admin_slot_enabled;
  bool p_admin_slot_avail;

  bool p_exclusively_assigned;

public:
  node_info() : node_cost(1.0), node_spec(10.0), p_core_total(0), p_core_free(0), p_core_assigned(0), p_admin_slot_enabled(false), p_admin_slot_avail(false), p_exclusively_assigned(false) {}
  };

#endif /* NODEINFO_H_ */
