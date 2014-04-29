#ifndef SERVERINFO_H_
#define SERVERINFO_H_

#include "data_types.h"
#include <map>
#include <set>
#include <string>
#include "DynamicResource.h"

#define DEFAULT_JOB_START_TIMEOUT 60

struct server_info
  {
  char *name;   /* name of server */

  struct resource *res;  /* list of resources */
  std::map<std::string, DynamicResource> dynamic_resources; /* list of dynamic resources */
  char *default_queue;  /* the default queue atribute of the server */
  int max_run;   /* max jobs that can be run at one time */
  int max_user_run;  /* max jobs a user can run at one time */
  int max_group_run;  /* max jobs a group can run at one time */
  int num_queues;  /* number of queues that reside on the server */
  int num_nodes;  /* number of nodes associated with the server */
  state_count sc;  /* number of jobs in each state */
  queue_info **queues;  /* array of queues */
  job_info **jobs;  /* array of jobs on the server */
  job_info **running_jobs; /* array of jobs in the running state */

  node_info **nodes;  /* array of nodes associated with the server */
  node_info **non_dedicated_nodes; /* array of nodes, not exclusively assigned anywhere */
  int non_dedicated_node_count;

  token **tokens;               /* array of tokens */

  int max_installing_nodes;
  int installing_node_count;

  void recount_installing_nodes();
  bool installing_nodes_overlimit();

  int job_start_timeout;

  std::map<std::string,int> exec_count; /* executed jobs this cycle per-user */
  };

#endif /* SERVERINFO_H_ */
