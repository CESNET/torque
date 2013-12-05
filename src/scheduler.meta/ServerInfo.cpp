#include "ServerInfo.h"

void server_info::recount_installing_nodes()
  {
  this->installing_node_count = 0;

  for (int i=0; i<this->num_nodes; i++)
    {
    if (this->nodes[i]->is_building_cluster)
      ++(this->installing_node_count);
    }
  }

bool server_info::installing_nodes_overlimit()
  {
  if (this->max_installing_nodes == INFINITY)
    return false;

  return this->installing_node_count >= this->max_installing_nodes;
  }
