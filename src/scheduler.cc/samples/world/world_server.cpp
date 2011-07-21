#include "world_server.h"
#include "server_info.h"
#include "prev_job_info.h"

void world_server_init(world_server_t* server)
  {
  server->info = (server_info*)0;
  server->update_func = 0;

  server->last_running = 0;
  server->last_running_size = 0;
  server->is_down = 1;
  }

void world_server_free(world_server_t* server)
  {
  if (server->info)
    free_server(server->info,1);
  server->update_func = 0;
  server->info = 0;

  if (server->last_running_size)
    free_pjobs(server->last_running,server->last_running_size);

  server->last_running = 0;
  server->last_running_size = 0;
  server->is_down = 1;
  }

world_server_t* world_server_update_info(world_server_t* server,
                                         server_info* info)
  {
  if (server->info)
    free_server(server->info,1);
  server->info = info;
  server->is_down = 0;
  return server;
  }

int world_server_fetch_info(world_server_t* server)
  {
  if (server->update_func)
    {
    return (*server->update_func)(server);
    }
  else
    return 0;
  }
