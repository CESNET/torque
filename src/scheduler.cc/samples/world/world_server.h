#ifndef FIFO_WORLD_SERVER_H_
#define FIFO_WORLD_SERVER_H_

#include "data_types.h"

typedef struct world_server world_server_t;

struct world_server
  {
  server_info* info;
  int (*update_func)(world_server_t*);

  prev_job_info *last_running;
  int last_running_size;
  unsigned int is_down :1;
  };

/** Initialize world server */
void world_server_init(world_server_t* server);

/** Clenup world server
* NOTE: cleans up the server_info struct
*/
void world_server_free(world_server_t* server);

/** Update the server information from a server_info struct */
world_server_t* world_server_update_info(world_server_t* server, 
                                         server_info* info);

/** Fetch server info using update func */
int world_server_fetch_info(world_server_t* server);

#endif
