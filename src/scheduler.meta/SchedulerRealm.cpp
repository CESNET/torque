#include "SchedulerRealm.h"
#include "fifo.h"
#include "misc.h"
#include "globals.h"
#include "fairshare.h"
#include "sort.h"
#include "prev_job_info.h"
#include "check.h"
#include "node_info.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <cstdlib>
using namespace std;

extern "C" {
#include "dis.h"
}

extern void dump_current_fairshare(group_info *root);

World::World(int argc, char *argv[])
  {
  /* init the scheduler (configuration and stuff) */
  if (schedinit(argc, argv))
    {
    sched_log(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, "world_init", "World initialization failed, terminating.");
    exit(1);
    }

  /* verify configuration and current state of the system */
  }

job_info ** merge_job_arrays(job_info **arr1, job_info **arr2)
  {
  int size1 = 0, size2 = 0;
  if (arr1 != NULL)
    for (size1 = 0; arr1[size1] != NULL; size1++);
  if (arr2 != NULL)
    for (size2 = 0; arr2[size2] != NULL; size2++);

  job_info ** result = (job_info**)malloc(sizeof(job_info*)*(size1+size2+1));

  int index = 0;
  if (arr1 != NULL)
  for (size1 = 0; arr1[size1] != NULL; size1++)
    {
    result[index] = arr1[size1];
    index++;
    }

  if (arr2 != NULL)
  for (size2 = 0; arr2[size2] != NULL; size2++)
    {
    result[index] = arr2[size2];
    index++;
    }

  result[index] = NULL;

  return result;
  }

bool World::fetch_servers()
  {
  try
    {
    if ((p_info = query_server(p_connections.make_master_connection(string(conf.local_server)))) == NULL)
      {
      sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "Couldn't fetch information from server \"%s\".",conf.local_server);
      return false;
      }
    }
  // catch deep errors
  catch (const exception& e)
    {
    sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, e.what());
    return false;
    }

  /* refresh slave servers connections */
  int i = 0;
  while (conf.slave_servers[i][0] != '\0')
    {
    for (int j = 0; j < p_info->num_queues; ++j)
      {
      if (p_info->queues[j]->is_global)
        {
        job_info **jobs;
        try
          {
          if ((jobs = query_jobs(p_connections.make_remote_connection(string(conf.slave_servers[i])),p_info->queues[j])) == NULL)
            {
            sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "Couldn't fetch information from server \"%s\".",conf.slave_servers[i]);
            i++;
            continue;
            }
          }
        // catch deep errors
        catch (const exception& e)
          {
          sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, e.what());
          i++;
          continue;
          }

        // merge the new jobs into the local queue
        if (jobs[0] != NULL)
          {
          job_info **merged = merge_job_arrays(p_info->queues[j]->jobs,jobs);
          init_state_count(&p_info->queues[j]->sc);
          count_states(merged,&p_info->queues[j]->sc);

          free(p_info->queues[j]->jobs);
          p_info->queues[j]->jobs = merged;

          free(p_info->queues[j]->running_jobs);
          p_info->queues[j]->running_jobs = job_filter(p_info->queues[j]->jobs, p_info->queues[j]->sc.total, check_run_job, NULL);

          merged = merge_job_arrays(p_info->jobs,jobs);
          init_state_count(&p_info->sc);
          count_states(merged,&p_info->sc);

          free(p_info->jobs);
          p_info->jobs = merged;

          free(p_info->running_jobs);
          p_info->running_jobs = job_filter(p_info->jobs, p_info->sc.total, check_run_job, NULL);
          }
        }
      }
    ++i;
    }

  return true;
  }

void World::cleanup_servers()
  {
  p_connections.disconnect_all();
  free_server(p_info,1);
  }


// TODO move to class
extern time_t last_sync;

void World::update_fairshare()
  {
  if (!cstat.fair_share)
    return;

  if (p_last_running.size() > 0)
    {
    /* add the usage which was accumulated between the last cycle and this
     * one and calculate a new value
     */
    for (size_t i = 0; i < p_last_running.size(); i++)
      {
      job_info** jobs = p_info->jobs;
      group_info *user = p_last_running[i].ginfo;

      int j;
      for (j = 0; jobs[j] != NULL; j++)
        {
        if (jobs[j] -> state == JobCompleted || jobs[j] -> state == JobExiting || jobs[j] -> state == JobRunning)
          if (!strcmp(p_last_running[i].name, jobs[j] -> name))
            break;
        }

      if (jobs[j] != NULL)
        {
        resource_req *tmp = find_resource_req(jobs[j]->resreq, "procs");
        user -> usage += (calculate_usage_value(jobs[j] -> resused) -
                         calculate_usage_value(p_last_running[i].resused))*tmp->amount;
        }
      }

    /* assign usage into temp usage since temp usage is used for usage
     * calculations.  Temp usage starts at usage and can be modified later.
     */
    for (size_t i = 0; i < p_last_running.size(); i++)
      p_last_running[i].ginfo -> temp_usage = p_last_running[i].ginfo -> usage;
    }

  time_t t = cstat.current_time;

  time_t last_decay;

  fstream decay_file;
  decay_file.open(LAST_DECAY_FILE,ios::in);
  if (decay_file.is_open())
    {
    decay_file >> last_decay;
    decay_file.close();
    }
  else
    {
    last_decay = cstat.current_time;
    }

  bool decayed = false;
  while (t - last_decay > conf.half_life)
    {
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "", "Decaying Fairshare Tree");
    decay_fairshare_tree(conf.group_root);
    t -= conf.half_life;
    decayed = true;
    }

  if (decayed)
    {
    /* set the time to the acuall the half-life should have occured */
    last_decay = cstat.current_time -
                 (cstat.current_time - last_decay) % conf.half_life;

    decay_file.open(LAST_DECAY_FILE,ios::out | ios::trunc);
    decay_file << last_decay << endl;
    decay_file.close();
    }

  if (cstat.current_time - last_sync > conf.sync_time)
    {
    write_usage();
    last_sync = cstat.current_time;
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "", "Usage Sync");
    }
  }

void World::init_scheduling_cycle()
  {
  update_fairshare();

  /* sort queues by priority if requested */
  if (cstat.sort_queues)
    qsort(p_info -> queues, p_info -> num_queues, sizeof(queue_info *), cmp_queue_prio_dsc);

  if (cstat.sort_by[0].sort != NO_SORT)
    {
    if (cstat.by_queue || cstat.round_robin)
      {
      for (int i = 0; i < p_info -> num_queues; i++)
        {
        qsort(p_info->queues[i]->jobs, p_info->queues[i]->sc.total, sizeof(job_info *), cmp_sort);
        }
      }
    else
      qsort(p_info -> jobs, p_info -> sc.total, sizeof(job_info *), cmp_sort);
    }

  next_job(p_info, INITIALIZE);
  }

void World::update_last_running()
  {
  for (size_t i = 0; i < p_last_running.size(); i++)
    {
    free_prev_job_info(&p_last_running[i]);
    }

  p_last_running.clear();
  p_last_running.resize(p_info->sc.running);

  for (int i = 0; p_info->running_jobs[i] != NULL; i++)
    {
    p_last_running[i].name = strdup(p_info->running_jobs[i]->name);
    p_last_running[i].resused = clone_resource_req_list(p_info->running_jobs[i]->resused);
    p_last_running[i].account = strdup(p_info->running_jobs[i]->account);
    p_last_running[i].ginfo = p_info->running_jobs[i]->ginfo;
    }
  }

int World::try_run_job(job_info *jinfo)
  {
  int booting = 0;
  char *best_node_name = NULL;
  best_node_name = nodes_preassign_string(jinfo, p_info->nodes, p_info->num_nodes, &booting);

  int ret = 0;

  bool remote = false;
  int socket;
  if (string(jinfo->name).find(conf.local_server) == string::npos)
    {
    remote = true;
    string jobid = string(jinfo->name);
    size_t firstdot = jobid.find('.');
    string server_name = jobid.substr(firstdot+1);

    socket = p_connections.get_connection(server_name);
    }
  else
    {
    socket = p_connections.get_master_connection();
    }

  if (!booting)
    {
    if (remote)
      {
      string destination = string(jinfo->queue->name)+string("@")+string(conf.local_server);

      DIS_tcp_settimeout(p_info->job_start_timeout*2+30); /* move + run, double the tolerance */

      ret = pbs_movejob(socket, jinfo->name, (char*)destination.c_str(), best_node_name);
      }
    else
      {
      DIS_tcp_settimeout(p_info->job_start_timeout+30); /* add a generous 30 seconds communication overhead */

      ret = pbs_runjob(socket, jinfo->name, best_node_name, NULL);
      }
    }
  else
    {
    if (!remote)
      update_job_comment(socket, jinfo, COMMENT_NODE_STILL_BOOTING);
    ret = 0;
    }

  if (ret == 0)
    {
    if (!booting)
      {
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, jinfo -> name, "Job Run.");
      }
    else
      {
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, jinfo -> name, "Job Waiting for booting node.");
      }

    update_server_on_run(p_info, jinfo);
    update_queue_on_run(jinfo->queue, jinfo);
    update_job_on_run(socket, jinfo);

    if (!booting && cstat.fair_share)
      update_usage_on_run(jinfo);

    free(p_info -> running_jobs);
    p_info -> running_jobs = job_filter(p_info -> jobs, p_info -> sc.total,
                                       check_run_job, NULL);

    free(jinfo->queue-> running_jobs);
    jinfo->queue-> running_jobs = job_filter(jinfo->queue-> jobs, jinfo->queue -> sc.total,
                                       check_run_job, NULL);
    }
  else
    {
    if (ret == PBSE_PROTOCOL || ret == PBSE_TIMEOUT)
      {
      log_err(ret,"pbs_runjob","Protocol problem while communicating with the server.");
      return ret;
      }

    if (!remote)
      {
      //char * errmsg = pbs_geterrmsg(socket);
      //snprintf(buf, BUF_SIZE, "Not Running - PBS Error: %s", errmsg);
      //update_job_comment(pbs_sd, jinfo, buf);
      }
    }

  nodes_preassign_clean(p_info->nodes, p_info->num_nodes);

  return ret;
  }

extern bool scheduler_not_dying;

void World::run()
  {
  const unsigned int sleep_suspend = 2;

  try {
  while (scheduler_not_dying)
    {
    sched_log(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "Suspending scheduler for %d seconds.",sleep_suspend);
    sleep(sleep_suspend);
    sched_log(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "Scheduler woken up, initializing scheduling cycle.");

    update_cycle_status();

    /* create the server / queue / job / node structures */
    if (!fetch_servers()) { continue; }

    init_scheduling_cycle();

    time_t cycle_start = time(NULL);

    int run_errors = 0;

    /* main scheduling loop */
    job_info *jinfo;
    while ((run_errors <= 3) && (jinfo = next_job(p_info, 0)))
      {
      if (difftime(time(NULL),cycle_start) > conf.max_cycle)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "cycle", "Cycle ending prematurely due to time limit.");
        break;
        }

      JobLog log(string(jinfo->name));

      int ret;
      if ((ret = is_ok_to_run_job(log, p_info, jinfo->queue, jinfo, 0)) == SUCCESS)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Trying to execute job.");

        /* split local vs. remote */
        if (try_run_job(jinfo) != 0)
          {
          jinfo->can_not_run = 1;
          sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_JOB, jinfo->name, "Server and scheduler mismatch: %s", jinfo->comment);
          run_errors++;
          continue;
          }

        /* refresh magrathea state after run succeeded or failed */
        query_external_cache(p_info,1);
        }
      else
        {
        jinfo->can_not_run = 1;

        char log_msg[MAX_LOG_SIZE]; /* used to log an message about job */
        char comment[MAX_COMMENT_SIZE]; /* used to update comment of job */

        if (translate_job_fail_code(ret, comment, log_msg))
          {
          /* determine connection and update on correct server */
          if (update_job_comment(p_connections.get_master_connection(), jinfo, comment) == 0)
            sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, jinfo->name, log_msg);

          if ((ret != NOT_QUEUED) && cstat.strict_fifo && (!jinfo->queue->is_global))
            {
            update_jobs_cant_run(p_connections.get_master_connection(), jinfo->queue->jobs, jinfo, COMMENT_STRICT_FIFO, START_AFTER_JOB);
            }
          }
        }
      }

    if (cstat.fair_share)
      {
      update_last_running();
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Dumping fairshare\n");
      dump_current_fairshare(conf.group_root);
      }

    cleanup_servers();

    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Leaving schedule\n");
    }

  if (conf.prime_fs || conf.non_prime_fs)
    write_usage();
  }
  catch (const exception& e)
    {
    sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "Unexpected exception caught : %s", e.what());
    }
  }
