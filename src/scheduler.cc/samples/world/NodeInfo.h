#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <vector>
#include <set>
#include <string>

typedef enum node_type { NodeTimeshared, NodeCluster, NodeVirtual, NodeCloud } node_type;
enum ResourceCheckMode { MaxOnly, Avail };

struct node_info
  {
unsigned is_down: 1; /* node is down */
unsigned is_free: 1;  /* node is free to run a job */
unsigned is_offline: 1; /* node is off-line */
unsigned is_unknown: 1; /* node is in an unknown state */
unsigned is_reserved: 1; /* node has been reserved */
unsigned is_exclusive: 1; /* node is running in job-exclusive mode */
unsigned is_sharing: 1; /* node is running in job-sharing mode */
unsigned is_busy: 1;  /* load on node is too high to schedule */


unsigned no_multinode_jobs: 1; /* no multinode jobs on this node */
unsigned no_starving_jobs:  1; /* no starving jobs no this node */

unsigned is_bootable: 1;
unsigned is_exclusively_assigned: 1;

unsigned is_usable_for_run  : 1; /* node is usable for running jobs */
unsigned is_usable_for_boot : 1; /* node is usable for booting jobs */
unsigned is_full            : 1; /* node is full (all slots used or exclusively assigned */

unsigned admin_slot_available : 1; /* admin slot is available */

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

  float max_load;  /* the load not to go over */
  float ideal_load;  /* the ideal load of the machine */
  char *arch;   /* machine architecture */
  int ncpus;   /* number of cpus */
  int physmem;   /* amount of physical memory in kilobytes */
  float loadave;  /* current load average */

  int np;       /* number of total virtual processors */
  int npfree;   /* number of free virtual processors */
  int npassigned; /* number of scheduled processors */

  job_info* starving_job; /* starving job */

  char *cluster_name;

  MagratheaState magrathea_status;
  struct repository_alternatives ** alternatives;

  pars_spec_node *temp_assign; /**< Temporary job assignment */
  repository_alternatives *temp_assign_alternative; /**< Alternative assignment */

  node_info* host; /*< the physical host of this node */
  std::vector< node_info* > hosted; /*< virtual nodes hosted on this node */

  bool has_prop(pars_prop* property, int preassign_starving, bool physical_only);
  bool has_prop(const char* property);
  };

#endif /* NODEINFO_H_ */
