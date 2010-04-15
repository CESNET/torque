#ifndef FIFO_WORLD_H_
#define FIFO_WORLD_H_

#include "world_server.h"

typedef struct world world_t;

struct world 
{
  world_server_t* servers; /**< world servers */
  int count; /**< current number of world servers */
  int capacity; /**< alocated capacity */
};

/** global variable representing the world */
extern world_t cluster;

/** Initialize world information */
int world_init(world_t* w);

/** Clenup world information */
void world_free(world_t* w);

/** Either push a new server into world, or update info
*
* @returns NULL if alocation failed, updated/inserted world server
*/
world_server_t* world_add(world_t* w, server_info* info);

/** Check all servers for updates
* NOTE: currently doesn't do anything
*/
void world_check_update(world_t* w);

#endif

