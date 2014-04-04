#ifndef WORLD_NG_H_
#define WORLD_NG_H_

#include <string>
#include <vector>
#include <map>

#include "data_types.h"
#include "ConnectionMgr.h"

class World
  {
  public:
    /** \brief Initialize the world */
    World(int argc, char *argv[]);
    /** \brief Cleanup */
    ~World();

    void run();


  private:
    bool fetch_servers();
    void cleanup_servers();
    void update_fairshare();
    void init_scheduling_cycle();
    void update_last_running();
    int try_run_job(job_info *jinfo);

    ConnectionMgr p_connections;

    server_info *p_info;
    std::vector<prev_job_info> p_last_running;
  };

#endif /* WORLD_NG_H_ */
