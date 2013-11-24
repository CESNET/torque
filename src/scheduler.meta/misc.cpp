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
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <grp.h>
#include "torque.h"
#include "config.h"
#include "constant.h"
#include "misc.h"
#include "globals.h"
#include "fairshare.h"
#include "utility.h"
#include "site_pbs_cache_scheduler.h"
#include "assertions.h"
#include "node_info.h"
#include <stdarg.h>

#include "api.hpp"

#include "data_types.h"
#include "RescInfoDb.h"

#include <string>
#include <utility>
using namespace std;

/*
 *
 *      res_to_num - convert a resource string to a  sch_resource_t to
 *                      kilobytes
 *                      example: 1mb -> 1024
 *     1mw -> 1024 * SIZE_OF_WORD
 *
 *      res_str - the resource string
 *
 *      return a number in kilobytes or UNSPECIFIED on error
 *
 */

sch_resource_t res_to_num(char * res_str)
  {
  sch_resource_t count = UNSPECIFIED; /* convert string resource to numeric */
  sch_resource_t count2 = UNSPECIFIED; /* convert string resource to numeric */
  char *endp;    /* used for strtol() */
  char *endp2;    /* used for strtol() */
  long multiplier;   /* multiplier to count */

  count = strtol(res_str, &endp, 10);

  if (*endp == ':')   /* time resource -> convert to seconds */
    {
    count2 = strtol(endp + 1, &endp2, 10);

    if (*endp2 == ':')   /* form of HH:MM:SS */
      {
      count *= 3600;
      count += count2 * 60;
      count += strtol(endp2 + 1, &endp, 10);

      if (*endp != '\0')
        count = UNSPECIFIED;
      }
    else   /* form of MM:SS */
      {
      count *= 60;
      count += count2;
      }

    multiplier = 1;
    }
  else if (*endp == 'k' || *endp == 'K')
    multiplier = 1;
  else if (*endp == 'm' || *endp == 'M')
    multiplier = MEGATOKILO;
  else if (*endp == 'g' || *endp == 'G')
    multiplier = GIGATOKILO;
  else if (*endp == 't' || *endp == 'T')
    multiplier = TERATOKILO;
  else if (*endp == 'b' || *endp == 'B')
    {
    count /= KILO;
    multiplier = 1;
    }
  else if (*endp == 'w')
    {
    count /= KILO;
    multiplier = SIZE_OF_WORD;
    }
  else /* catch all */
    multiplier = 1;

  if (*endp != '\0' && *(endp + 1) == 'w')
    multiplier *= SIZE_OF_WORD;

  return count * multiplier;
  }

/*
 *
 *      skip_line - find if the line of the config file needs to be skipped
 *                  due to it being a comment or other means
 *
 *        line - the line from the config file
 *
 *      returns true if the line should be skipped or false if it should be
 *              parsed
 *
 */
int skip_line(char *line)
  {
  int skip = 0;    /* whether or not to skil the line */

  if (line != NULL)
    {
    while (isspace((int) *line))
      line++;

    /* '#' is comment in config files and '*' is comment in holidays file */
    if (line[0] == '\0' || line[0] == '#' || line[0] == '*')
      skip = 1;
    }

  return skip;
  }

/* Write a log entry to the scheduler log file using log_record */
void sched_log(int event, int event_class, const char *name, const char *text,...)
  {
  if (!(conf.log_filter & event) && text[0] != '\0')
    {
    va_list extras;
    va_start(extras,text);
    vsprintf(log_buffer,text,extras);
    log_record(event, event_class, (char*)name, (char*)log_buffer);
    /* TODO quick fix, log_record doesn't take const */
    va_end(extras);
    }
  }

/*
 *
 * break_comma_list - break apart a comma delemeted string into an array
 *      of strings
 *
 *   list - the comma list
 *
 * returns an array of strings
 *
 */
char **break_comma_list(char *list)
  {
  int num_words = 1;   /* number of words delimited by commas*/
  char **arr = NULL;   /* the array of words */
  char *tok;    /* used with strtok() */
  int i;

  if (list != NULL)
    {
    for (i = 0; list[i] != '\0'; i++)
      if (list[i] == ',')
        num_words++;

    if ((arr = (char **) malloc(sizeof(char*)  * (num_words + 1))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    tok = strtok(list, ",");

    for (i = 0; tok != NULL; i++)
      {


      retnull_on_null(arr[i] = strdup(tok));

      tok = strtok(NULL, ",");
      }

    arr[i] = NULL;
    }

  return arr;
  }

void comma_list_to_set(char *list, set<string>& s)
  {
  char *tok;

  while ((tok = strchr(list,',')) != NULL)
    {
    *tok = '\0';
    tok++;
    while (isspace((int) *tok))
      tok++;

    s.insert(string(list));
    list = tok;
    }
  s.insert(string(list));
  }

/*
 *
 * free_string_array - free an array of strings with a NULL as a sentinal
 *
 *   arr - the array to free
 *
 * returns nothing
 *
 */
void free_string_array(char **arr)
  {
  int i;

  if (arr != NULL)
    {
    for (i = 0; arr[i] != NULL; i++)
      free(arr[i]);

    free(arr);
    }
  }

/*
 *
 *      dup_string_array - duplicate an array of strings
 *
 *        ostrs - the array to copy
 *
 *      returns the duplicated array.
 *
 */
char **dup_string_array(char **ostrs)
  {
  char **nstrs = NULL;
  int i;

  if (ostrs != NULL)
    {
    for (i = 0; ostrs[i] != NULL; i++);

    if ((nstrs = (char **)malloc((i + 1) * sizeof(char**))) == NULL)
      {
      fprintf(stderr, "Memory Allocation Error for char **\n");
      return NULL;
      }

    for (i = 0; ostrs[i] != NULL; i++)
      retnull_on_null(nstrs[i] = strdup(ostrs[i]));

    nstrs[i] = NULL;
    }

  return nstrs;
  }

/*
 *
 * string_array_verify - verify two string arrays are equal
 *
 *   sa1 - string array 1
 *   sa2 - string array 2
 *
 * returns 0: array equal
 *  number of the first unequal string
 *  (unsigned) -1 on error
 *
 */
unsigned string_array_verify(char **sa1, char **sa2)
  {
  int i;

  if (sa1 == NULL && sa2 == NULL)
    return 0;

  if (sa1 == NULL || sa2 == NULL)
    return (unsigned) - 1;

  for (i = 0; sa1[i] != NULL && sa2[i] != NULL && !cstrcmp(sa1[i], sa2[i]); i++)
    ;

  if (sa1[i] != NULL || sa2[i] != NULL)
    return i + 1;

  return 0;
  }


/*
 *
 * calc_time_left - calculate the remaining time of a job
 *
 *   jinfo - the job to calculate
 *
 * returns time left on job or -1 on error
 *
 */
int calc_time_left(job_info *jinfo)
  {
  resource_req *req, *used;
  int used_amount;

  req = find_resource_req(jinfo -> resreq, "walltime");
  used = find_resource_req(jinfo -> resused, "walltime");

  if (req == NULL)
    return -1;

  /* If we can't find the used structure, we will just assume no usage */
  if (used == NULL)
    used_amount = 0;
  else
    used_amount = used -> amount;

  return req -> amount - used_amount;
  }

/*
 *
 * cstrcmp - check string compare - compares two strings but doesn't bomb
 *    if either one is null
 *
 *   s1 - string one
 *   s2 - string two
 *
 * returns -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2
 *
 */
int cstrcmp(char *s1, char *s2)
  {
  if (s1 == NULL && s2 == NULL)
    return 0;

  if (s1 == NULL && s2 != NULL)
    return -1;

  if (s1 != NULL && s2 == NULL)
    return 1;

  return strcmp(s1, s2);
  }

void query_external_cache(server_info *sinfo, int dynamic)
  {
  resource *res;
  node_info *node;
  char *value;
  int i;
  void *ptable;


  RescInfoDb::iterator j;
  for (j = resc_info_db.begin(); j != resc_info_db.end(); j++)
    {
    ptable=cache_hash_init();
    if ((j->second.source == ResCheckBoth) || (j->second.source == ResCheckCache))
      {
      if (xcache_hash_fill_local(j->second.name.c_str(),ptable) == 0)
        {
        for (i=0;i<sinfo -> num_nodes;i++)
          {
           node=sinfo -> nodes[i];
           value=xcache_hash_find(ptable,node->name);
           if (value!=NULL)
             {
             res = find_alloc_resource( node -> res, j->second.name.c_str() );

             if (node->res == NULL)
               node->res = res;

             if (res != NULL)
               {
               free(res -> str_avail);
               if (is_num(value))
                 {
                 res -> is_string = 0;
                 res -> avail = res_to_num(value);
                 res -> str_avail = value;
                 res -> assigned = 0;
                 res -> max = UNSPECIFIED;
                 }
               else
                 {
                 res -> is_string = 1;
                 res -> str_avail = value;
                 res -> avail = 0;
                 res -> assigned = 0;
                 }
               }
             }
           else
             {
             free(value);
             }
          }
        }
      else
        {
        sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, "pbs_cache",
                  "Couldn't fetch pbs_cache info for [%s] metric.",
                  j->second.name.c_str());
        }
      }
    cache_hash_destroy(ptable);
    }

  if (!dynamic)
    {
    /* read cluster info */
    ptable=cache_hash_init();
    if (xcache_hash_fill_local("phys_cluster",ptable)==0)
      {
      for (i=0;i<sinfo -> num_nodes;i++)
        {
        node=sinfo -> nodes[i];
        value=xcache_hash_find(ptable,node->name);
        if (value != NULL)
          {
          free(node->cluster_name);
          node->cluster_name = value;
          }
        }
      }
    else
      {
      sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, "pbs_cache",
                "Couldn't fetch pbs_cache info for [phys_cluster] metric.");
      }
    cache_hash_destroy(ptable);

    /* read cluster info */
    ptable=cache_hash_init();
    if (xcache_hash_fill_local("scratch_pool",ptable)==0)
      {
      for (i=0;i<sinfo -> num_nodes;i++)
        {
        node=sinfo -> nodes[i];
        value=xcache_hash_find(ptable,node->name);
        if (value != NULL)
          {
          node->scratch_pool = string(value);
          resc_info_db.insert(value," ",ResCheckDynamic); // register as new dynamic resource
          }
        free(value);
        }
      }
    else
      {
      sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, "scratch_pool",
                "Couldn't fetch pbs_cache info for [scratch_pool] metric.");
      }
    cache_hash_destroy(ptable);

    /* read server dynamic resources */
    ptable=cache_hash_init();
    if (xcache_hash_fill_local("dynamic_resources",ptable)==0)
      {
      for (j = resc_info_db.begin(); j != resc_info_db.end(); j++)
        {
        if (j->second.source == ResCheckDynamic)
          {
          value=xcache_hash_find(ptable,j->second.name.c_str());
          if (value != NULL)
            {
            sinfo->dynamic_resources.insert(make_pair(j->second.name,DynamicResource(j->second.name.c_str(),value)));
            free(value);
            }
          }
        }
      }
    else
      {
      sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, "pbs_cache",
                "Couldn't fetch pbs_cache info for [dynamic_resources] metric.");
      }
    cache_hash_destroy(ptable);
    }

  /* decode magrathea status */
  for (i=0; i<sinfo->num_nodes; i++)
    {
    sinfo->nodes[i]->process_magrathea_status();
    }

  return;
  }


/** Trivial check for number
 *
 * everything that begins with a digit is considered a number
 *
 * @param value string
 * @return 1 if number 0 if not
 */
int is_num(const char* value)
  {
  assert(value != NULL);
  if (isdigit(*value))
    return 1;
  else
    return 0;
  }

int cloud_check(job_info *jinfo)
{
  char *owner=NULL;
  char *group=NULL;
  char *cluster=NULL;
  struct group *g = NULL;
  char *jowner=NULL;
  char *tmp = NULL;
  int ret=0;

  tmp = strchr(jinfo->account,'@');
  if (tmp == NULL)
    {
    jowner = strdup(jinfo->account);
    }
  else
    {
    jowner = (char*) malloc(tmp-jinfo->account+1);
    strncpy(jowner,jinfo->account,tmp-jinfo->account);
    jowner[tmp-jinfo->account] = '\0';
    }

  if (jinfo->cluster_mode == ClusterUse) {
      cluster = xpbs_cache_get_local (jinfo->cluster_name, "cluster");

      if (cluster == NULL)
        {
        ret = CLUSTER_PERMISSIONS;
        goto perm_done;
        }

      retrieve_cluster_attr (cluster, "owner", &owner);
      retrieve_cluster_attr (cluster, "group", &group);

      if (owner == NULL)
        goto perm_done;

      if (strcmp(jowner,owner) == 0)
        goto perm_done;

      /* user does not match, check for group */
      if (group == NULL)
        {
        ret = CLUSTER_PERMISSIONS;
        goto perm_done;
        }

      g = getgrnam(group);
      if (g != NULL)
        {
        char **iter = g->gr_mem;

        while (*iter != NULL)
          {
          if (strcmp(jowner,*iter) == 0)
            goto perm_done;

          iter++;
          }
        }

      if (is_users_in_group(group,jowner) != 0)
        goto perm_done;

      jinfo->can_never_run = 1;
      ret = CLUSTER_PERMISSIONS; /* if reached, then the user doesn't have permission */

perm_done:

      free(owner);
      free(group);

      if (ret != 0)
        return ret;
  }

  if (jinfo->cluster_mode == ClusterCreate) {
      cluster = xpbs_cache_get_local (jinfo -> custom_name, "cluster");
      if (cluster != NULL)
        ret=CLUSTER_RUNNING;
  }

  if (cluster)
      free(cluster);

  free(jowner);

  return ret;
}
