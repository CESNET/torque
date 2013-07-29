#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <vector>
#include <set>
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
unsigned is_bootable: 1;
unsigned is_usable_for_run  : 1; /* node is usable for running jobs */
unsigned is_usable_for_boot : 1; /* node is usable for booting jobs */

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

  job_info* starving_job; /* starving job */

  char *cluster_name;

  MagratheaState magrathea_status;
  struct repository_alternatives ** alternatives;

  pars_spec_node *temp_assign; /**< Temporary job assignment */
  repository_alternatives *temp_assign_alternative; /**< Alternative assignment */
  ScratchType temp_assign_scratch;

  node_info* host; /*< the physical host of this node */
  std::vector< node_info* > hosted; /*< virtual nodes hosted on this node */

  std::string scratch_pool;

  bool has_prop(pars_prop* property, int preassign_starving, bool physical_only);
  bool has_prop(const char* property);

public:
  // CPU, admin slot, exclusively assigned

  /** \brief Check whether the job will fit on the node processors
   *
   * Check whether there are enough processors to match the specified job/nodespec to this node.
   * Also handles admin slots and exclusive requests.
   */
  CheckResult has_proc(job_info *job, pars_spec_node *spec);

  CheckResult has_spec(job_info *job, pars_spec_node *spec);
  CheckResult has_resc(job_info *job, pars_prop *resc);
  CheckResult has_mem(job_info *job, pars_spec_node *spec);
  CheckResult has_scratch(job_info *job, pars_spec_node *spec);

  void deplete_admin_slot();
  void deplete_exclusive_access();

  int get_proc_total() { return p_core_total; }
  void deplete_proc(int count) { p_core_assigned += count; }

  void set_proc_total(const char* value) { p_core_total = atoi(value); } // TODO add validity checking
  void set_proc_free(const char* value) { p_core_free = atoi(value); } // TODO add validity checking
  void set_admin_slot_enabled(const char* value) { p_admin_slot_enabled = !strcmp(value,"True"); }
  void set_admin_slot_avail(const char* value) { p_admin_slot_avail = !strcmp(value,"True"); }
  void set_exclusively_assigned(const char* value) { p_exclusively_assigned = !strcmp(value,"True"); }

private:
  // CPU related section
  int p_core_total;
  int p_core_free;
  int p_core_assigned;

  bool p_admin_slot_enabled;
  bool p_admin_slot_avail;

  bool p_exclusively_assigned;

public:
  node_info() : p_core_total(0), p_core_free(0), p_core_assigned(0), p_admin_slot_enabled(false), p_admin_slot_avail(false), p_exclusively_assigned(false) {}
  };

#endif /* NODEINFO_H_ */
