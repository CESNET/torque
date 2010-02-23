/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "pbs_error.h"
#include "pbs_ifl.h"
#include "sched_cmds.h"
#include <time.h>
#include "log.h"
#include <string.h>
#include "queue_info.h"
#include "server_info.h"
#include "check.h"
#include "constant.h"
#include "job_info.h"
#include "node_info.h"
#include "misc.h"
#include "fifo.h"
#include "config.h"
#include "sort.h"
#include "parse.h"
#include "globals.h"
#include "prev_job_info.h"
#include "fairshare.h"
#include "prime.h"
#include "dedtime.h"
#include "token_acct.h"

/* WORLD SCHEDULING */
#include "world.h"

static time_t last_decay;
static time_t last_sync;


/*
 *
 * schedinit - initialize conf struct and parse conf files
 *
 *   argc - passed in from main
 *   argv - passed in from main
 *
 * Returns Success/Failure
 *
 *
 */
int schedinit(int argc, char *argv[])
  {
  init_config();
  parse_config(CONFIG_FILE);
  parse_holidays(HOLIDAYS_FILE);

  time(&(cstat.current_time));

  if (is_prime_time())
    init_prime_time();
  else
    init_non_prime_time();

  parse_ded_file(DEDTIME_FILE);

  /* preload the static members to the fairshare tree */
  preload_tree();

  parse_group(RESGROUP_FILE);

  calc_fair_share_perc(conf.group_root -> child, UNSPECIFIED);

  if (conf.prime_fs || conf.non_prime_fs)
    {
    read_usage();

    /*
     * initialize the last_decay to the current time, since we do not know when
     * the last decay happened
     */
    last_sync = last_decay = cstat.current_time;
    }

  token_acct_open((char *)0);

  return 0;
  }

/*
 *
 * update_cycle_status - update global status structure which holds
 *         status information used by the scheduler which
 *         can change from cycle to cycle
 *
 * returns nothing
 *
 */
void
update_cycle_status(void)
  {
  char dedtime;    /* boolean: is it dedtime? */
  enum prime_time prime;  /* current prime time status */

  time(&(cstat.current_time));
  dedtime = is_ded_time();

  /* it was dedtime last scheduling cycle, and it is not dedtime now */

  if (cstat.is_ded_time && !dedtime)
    {
    /* since the current dedtime is now passed, set it to zero and sort it to
     * the end of the dedtime array so the next dedtime is at the front
     */
    conf.ded_time[0].from = 0;
    conf.ded_time[0].to = 0;
    qsort(conf.ded_time, MAX_DEDTIME_SIZE, sizeof(struct timegap), cmp_ded_time);
    }

  cstat.is_ded_time = dedtime;


  /* if we have changed from prime to nonprime or nonprime to prime
   * init the status respectivly
   */
  prime = is_prime_time();

  if (prime == PRIME && !cstat.is_prime)
    init_prime_time();
  else if (prime == NON_PRIME && cstat.is_prime)
    init_non_prime_time();
  }

/*
 *
 * init_scheduling_cycle - run things that need to be set up every
 *    scheduling cycle
 *
 * returns 1 on success 0 on failure
 *
 * NOTE: failure of this function will cause schedule() to return
 */

int init_scheduling_cycle(world_server_t* server)
  {
  group_info *user; /* the user for the running jobs of the last cycle */
  queue_info *qinfo; /* user to cycle through the queues to sort the jobs */
  char decayed = 0; /* boolean: have we decayed usage? */
  time_t t;  /* used in decaying fair share */
  int i, j;

  if (cstat.fair_share)
    {
    if (server->last_running != NULL)
      {
      /* add the usage which was accumulated between the last cycle and this
       * one and calculate a new value
       */

      for (i = 0; i < server->last_running_size ; i++)
        {
        job_info** jobs;
        user = server -> last_running[i].ginfo;
#if HIGH_PRECISION_FAIRSHARE
        jobs = server -> info -> jobs; /* check all jobs (exiting, completed, running) */
#else
        jobs = server -> info -> running_jobs; /* check only running */
#endif

        for (j = 0; jobs[j] != NULL; j++)
          {
            if (jobs[j] -> is_completed || jobs[j] -> is_exiting ||
              jobs[j] -> is_running)
              if (!strcmp(server->last_running[i].name, jobs[j] -> name))
                break;
          }

        if (jobs[j] != NULL)
          {
          user -> usage +=
            calculate_usage_value(jobs[j] -> resused) -
            calculate_usage_value(server -> last_running[i].resused);
          }
        }

      /* assign usage into temp usage since temp usage is used for usage
       * calculations.  Temp usage starts at usage and can be modified later.
       */
      for (i = 0; i < server->last_running_size; i++)
        server->last_running[i].ginfo -> temp_usage = server->last_running[i].ginfo -> usage;
      }

    /* The half life for the fair share tree might have passed since the last
     * scheduling cycle.  For that matter, several half lives could have
     * passed.  If this is the case, perform as many decays as necessary
     */

    t = cstat.current_time;

    while (t - last_decay > conf.half_life)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "", "Decaying Fairshare Tree");
      decay_fairshare_tree(conf.group_root);
      t -= conf.half_life;
      decayed = 1;
      }

    if (decayed)
      {
      /* set the time to the acuall the half-life should have occured */
      last_decay = cstat.current_time -
                   (cstat.current_time - last_decay) % conf.half_life;
      }

    if (cstat.current_time - last_sync > conf.sync_time)
      {
      write_usage();
      last_sync = cstat.current_time;
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "", "Usage Sync");
      }
    }

  if (cstat.help_starving_jobs)
    cstat.starving_job = update_starvation(server -> info -> jobs);

  /* sort queues by priority if requested */

  if (cstat.sort_queues)
    qsort(server -> info -> queues, server -> info -> num_queues, sizeof(queue_info *),
          cmp_queue_prio_dsc);


  if (cstat.sort_by[0].sort != NO_SORT)
    {
    if (cstat.by_queue || cstat.round_robin)
      {
      for (i = 0; i < server -> info -> num_queues; i++)
        {
        qinfo = server -> info -> queues[i];
        qsort(qinfo -> jobs, qinfo -> sc.total, sizeof(job_info *), cmp_sort);
        }
      }
    else
      qsort(server -> info -> jobs, server -> info -> sc.total, sizeof(job_info *), cmp_sort);
    }

  next_job(server -> info, INITIALIZE);

  return 1;  /* SUCCESS */
  }

/*
 *
 * schedule - this function gets called to start each scheduling cycle
 *     It will handle the difference cases that caused a
 *     scheduling cycle
 *
 *   cmd - reason for scheduling cycle
 *   SCH_ERROR  error
 *  SCH_SCHEDULE_NULL NULL command
 *  SCH_SCHEDULE_NEW A new job was submited
 *  SCH_SCHEDULE_TERM A job terminated
 *  SCH_SCHEDULE_TIME The scheduling interval expired
 *  SCH_SCHEDULE_RECYC A scheduling recycle(see admin guide)
 *  SCH_SCHEDULE_CMD The server scheduling variabe was set
 *     or reset to true
 *  SCH_SCHEDULE_FIRST the first cycle after the server starts
 *  SCH_CONFIGURE  perform scheduling configuration
 *  SCH_QUIT  have the scheduler quit
 *  SCH_RULESET  reread the scheduler ruleset
 *
 *   sd - connection desctiptor to the pbs server
 *
 *
 * returns 1: continue calling scheduling cycles, 0: exit scheduler
 */

int schedule(

  int cmd,
  int sd)

  {
  world_init(&cluster);

  sprintf(log_buffer,"No. %d",cmd);
  sched_log(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER, "Woken up by command",log_buffer);
    
  switch (cmd)
    {

    case SCH_ERROR:

    case SCH_SCHEDULE_NULL:

    case SCH_RULESET:

    case SCH_SCHEDULE_RECYC:

      /* ignore and end cycle */

      break;

    case SCH_SCHEDULE_NEW:

    case SCH_SCHEDULE_TERM:

    case SCH_SCHEDULE_FIRST:

    case SCH_SCHEDULE_CMD:

    case SCH_SCHEDULE_TIME:

      return(scheduling_cycle(sd));

      /*NOTREACHED*/

      break;

    case SCH_CONFIGURE:

      if (conf.prime_fs || conf.non_prime_fs)
        write_usage();

      reinit_config();

      parse_config(CONFIG_FILE);

      parse_holidays(HOLIDAYS_FILE);

      parse_ded_file(DEDTIME_FILE);

      preload_tree();

      parse_group(RESGROUP_FILE);

      calc_fair_share_perc(conf.group_root -> child, UNSPECIFIED);

      if (conf.prime_fs || conf.non_prime_fs)
        read_usage();

      break;

    case SCH_QUIT:
      if (conf.prime_fs || conf.non_prime_fs)
        write_usage();

      return 1;  /* have the scheduler exit nicely */

    default:
      return 0;
    }

  return(0);
  }  /* END schedule() */



int move_update_job(int sd, world_server_t* src, job_info* job,
                    world_server_t* dst, queue_info* dst_q)
  {
  char deststr[PBS_MAXSERVERNAME + PBS_MAXQUEUENAME + 2];
  int ret;
      
  sprintf(deststr, "%s@%s", dst_q->name, dst->info->name);
      
  ret = pbs_movejob(sd,job->name,deststr,NULL);
  switch(ret)
    {
    case 0: 
      sched_log(PBSEVENT_DEBUG2,
                PBS_EVENTCLASS_SERVER,
                deststr,
                "Move successfull.");
      update_server_on_move(dst->info,dst_q,job);
      update_queue_on_move(dst_q,job);
      update_job_on_move(job);
      nodes_preassign_clean(dst->info->nodes,dst->info->num_nodes);
      return 0;
      break;

    case PBSE_PROTOCOL:
      sched_log(PBSEVENT_DEBUG2,
                PBS_EVENTCLASS_SERVER,
                deststr,
                "Couldn't move job to server - communication error.");
      dst->is_down = 1;
      break;
    default:
      sched_log(PBSEVENT_DEBUG2,
                  PBS_EVENTCLASS_SERVER,
                  deststr,
                  "Couldn't move job to server - unknown error.");          
    }
    return ret;
  }

int job_is_movable(job_info* job)
  {
  if (job->can_not_run || (!job->queue->is_global) || (!job->is_queued))
    return 0;
  else
    return 1;
}

/*
 *
 * scheduling_cycle - the controling function of the scheduling cycle
 *
 *   sd  - connection descriptor to the pbs server
 *
 * returns success/failure
 *
 */

int scheduling_cycle(

  int sd)

  {
  server_info *sinfo;  /* ptr to the server/queue/job/node info */
  world_server_t* server;
  job_info *jinfo;  /* ptr to the job to see if it can run */
  int ret = SUCCESS;  /* return code from is_ok_to_run_job() */
  char log_msg[MAX_LOG_SIZE]; /* used to log an message about job */
  char comment[MAX_COMMENT_SIZE]; /* used to update comment of job */
  int i, j, lock = 1;

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Entering Schedule");

  update_cycle_status();

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Cycle status update, acquiring lock.");

  /* acquire lock */
  if (conf.lock_server)
  if ((ret = pbs_lock(sd,0,NULL)) != PBSE_NONE)
    {
    lock = 0;
    sched_log(PBSEVENT_DEBUG,
              PBS_EVENTCLASS_SERVER,
              "",
              pbse_to_txt(ret));
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Fetching server info.");

  /* create the server / queue / job / node structures */
  if ((sinfo = query_server(sd)) == NULL)
    {
    fprintf(stderr, "Problem with creating server data strucutre\n");

    return(0);
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_SERVER, "talking to", sinfo->name);
    
  if ((server = world_add(&cluster,sinfo)) == NULL)
    {
    sched_log(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_SERVER,
      sinfo -> name,
      "Couldn't store server info.");
    return 0;
    }
    
  world_check_update(&cluster);

  if (cstat.help_starving_jobs)
    {
    /* starving jobs support */
    if (init_scheduling_cycle(server) == 0)
      {
      sched_log(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_SERVER,
        sinfo -> name,
        "init_scheduling_cycle failed.");

      return(0);
      }

    while ((jinfo = next_job(server->info, 0)) != NULL)
      {
      if (jinfo->is_starving) /* set in update_starvation() */
        {
        sched_log(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER,
                  jinfo->name, "Considering starving job.");
        query_external_cache(server->info);
        is_ok_to_run_job(sd, server->info, jinfo->queue, jinfo, 1);
        }
      }
    } /* end starving jobs support */

  if (init_scheduling_cycle(server) == 0)
    {
    sched_log(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_SERVER,
      sinfo -> name,
      "init_scheduling_cycle failed.");

    /* do not free on fail, we are keeping the data
     * free_server(sinfo, 1);
     */

    return(0);
    }

  /* main scheduling loop */
  while (((!conf.lock_server) || lock) && (jinfo = next_job(server->info, 0)))
    {
    sched_log(
      PBSEVENT_DEBUG2,
      PBS_EVENTCLASS_JOB,
      jinfo->name,
      "Considering job to run");

    /* refresh lock */
    if (conf.lock_server)
    if ((ret = pbs_lock(sd,1,NULL)) != PBSE_NONE)
      {
      lock = 0;
      sched_log(PBSEVENT_DEBUG,
                PBS_EVENTCLASS_SERVER,
                sinfo -> name,
                pbse_to_txt(ret));
      continue;
      }

    query_external_cache(server->info);
    if ((ret = is_ok_to_run_job(sd, server->info, jinfo->queue, jinfo, 0)) == SUCCESS)
      {
      sched_log(
          PBSEVENT_DEBUG2,
          PBS_EVENTCLASS_JOB,
          jinfo->name,
          "Trying to run locally.");
      if (run_update_job(sd, server->info, jinfo->queue, jinfo) != 0)
        jinfo->can_not_run = 1;
      }
    else
      {
      if (conf.move_jobs && job_is_movable(jinfo))
        {
        i = 0;
        while (i < cluster.count)
          {
          /* if this is our local server, skip */
          if (&cluster.servers[i] == server)
#if 0 /* address comparison should be enough */
            if (!strcmp(cluster.servers[i].info->name,server->info->name))
#endif
              { i++; continue; }

          /* find remote queue */
          for (j = 0; j < cluster.servers[i].info->num_queues; j++)
            if (!strcmp(cluster.servers[i].info->queues[j]->name,
                        jinfo->queue->name))
              break;

          /* destination server does not have the correct queue, skip */
          if (j == cluster.servers[i].info->num_queues)
            { i++; continue; }

          /* if cannot run on the remote server, skip */
          if (is_ok_to_run_job(-1,cluster.servers[i].info,
                                  cluster.servers[i].info->queues[j],
                                  jinfo, 0) != SUCCESS)
            { i++; continue; }

          /* try to move */
          if (move_update_job(sd,server,jinfo,&cluster.servers[i],
                          cluster.servers[i].info->queues[j]) == 0)
            break;
          else
            { /* something happened on the server, lets try the next one */
            ret = JOB_FAILED_MOVE;
            i++;
            }
          }

          if (i != cluster.count && ret != JOB_FAILED_MOVE)
            ret = JOB_MOVED;
          else
            ret = JOB_FAILED_MOVE;
        }

        /* either the job is local and cannot run,
         * or the job is global and just moved
         * or the job is global and move failed
         *
         * The can never run part was removed.
         */
        jinfo->can_not_run = 1;

        if (ret != JOB_MOVED)
          if (translate_job_fail_code(ret, comment, log_msg))
            {
            /* if the comment doesn't get changed, its because it hasn't changed.
             * if the reason for the job has not changed, we do not need to log it
             */

            if (update_job_comment(sd, jinfo, comment) == 0)
              {
              sched_log(
                        PBSEVENT_SCHED,
                        PBS_EVENTCLASS_JOB,
                        jinfo->name,
                        log_msg);
              }
            }

        if ((ret != NOT_QUEUED) && (ret != JOB_MOVED) && cstat.strict_fifo)
          {
          update_jobs_cant_run(
                               sd,
                               jinfo->queue->jobs,
                               jinfo,
                               COMMENT_STRICT_FIFO,
                               START_AFTER_JOB);
          }
      }
    }

  if (cstat.fair_share)
    update_last_running(server);

  /* free_server(sinfo, 1); */

  if (conf.lock_server)
  if (lock)
    pbs_lock(sd,2,NULL);

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "", "Leaving schedule\n");

  return 0;
  }




/*
 *
 * update_last_running - update the last_running job array
 *         keep the currently running jobs till next
 *         scheduling cycle
 *
 *   sinfo - the server of current jobs
 *
 * Returns success/failure
 *
 */
int update_last_running(world_server_t* server)
  {
  free_pjobs(server->last_running, server->last_running_size);

  server->last_running = create_prev_job_info(server -> info -> running_jobs,
                                      server -> info -> sc.running);
  server->last_running_size = server -> info -> sc.running;

  if (server->last_running == NULL)
    return 0;

  return 1;
  }

/*
 *
 * update_starvation - update starvation information in array of jobs
 *
 *   jobs - job array to update
 *
 * returns the most starving job
 *
 */
job_info *update_starvation(job_info **jobs)
  {
  job_info *jinfo = NULL; /* used as a ptr to the job to update */
  int i;

  if (jobs != NULL)
    {
    for (i = 0; jobs[i] != NULL; i++)
      {
      if (jobs[i] -> queue -> starving_support >= 0 &&
          jobs[i] -> qtime + jobs[i] -> queue -> starving_support < cstat.current_time &&
          jobs[i] -> is_queued)
        {
        jobs[i] -> is_starving = 1;
        jobs[i] -> sch_priority =
          cstat.current_time - jobs[i] -> qtime - jobs[i] -> queue -> starving_support;

        if (jinfo == NULL || jobs[i] -> sch_priority > jinfo -> sch_priority)
          jinfo = jobs[i];
        }

      /* job not starving yet
      else
        ;
      */
      }
    }

  return jinfo;
  }




/*
 *
 * run_update_job - run the job and update the job information
 *
 *   pbs_sd - connection to pbs_server
 *   sinfo  - server job is on
 *   qinfo  - queue job resides in
 *   jinfo  - the job to run
 *
 * returns success/failure - see pbs_errno for more info
 *
 */
int run_update_job(int pbs_sd, server_info *sinfo, queue_info *qinfo,
                   job_info *jinfo)
  {
  int ret;    /* return code from pbs_runjob() */
  node_info *best_node = NULL;  /* best node to run job on */
  char *best_node_name = NULL;  /* name of best node */
  char buf[256] = {'\0'};  /* generic buffer - comments & logging*/
  char timebuf[128];   /* buffer to hold the time and date */
  resource_req *res;   /* ptr to the resource of ncpus */
  int ncpus;    /* numeric amount of resource ncpus */
  char *errmsg;    /* used for pbs_geterrmsg() */

  strftime(timebuf, 128, "started on %a %b %d at %H:%M", localtime(&cstat.current_time));

  if (cstat.load_balancing || cstat.load_balancing_rr)
    {
    best_node = find_best_node(jinfo, sinfo -> timesharing_nodes);

    if (best_node != NULL)
      {
      best_node_name = strdup(best_node -> name);
      sprintf(buf, "Job run on node %s - %s", best_node_name, timebuf);
      }
    }

  /* construct nodespec assignments */
  if (best_node_name == NULL)
    best_node_name = nodes_preassign_string(jinfo, sinfo->nodes, sinfo->num_nodes);

  printf("Target nodes are: %s\n",best_node_name);

  if (best_node == NULL)
    sprintf(buf, "Job %s", timebuf);

  update_job_comment(pbs_sd, jinfo, buf);

  buf[0] = '\0';

  ret = pbs_runjob(pbs_sd, jinfo -> name, best_node_name, NULL);

  /* cleanup */
  free(best_node_name);

  if (ret == 0)
    {
    /* If a job is 100% efficent, it will raise the load average by 1 per
     * cpu is uses.  Temporarly inflate load average by that value
     */
    if (cstat.load_balancing && best_node != NULL)
      {
      if ((res = find_resource_req(jinfo -> resreq, "ncpus")) == NULL)
        ncpus = 1;
      else
        ncpus = res -> amount;

      best_node -> loadave += ncpus;
      }

    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, jinfo -> name, "Job Run");

    update_server_on_run(sinfo, qinfo, jinfo);

    update_queue_on_run(qinfo, jinfo);

    update_job_on_run(pbs_sd, jinfo);

    if (cstat.fair_share)
      update_usage_on_run(jinfo);

    free(sinfo -> running_jobs);

    sinfo -> running_jobs = job_filter(sinfo -> jobs, sinfo -> sc.total,
                                       check_run_job, NULL);

    free(qinfo -> running_jobs);

    qinfo -> running_jobs = job_filter(qinfo -> jobs, qinfo -> sc.total,
                                       check_run_job, NULL);
    }
  else
    {
    errmsg = pbs_geterrmsg(pbs_sd);
    sprintf(buf, "Not Running - PBS Error: %s", errmsg);
    update_job_comment(pbs_sd, jinfo, buf);
    }

  nodes_preassign_clean(sinfo->nodes, sinfo->num_nodes);

  return ret;
  }

/*
 *
 * next_job - find the next job to be run by the scheduler
 *
 *   sinfo - the server the jobs are on
 *   init  - whether or not to initialize
 *
 * returns
 *     - the next job to run
 *     - NULL on error or if there are no more jobs to run
 *
 */
job_info *next_job(server_info *sinfo, int init)
  {
  /* cjobs is an array of the queue's job_info arrays.  It is used to cycle
   * through the jobs returning 1 job from each queue
   */
  static job_info ***cjobs = NULL;

  /* last_queue is the index into a queue array of the last time
   * the function was called
   */
  static int last_queue;

  /* last_job is the index into a job array of the last time
   * the function was called
   */
  static int last_job;

  job_info *rjob = NULL;  /* the job to return */
  int i;

  if (cstat.round_robin)
    {
    if (init == INITIALIZE)
      {
      if (cjobs != NULL)
        {
        free(cjobs);
        cjobs = NULL;
        }

      /*
       * cjobs is an array of pointers which will point to each of the job
       * arrays in each queue
       */
      if (cjobs == NULL)
        {
        if ((cjobs = (job_info ***) malloc(sizeof(job_info**) * sinfo -> num_queues)) == NULL)
          {
          perror("Memory Allocation Error");
          return NULL;
          }

        last_queue = -1;

        last_job = 0;

        for (i = 0; i < sinfo -> num_queues; i++)
          cjobs[i] = sinfo -> queues[i] -> jobs;
        }
      } /* end initalization */
    else
      {
      /* find the next queue to run a job out of */
      for (i = 0; i < sinfo -> num_queues && rjob == NULL; i++)
        {
        /* we have reached the end of the array, cycle back through */
        if (last_queue == (sinfo -> num_queues - 1))
          {
          last_queue = 0;
          last_job++;
          }
        else /* still more queues to go */
          last_queue++;

        if (cjobs[last_queue] != NULL)
          {
          if (cjobs[last_queue][last_job] == NULL)
            cjobs[last_queue] = NULL;
          else
            {
            if (cstat.fair_share)
              rjob = extract_fairshare(cjobs[last_queue]);
            else if (cjobs[last_queue][last_job] -> can_not_run == 0)
              rjob = cjobs[last_queue][last_job];
            }
          }
        }
      }
    }
  else if (cstat.by_queue)
    {
    if (init == INITIALIZE)
      {
      last_job = -1;
      last_queue = 0;
      }
    else
      {
      last_job++;

      /* check if we have reached the end of a queue and need to find another */

      while (last_queue < sinfo -> num_queues &&
             (sinfo -> queues[last_queue] -> jobs == NULL ||
              sinfo -> queues[last_queue] -> jobs[last_job] == NULL ||
              sinfo -> queues[last_queue] -> jobs[last_job] -> can_not_run))
        {
        if (sinfo -> queues[last_queue] -> jobs == NULL ||
            sinfo -> queues[last_queue] -> jobs[last_job] == NULL)
          {
          last_queue++;
          last_job = 0;
          }
        else if (sinfo -> queues[last_queue] -> jobs[last_job] -> can_not_run)
          last_job++;
        }

      if (last_queue == sinfo -> num_queues ||
          sinfo -> queues[last_queue] == NULL)
        rjob = NULL;
      else
        {
        if (cstat.fair_share)
          rjob = extract_fairshare(sinfo -> queues[last_queue] -> jobs);
        else
          {
          if (last_job < sinfo -> queues[last_queue] -> sc.total)
            rjob = sinfo -> queues[last_queue] -> jobs[last_job];
          }
        }
      }
    }
  else /* treat the entire system as one large queue */
    {
    if (init == INITIALIZE)
      {
      last_job = -1;
      }
    else
      {
      if (cstat.fair_share)
        rjob = extract_fairshare(sinfo -> jobs);
      else
        {
        last_job++;

        while (sinfo -> jobs[last_job] != NULL &&
               sinfo -> jobs[last_job] -> can_not_run)
          {
          last_job++;
          }

        rjob = sinfo -> jobs[last_job];
        }
      }
    }

  return rjob;
  }
