#include "data_types.h"
#include "assertions.h"
#include "node_info.h"
#include "job_info.h"
#include "misc.h"
#include "globals.h"
#include "nodespec.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assertions.h>
#include "site_pbs_cache.h"

/* get the number of requested virtual cpus */
int get_req_vps(pars_spec_node *spec)
  {
  pars_prop *iter = spec->properties;

  while (iter != NULL)
    {
    if ((strcmp(iter->name,"ppn") == 0) && (iter->value != NULL))
      {
      return atoi(iter->value);
      }

    iter = iter->next;
    }

  return 1;
  }

enum ResourceCheckMode { MaxOnly, Avail };

static int node_has_enough_np(node_info *ninfo, char *value, enum ResourceCheckMode mode)
  {
#if 0
  long amount = res_to_num(value);
  switch(mode)
    {
    case MaxOnly:
      if (ninfo->np >= amount)
        return 1;
      break;
    case Avail:
      if (ninfo->np - ninfo->npshared >= amount &&
          ninfo->npfree >= amount)
        return 1;
      else
        return 0;
      break;
    }
  return 0; /* default is no */
#endif
  return 1; /* np support turned of for a while */
  }

static int node_has_enough_resource(node_info *ninfo, char *name, char *value,
    enum ResourceCheckMode mode)
  {
  struct resource *res;
  int i;
  long amount = 0;

  for (i = 0; i < num_res; i++)
    {
    if (strcmp(name,res_to_check[i].name) == 0)
      break;
    }

  if (i == num_res) /* not checking this resource */
    {
    return 1;
    }

  if ((res = find_resource(ninfo->res, name)) == NULL)
    return 0;

  if (res->is_string)
    {
    /* string resources work kind of like properties right now */
    if (strcmp(res->str_avail,value) == 0)
      return 1;
    }

  amount = res_to_num(value);

  switch (mode)
    {
    case MaxOnly:
      /* there is no maximum (only avail), and we therefore can't check suitability,
       * so lets assume it is suitable. The maximum is infinity, then its definitely suitable,
       * or the maximum is more then we need.
       */
      if (res->max == UNSPECIFIED  || res->max == INFINITY || res->max >= amount)
        return 1;
      break;
    case Avail:
      if (res->max == INFINITY || res->max == UNSPECIFIED)
        /* we only have the avail value */
        {
        if (res->avail >= amount)
          return 1;
        }
      else
        {
        /* we don't have avail - only max and assigned */
        if (res->max - res->assigned >= amount)
          return 1;
        }
      break;
    }
  return 0; /* by default, the node does not have enough */
  }

int get_node_has_prop(node_info *ninfo, pars_prop* property, int preassign_starving)
  {
  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  if (property->value == NULL) /* property */
    {
    char **iter;

    /* the property is not a resource */
    iter = ninfo->properties;

    if (iter == NULL)
      return 0;

    while (*iter != NULL)
      {
      if (strcmp(*iter,property->name) == 0)
        return 1;

      iter++;
      }
    }
  else /* resource or ppn */
    {
    if (strcmp("ppn",property->name) == 0)
      {
      return node_has_enough_np(ninfo, property->value,
               preassign_starving?MaxOnly:Avail);
      }
    else
      {
      return node_has_enough_resource(ninfo,property->name, property->value,
               preassign_starving?MaxOnly:Avail);
      }
    }

  return 0;
  }

int get_node_has_property(node_info *ninfo, const char* property)
  {
  char **iter;
  char *delim = NULL;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");
  dbg_precondition(ninfo != NULL, "This functions does not accept NULL.");

  printf("DEBUG: Checking property %s\n",property);

  /* resource/ppn support */
  if ((delim = strchr(property,'=')) != NULL)
    {
    long amount = 0;
    char *name;

    name = strndup(property,delim-property);
    delim++;

    amount = res_to_num(delim);
    dbg_consistency(amount > 0,"resource conversion to number should not fail");

    if (!(strcmp("ppn",name)))
      {
      if (ninfo->np - ninfo->npshared >= amount &&
          ninfo->npfree >= amount)
        return 1;
      else
        return 0;
      }
    else /* generic resource */
      {
      struct resource *res;
      int i;

      printf("DEBUG Checking generic resource %s\n",property);

      for (i = 0; i < num_res; i++)
        {
        if (!(strcmp(name,res_to_check[i].name)))
          break;
        }

      if (i == num_res) /* not checking this resource */
        {
        printf("DEBUG: Resource not set to be checked\n");
        return 1;
        }

      res = find_resource(ninfo->res, res_to_check[i].name);

      if (res == NULL)
        return 0;

      if (res->max - res->assigned >= amount)
        return 1;
      }

    free(name);
    }

  /* the property is not a resource */
  iter = ninfo->properties;

  if (iter == NULL)
    return 0;

  while (*iter != NULL)
    {
    if (strcmp(*iter,property) == 0)
      {
      return 1;
      }

    iter++;
    }
  return 0;
  }

static int assign_node(job_info *jinfo, pars_spec_node *spec, int exclusivity,
                       int avail_nodes, node_info **ninfo_arr, int preassign_starving)
  {
  int i, ret = 1;
  pars_prop *iter = spec->properties;

  for (i = 0; i < avail_nodes; i++)
    {
    if (jinfo->is_multinode && ninfo_arr[i]->no_multinode_jobs)
      continue; /* multinode job cannot run on non-multinode nodes */

    if ((ninfo_arr[i]->starving_job != NULL) && /* if already assigned to a starving job */
        (strcmp(ninfo_arr[i]->starving_job->name,jinfo->name) != 0) && /* and not this job */
        (ninfo_arr[i]->starving_job->queue->priority >= jinfo->queue->priority)) /* job priority is not higher */
      continue; /* skip this node */

    if (ninfo_arr[i]->temp_assign != NULL)
      continue; /* skip already assigned nodes */
    /* TODO add support for one node to be used by several parts of the spec */

    if (preassign_starving == 0) /* only for non-starving jobs */
    if ((exclusivity == 1) && (ninfo_arr[i]->npfree != ninfo_arr[i]->np))
      continue; /* skip nodes with running jobs for exclusive requests */

    while (iter != NULL)
      {
      if (get_node_has_prop(ninfo_arr[i],iter,preassign_starving) == 0)
        break;

      iter = iter->next;
      }

    if (iter != NULL)
      continue;

    if (site_user_has_account(jinfo->account,ninfo_arr[i]->name,ninfo_arr[i]->cluster_name) == CHECK_NO)
      break;

    if (preassign_starving)
      ninfo_arr[i]->starving_job = jinfo;
    else
      ninfo_arr[i]->temp_assign = clone_pars_spec_node(spec);

    ret = 0;
    break;
    }

  return ret;
  }

int check_nodespec(job_info *jinfo, int nodecount, node_info **ninfo_arr, int preassign_starving)
  {
  const char *node_spec;
  resource_req *res;
  int missed_nodes = 0;

  res = find_resource_req(jinfo -> resreq, "nodes");
  if (res != NULL)
    node_spec = res->res_str;

  if (res != NULL && node_spec != NULL)
    {
    pars_spec *spec;
    pars_spec_node *iter;

    if ((spec = parse_nodespec(node_spec)) == NULL)
      return SCHD_ERROR;

    jinfo->is_exclusive = spec->is_exclusive;
    jinfo->is_multinode = (spec->total_nodes > 1)?1:0;

    iter = spec->nodes;
    while (iter != NULL)
      {
      unsigned i, tot_nodes;

      if (preassign_starving == 1)
        tot_nodes = nodecount;
      else
        tot_nodes = iter->node_count;

      for (i = 0; i < iter->node_count; i++)
        {
        missed_nodes += assign_node(jinfo, iter, spec->is_exclusive, nodecount,
                                    ninfo_arr, preassign_starving);
        }

      iter = iter->next;
      }

    if (preassign_starving == 0)
    if (missed_nodes > 0) /* try reassigns */
      {
      nodes_preassign_clean(ninfo_arr,nodecount);
      return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
      }
    }

  return SUCCESS; /* if we reached this point, we are done */
  }

/** Clean job assignments
 *
 * @param ninfo_arr
 */
void nodes_preassign_clean(node_info **ninfo_arr, int count)
  {
  int i;

  if (ninfo_arr == NULL)
    return;

  for (i = 0; i < count; i++)
    {
    dbg_consistency(ninfo_arr[i] != NULL, "Given count/real count mismatch");

    if (ninfo_arr[i]->temp_assign != NULL)
      free_pars_spec_node(&ninfo_arr[i]->temp_assign);

    ninfo_arr[i]->temp_assign = NULL;
    }
  }

/** Construct a full target node specification (hostname:nodespec)
 *
 * @note Expects a valid node info
 */
static char *get_target_full(node_info *ninfo)
  {
  char *str = NULL, *cp;
  pars_prop *iter;
  int len = 0;

  dbg_precondition(ninfo != NULL, "This function does not accept NULL.");

  if (ninfo->temp_assign == NULL)
    return NULL;

  len += strlen(ninfo->name) + 1;

  iter = ninfo->temp_assign->properties;

  while (iter != NULL)
    {
    len += strlen(iter->name) + 1; /* :prop */
    if (iter->value != NULL)
      len += strlen(iter->value) + 1; /* =value */
    iter = iter->next;
    }

  if ((str = malloc(len)) == NULL)
    return NULL;

  iter = ninfo->temp_assign->properties;
  cp = str;

  strcpy(cp,ninfo->name);
  cp += strlen(ninfo->name);

  while (iter != NULL)
    {
    strcpy(cp,":"); cp++;
    strcpy(cp,iter->name); cp += strlen(iter->name);
    if (iter->value != NULL)
      {
      strcpy(cp,"="); cp++;
      strcpy(cp,iter->value); cp += strlen(iter->value);
      }
    iter = iter->next;
    }

  return str;
  }

/** Get the target string from preassigned nodes
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string(job_info *jinfo, node_info **ninfo_arr, int count)
  {
  char *str = NULL;
  int i;

  if (ninfo_arr == NULL)
    return str;

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->temp_assign != NULL)
      {
      if (str == NULL)
        {
        str = get_target_full(ninfo_arr[i]);

        if (str == NULL) /* alloc failure */
          return NULL;
        }
      else
        {
        char *tmp;
        char *append;

        /* get the full spec for this node */
        append = get_target_full(ninfo_arr[i]);
        if (append == NULL)
          {
          free(str);
          return NULL;
          }

        tmp = malloc(strlen(str)+strlen(append)+2);
        if (tmp == NULL) /* alloc failure */
          {
          free(append);
          free(str);
          return NULL;
          }

        sprintf(tmp,"%s+%s",str,append);
        free(append);
        free(str);
        str = tmp;
        }
      }
    }

  dbg_consistency(str != NULL, "At this point, the target must be set.");

  if (!jinfo->is_exclusive)
    {
    char *tmp = malloc(strlen(str)+strlen("#shared")+1);
    if (tmp == NULL)
      return NULL;

    sprintf(tmp,"%s%s",str,"#shared");
    free(str);
    str = tmp;
    }
  else
    {
    char *tmp = malloc(strlen(str)+strlen("#excl")+1);
    if (tmp == NULL)
      return NULL;

    sprintf(tmp,"%s%s",str,"#excl");
    free(str);
    str = tmp;

    }

  return str;
  }
