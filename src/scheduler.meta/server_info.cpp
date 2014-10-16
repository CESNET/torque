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
#include <string.h>
#include "torque.h"
#include "server_info.h"
#include "constant.h"
#include "queue_info.h"
#include "job_info.h"
#include "misc.h"
#include "config.h"
#include "node_info.h"
#include "assertions.h"
#include "globals.h"
#include "utility.h"
extern "C" {
#include "site_pbs_cache.h"
}
#include "nodespec_sch.h"
#include "sort.h"
#include "global_macros.h"

#include "RescInfoDb.h"
#include "base/MiscHelpers.h"
using namespace Scheduler;
using namespace Base;

#include <new>
using namespace std;

/*
 *
 * query_server - creates a structure of arrays consisting of a server
 *   and all the queues and jobs that reside in that server
 *
 *   pbs_sd - connection to pbs_server
 *
 * returns a pointer to the server_info struct
 *
 */
server_info *query_server(int pbs_sd)
  {

  struct batch_status *server; /* info about the server */
  server_info *sinfo;  /* scheduler internal form of server info */
  queue_info **qinfo;  /* array of queues on the server */

  int i;

  /* get server information from pbs server */

  if ((server = pbs_statserver(pbs_sd, NULL, NULL)) == NULL)
    {
    fprintf(stderr, "pbs_statserver failed: %d\n", pbs_errno);
    return NULL;
    }

  /* convert batch_status structure into server_info structure */
  if ((sinfo = query_server_info(server)) == NULL)
    {
    pbs_statfree(server);
    return NULL;
    }

  /* get the nodes, if any */
  if ((sinfo->nodes = query_nodes(pbs_sd, sinfo)) == NULL)
    {
    pbs_statfree(server);
    free_server(sinfo, 0);
    return NULL;
    }

  query_external_cache(sinfo,0);

  // requires query external cache results
  for (int i = 0; i < sinfo->num_nodes; i++)
    {
    sinfo->nodes[i]->fetch_bootable_alternatives();
    }

  /* get the queues */
  if ((sinfo -> queues = query_queues(pbs_sd, sinfo)) == NULL)
    {
    pbs_statfree(server);
    free_server(sinfo, 0);
    return NULL;
    }

  /* count the queues and total up the individual queue states
   * for server totals. (total up all the state_count structs)
   */
  qinfo = sinfo -> queues;

  while (*qinfo != NULL)
    {
    sinfo -> num_queues++;
    total_states(&(sinfo -> sc), &((*qinfo) -> sc));
    qinfo++;
    }

  if ((sinfo -> jobs = (job_info **) malloc(sizeof(job_info *) * (sinfo -> sc.total + 1))) == NULL)
    {
    free_server(sinfo, 1);
    perror("Memory allocation error");
    return NULL;
    }

  set_jobs(sinfo);

  sinfo -> running_jobs =
    job_filter(sinfo -> jobs, sinfo -> sc.total, check_run_job, NULL);

  sinfo -> non_dedicated_nodes =

    node_filter(sinfo -> nodes, sinfo -> num_nodes, is_node_non_dedicated, NULL);

  i = 0;
  if (sinfo->num_nodes > 0)
    while (sinfo -> non_dedicated_nodes[i] != NULL) i++;
  sinfo -> non_dedicated_node_count = i;

  pbs_statfree(server);

  for (i = 0; i < sinfo->sc.running; i++)
    {
    RescInfoDb::iterator j;
    for (j = resc_info_db.begin(); j != resc_info_db.end(); j++)
      {
      resource_req *resreq;

      /* skip non-dynamic resource */
      if (j->second.source != ResCheckDynamic)
        continue;

      /* no request for this resource */
      if ((resreq = find_resource_req(sinfo->running_jobs[i]->resreq, j->second.name.c_str())) == NULL)
        continue;

      map<string,DynamicResource>::iterator it = sinfo->dynamic_resources.find(j->second.name);
      if (it != sinfo->dynamic_resources.end())
        {
        it->second.add_scheduled(resreq->amount);
        }
      }
    }

  for (i = 0; i < sinfo->num_nodes && sinfo->nodes[i] != NULL; i++)
    {
    sinfo->nodes[i]->expand_virtual_nodes();
    }

  return sinfo;
  }

/*
 *
 * query_server_info - takes info from a batch_status structure about
 *       a server into a server_info structure for easy
 *       access
 *
 *   server - batch_status struct of server info
 *   last   - the last server so the new one can be added to the chain
 *     possibly NULL
 *
 * returns newly allocated and filled server_info struct
 *
 */

server_info *query_server_info(struct batch_status *server)
  {

  struct attrl *attrp; /* linked list of attributes */
  server_info *sinfo; /* internal scheduler structure for server info */
  sch_resource_t count; /* used to convert string -> integer */
  char *endp;  /* used with strtol() */

  if ((sinfo = new_server_info()) == NULL)
    return NULL;    /* error */

  sinfo -> name = strdup(server -> name);

  attrp = server -> attribs;

  while (attrp != NULL)
    {
    if (!strcmp(attrp -> name, ATTR_dfltque)) /* default_queue */
      sinfo -> default_queue = strdup(attrp -> value);
    else if (!strcmp(attrp -> name, ATTR_maxrun))  /* max_running */
      {
      count = strtol(attrp -> value, &endp, 10);

      if (*endp != '\0')
        count = -1;

      sinfo -> max_run = count;
      }
    else if (!strcmp(attrp -> name, ATTR_maxuserrun))  /* max_user_run */
      {
      count = strtol(attrp -> value, &endp, 10);

      if (*endp != '\0')
        count = -1;

      sinfo -> max_user_run = count;
      }
    else if (!strcmp(attrp -> name, ATTR_maxgrprun))  /* max_group_run */
      {
      count = strtol(attrp -> value, &endp, 10);

      if (*endp != '\0')
        count = -1;

      sinfo -> max_group_run = count;
      }
    else if (!strcmp(attrp -> name, ATTR_MaxInstallingNodes))
      {
      count = strtol(attrp -> value, &endp, 10);

      if (*endp != '\0')
        count = -1;

      sinfo -> max_installing_nodes = count;
      }
    else if (!strcmp(attrp -> name, ATTR_tokens))  /* tokens */
      {
      sinfo->tokens = get_token_array(attrp -> value);
      }
    else if (!strcmp(attrp -> name, ATTR_jobstarttimeout))
      {
      count = strtol(attrp -> value, &endp, 10);

      if (*endp != '\0')
        count = DEFAULT_JOB_START_TIMEOUT;

      sinfo -> job_start_timeout = count;
      }

    attrp = attrp -> next;
    }

  return sinfo;
  }

/*
 *
 * free_server_info - free the space used by a server_info structure
 *
 * returns norhing
 *
 */
void free_server_info(server_info *sinfo)
  {
  if (sinfo -> name != NULL)
    free(sinfo -> name);

  if (sinfo -> default_queue != NULL)
    free(sinfo -> default_queue);

  if (sinfo -> jobs != NULL)
    free(sinfo -> jobs);

  if (sinfo -> running_jobs != NULL)
    free(sinfo -> running_jobs);

  if (sinfo -> non_dedicated_nodes != NULL)
    free(sinfo -> non_dedicated_nodes);

  delete sinfo;
  }

/*
 *
 * new_server_info - allocate and initalize a new server_info struct
 *
 * returns pointer to new allocated struct
 *
 */
server_info *new_server_info()
  {
  server_info *sinfo;   /* the new server */

  if ((sinfo = new (nothrow) server_info) == NULL)
    return NULL;

  sinfo -> name = NULL;

  sinfo -> default_queue = NULL;

  sinfo -> queues = NULL;

  sinfo -> jobs = NULL;

  sinfo -> running_jobs = NULL;

  sinfo -> nodes = NULL;

  sinfo -> non_dedicated_nodes = NULL;

  sinfo -> num_queues = 0;

  sinfo -> num_nodes = 0;

  sinfo -> max_run = INFINITY;

  sinfo -> max_user_run = INFINITY;

  sinfo -> max_group_run = INFINITY;

  sinfo -> max_installing_nodes = INFINITY;

  sinfo -> tokens = NULL;

  sinfo -> job_start_timeout = DEFAULT_JOB_START_TIMEOUT;

  init_state_count(&(sinfo -> sc));

  return sinfo;
  }

/*
 *
 * free_server - free a server and possibly its queues also
 *
 *   sinfo - the server_info list head
 *   free_queues_too - flag to free the queues attached to server also
 *
 * returns nothing
 *
 */
void free_server(server_info *sinfo, int free_objs_too)
  {
  if (sinfo == NULL)
    return;

  if (free_objs_too)
    {
    free_queues(sinfo -> queues, 1);
    free_nodes(sinfo -> nodes);
    }

  free_token_list(sinfo -> tokens);

  free_server_info(sinfo);
  }

/** Update server on job move
 *
 * Update the server_info structure with new information, when new job is pushed
 * into the server.
 *
 * @param sinfo Server to be updated
 * @param qinfo Destination queue
 * @param jinfo Job that is moved
 */
void update_server_on_move(server_info *sinfo, job_info *jinfo)
  {
  sinfo -> sc.running++;
  jinfo -> queue -> server -> sc.queued--;

  for (int i = 0; i < sinfo->num_nodes; i++)
    if (sinfo->nodes[i]->has_assignment())
      jinfo->plan_on_node(sinfo->nodes[i],sinfo->nodes[i]->get_assignment());
  }

/*
 *
 * update_server_on_run - update server_info strucutre when a job is run
 *
 *   sinfo - the server to update
 *   qinfo - the queue the job is in
 *   jinfo - the job that was run
 *
 * returns nothing
 *
 */
void update_server_on_run(server_info *sinfo, job_info *jinfo)
  {
  sinfo -> sc.running++;
  sinfo -> sc.queued--;

  jinfo->plan_on_server(sinfo);

  for (int i = 0; i < sinfo->num_nodes; i++)
    if (sinfo->nodes[i]->has_assignment())
      jinfo->plan_on_node(sinfo->nodes[i],sinfo->nodes[i]->get_assignment());
  }

/*
 *
 * set_jobs - create a large server job array of all the jobs on the
 *     system by coping all the jobs from the queue job arrays
 *
 *   sinfo - the server
 *
 * returns nothing
 */
void set_jobs(server_info *sinfo)
  {
  queue_info **qinfo;  /* used to cycle through the array of queues */
  job_info **jinfo;  /* used in copying jobs to server array */
  int i = 0, j;

  qinfo = sinfo -> queues;

  while (*qinfo != NULL)
    {
    jinfo = (*qinfo) -> jobs;

    if (jinfo != NULL)
      {
      for (j = 0; jinfo[j] != NULL; j++, i++)
        sinfo -> jobs[i] = jinfo[j];
      }

    qinfo++;
    }

  sinfo -> jobs[i] = NULL;
  }

/*
 *
 *      check_run_job - function used by job_filter to filter out
 *                      non-running jobs.
 *
 *        job - the job to check
 *        arg - optional arg
 *
 *      returns 1 if the job is running
 *
 */
int check_run_job(job_info *job, void * UNUSED(arg))
  {
  return job -> state == JobRunning;
  }

token** get_token_array(char* tokenlist)
  {
  char **list;
  token **token_array;
  int i, count;

  list = break_comma_list(strdup(tokenlist));

  count = 0;

  while (list[count] != NULL)
    {
    count++;
    }

  token_array = (token **)malloc(sizeof(token *) * (count + 1));

  for (i = 0;  i < count; i++)
    {
    token_array[i] = get_token(list[i]);
    }

  token_array[count] = NULL;

  free_string_array(list);

  return token_array;
  }

token* get_token(char* token_string)
  {

  char *colon;
  token *this_token = (token *) malloc(sizeof(token));
  char *work_string = strdup(token_string);
  colon = strstr(work_string, ":");
  this_token->count = atof(colon + 1);
  *colon = 0;
  this_token->identifier = work_string;

  return this_token;
  }

/*
 * Free a token list
 *
 */

void free_token_list(token ** tokens)
  {
  int i;
  token *this_token;

  if (tokens != NULL)
    {
    for (i = 0; (this_token = tokens[i]) != NULL; i++)
      {
      free_token(this_token);
      }

    free(tokens);
    }
  }

/*
 * Free an individual token
 *
 */

void free_token(token* token_ptr)
  {
  if (token_ptr != NULL)
    {
    if (token_ptr->identifier != NULL)
      {
      free(token_ptr->identifier);
      }

    free(token_ptr);
    }
  }

