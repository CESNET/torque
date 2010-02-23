#include "world.h"
#include <stdlib.h>
#include <string.h>

/** Find a server in the world using server_info
*  NOTE: Internal
*
* @returns NULL if not found, found world server otherwise
* @param w The cluster
* @param info Server information
*/
static world_server_t* world_find(world_t* w, server_info* info);

/** Push a new server into the world
* NOTE: Internal, does not check for duplicates
*
* @returns NULL if alocation failed, inserted world server otherwise
*/
static world_server_t* world_push(world_t* w, server_info* info);

world_t cluster = { 0, 0, 0 };

int world_init(world_t* w)
  {
  if (!w->capacity)
    {
    w->servers = (world_server_t*)malloc(sizeof(world_server_t)*10);
    if (!w->servers)
      return 1;

    w->count = 0;
    w->capacity = 10;
    }

  return 0;
  }

void world_free(world_t* w)
  {
  int i;
  for (i = 0; i < w->count; i++)
    world_server_free(&w->servers[i]);

  free(w->servers);

  w->count = 0;
  w->capacity = 0;
  }

static world_server_t* world_find(world_t* w, server_info* info)
  {
  int i;
  for (i = 0; i < w->count; i++)
    if (!strcmp(info->name,w->servers[i].info->name))
      return &w->servers[i];

    return 0;
  }

static world_server_t* world_push(world_t* w, server_info* info)
  {
  if (w->count >= w->capacity)
    {
    world_server_t* tmp;
    if ((tmp = (world_server_t*)realloc(w->servers, (w->capacity*3)/2)) == 0)
      return 0;

    w->capacity = (w->capacity*3)/2;
    w->servers = tmp;
    }

  world_server_init(&w->servers[w->count]);
  world_server_update_info(&w->servers[w->count],info);
  return &w->servers[w->count++];
  }

world_server_t* world_add(world_t* w, server_info* info)
  {
  world_server_t* server = 0;
  if ((server = world_find(w,info)) != 0)
    { /* already know this one */
    return world_server_update_info(server,info);
    }
  else
    { /* a new server */
    return world_push(w,info);
    }
  }

void world_check_update(world_t* w)
  {
  int i;

  for (i = 0; i < w->count; i++)
    world_server_fetch_info(&w->servers[i]);

  }

