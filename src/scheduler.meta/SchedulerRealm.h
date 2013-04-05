#ifndef WORLD_NG_H_
#define WORLD_NG_H_

#include <string>
#include <vector>
#include <map>

#include "data_types.h"

class World
  {
  public:
    /** \brief Initialize the world */
    World(int argc, char *argv[]);

    void run();


  private:
    bool fetch_servers();
    void cleanup_servers();
    void update_fairshare();
    void init_scheduling_cycle();
    void update_last_running();
    int try_run_job(job_info *jinfo);

    int p_master_connection;
    std::map<std::string,int> p_slave_connections;

    server_info *p_info;
    std::vector<prev_job_info> p_last_running;
  };

#endif /* WORLD_NG_H_ */
