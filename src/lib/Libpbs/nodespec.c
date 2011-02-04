/*
(c) Simon Toth 2010 for CESNET

Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "assertions.h"
#include "nodespec.h"

static const char *excl = "excl";

/** Alloc and init a new parsed property instance
 *
 * @return Allocated instance or NULL on error
 */
pars_prop *init_pars_prop()
  {
  pars_prop *prop = (pars_prop*)malloc(sizeof(pars_prop));
  if (prop == NULL)
    return NULL;

  prop->name = NULL;
  prop->value = NULL;
  prop->next = NULL;
  prop->prev = NULL;

  return prop;
  }

/** Free one parsed property from the list of properties
 *
 * @param prop List of properties
 */
void free_pars_prop(pars_prop **prop)
  {
  pars_prop *tmp;

  dbg_precondition(prop != NULL && (*prop) != NULL,
      "This function does not accept NULL param");

  tmp = *prop;
  /* keep the list linked */
  if (tmp->prev != NULL)
    {
    tmp->prev->next = tmp->next;
    }
  if (tmp->next != NULL)
    {
    tmp->next->prev = tmp->prev;
    }

  *prop = (*prop)->next;

  free(tmp->name);
  free(tmp->value);
  free(tmp);
  }

/** Clone parsed properties
 *
 * @param prop Properties to clone
 * @return Cloned properties or NULL on error
 */
pars_prop *clone_pars_prop(pars_prop *prop, pars_prop **prop_last)
  {
  pars_prop *iter = prop, *result = NULL, *last = NULL;
  while (iter != NULL)
    {
    pars_prop *tmp;

    if ((tmp = init_pars_prop()) == NULL)
      {
      free_pars_prop(&result);
      return NULL;
      }

    if ((tmp->name = strdup(iter->name)) == NULL)
      {
      free_pars_prop(&result);
      return NULL;
      }

    if (iter->value != NULL)
    if ((tmp->value = strdup(iter->value)) == NULL)
      {
      free(tmp->name);
      free_pars_prop(&result);
      return NULL;
      }

    tmp->prev = last;
    if (result == NULL)
      {
      result = tmp;
      last = result;
      if (prop_last != NULL)
        *prop_last = last;
      }
    else
      {
      last->next = tmp;
      last = tmp;
      if (prop_last != NULL)
        *prop_last = last;
      }
    iter = iter->next;
    }

  return result;
  }

/** Parse a text property representation
 *
 * @param prop Property
 * @return Parsed property or NULL on error
 */
pars_prop *parse_prop(char *prop)
  {
  char *delim;
  pars_prop *result = init_pars_prop();

  if (result == NULL)
    return NULL;

  if ((delim = strchr(prop,'=')) != NULL)
    {
    *delim = '\0';
    delim++;
    result->value = strdup(delim);

    if (result->value == NULL)
      return NULL;
    }

  result->name = strdup(prop);
  if (result->name == NULL)
    {
    if (result->value != NULL)
      free(result->value);

    return NULL;
    }

  return result;
  }

/** Alloc and init a new parsed one node spec instance
 *
 * @return Allocated instance or NULL on error
 */
pars_spec_node *init_pars_spec_node()
  {
  pars_spec_node *spec = (pars_spec_node*)malloc(sizeof(pars_spec_node));
  if (spec == NULL)
    return NULL;

  spec->alternative = NULL;
  spec->node_count = 0;
  spec->procs = 0;
  spec->host = NULL;
  spec->next = NULL;
  spec->prev = NULL;
  spec->properties = NULL;
  spec->properties_end = NULL;

  return spec;
  }

pars_spec_node *clone_pars_spec_node(pars_spec_node *node)
  {
  pars_spec_node *result;
	if ((result = init_pars_spec_node()) == NULL)
    return NULL;

  result->node_count = node->node_count;
  if (node->alternative != NULL)
    result->alternative = strdup(node->alternative);
  if (node->host != NULL)
    result->host = strdup(node->host);

  if ((result->properties = clone_pars_prop(node->properties,&result->properties_end)) == NULL &&
      node->properties != NULL)
    {
    free_pars_spec_node(&result);
	  return NULL;
    }

  return result;
  }

/** Free one node spec from the list of node specs
 *
 * @param node List of node specs
 */
void free_pars_spec_node(pars_spec_node **node)
  {
  pars_spec_node *tmp;

  dbg_precondition(node != NULL && (*node) != NULL,
        "This function does not accept NULL param");

  free((*node)->alternative);
  (*node)->alternative = NULL;
  free((*node)->host);
  (*node)->host = NULL;

  while ((*node)->properties != NULL)
    {
    free_pars_prop(&(*node)->properties);
    }

  tmp = *node;
  if (tmp->prev != NULL)
    {
    tmp->prev->next = tmp->next;
    }
  if (tmp->next != NULL)
    {
    tmp->next->prev = tmp->prev;
    }
  *node = (*node)->next;
  free(tmp);
  }

/** Parse a text representation of one node spec
 *
 * @param node One node spec
 * @return Parsed one node spec or NULL on error
 */
pars_spec_node *parse_spec_node(char *node)
  {
  char *iter, *delim;
  pars_prop *end = NULL;
  pars_spec_node *result;

  /* nodespec can start with a number */
  int count = strtol(node,&iter,10);
  if (*iter == ':' && count > 0) /* ok */
    {
    iter++;
    }
  else if (*iter == '\0' && count > 0)
    {
    result = init_pars_spec_node();
    if (result == NULL)
      return NULL;

    result->node_count = count;
    result->procs = 1;

    return result;
    }
  else
    {
    count = 1;
    }

  result = init_pars_spec_node();
  if (result == NULL)
    return NULL;

  result->node_count = count;
  result->procs = 1;
  delim = iter;

  while (delim != NULL)
    {
    pars_prop *prop;

    delim = strchr(iter,':');
    if (delim != NULL)
      {
      *delim = '\0';
      delim++;
      }

    prop = parse_prop(iter);
    if (prop == NULL)
      {
      free_pars_spec_node(&result);
      return NULL;
      }

    /* special handle for alternative=image */
    if (strcmp(prop->name,"alternative") == 0 && prop->value != NULL)
      {
      result->alternative = prop->value;
      prop->value = NULL;
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"host") == 0 && prop->value != NULL)
      {
      result->host = prop->value;
      prop->value = NULL;
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"ppn") == 0 && prop->value != NULL)
      {
      result->procs = atoi(prop->value);
      free_pars_prop(&prop);
      }
    else
      {
      if (result->properties == NULL)
        {
        result->properties = prop;
        end = prop;
        result->properties_end = end;
        }
      else
        {
        end->next = prop;
        prop->prev = end;
        end = prop;
        result->properties_end = end;
        }
      }

    iter = delim;
    }

  return result;
  }

/** Alloc and init a new parsed spec instance
 *
 * @return Allocated instance or NULL on error
 */
pars_spec *init_pars_spec()
  {

  pars_spec *spec = (pars_spec*)malloc(sizeof(pars_spec));
  if (spec == NULL)
    return NULL;

  spec->is_exclusive = 0;
  spec->nodes = NULL;
  spec->nodes_end = NULL;
  spec->global = NULL;
  spec->global_end = NULL;
  spec->total_nodes = 0;
  spec->total_procs = 0;

  return spec;
  }

void free_parsed_nodespec(pars_spec *nodespec)
  {
  dbg_precondition(nodespec != NULL,"This function requires a non-null param.");

  while (nodespec->nodes != NULL)
    {
    free_pars_spec_node(&nodespec->nodes);
    }

  while (nodespec->global != NULL)
    {
    free_pars_prop(&nodespec->global);
    }

  free(nodespec);
  }

pars_spec *parse_nodespec(const char *nodespec)
  {
  pars_spec *result = NULL;
  pars_spec_node *last = NULL;
  char *spec = NULL, *global = NULL, *iter = NULL, *delim = NULL;

  spec = strdup(nodespec);
  if (spec == NULL)
    return NULL;

  result = init_pars_spec();

  /* parse global part of the nodespec */
  global = strchr(spec,'#');
  if (global != NULL)
    {
    pars_prop *curr = NULL, *end = NULL;
    global[0] = '\0';

    do
      {
      global++;
      iter = strpbrk(global,"#:");
      if (iter != NULL)
        iter[0] = '\0';

      if (strcmp(global,excl) == 0)
        { /* exclusive access */
        result->is_exclusive = 1;
        }
      else
        { /* properties */
        curr = parse_prop(global);
        if (curr == NULL)
          {
          free(spec);
          free_parsed_nodespec(result);
          return NULL;
          }

        if (result->global == NULL)
          {
          result->global = curr;
          end = curr;
          result->global_end = end;
          }
        else
          {
          end->next = curr;
          curr->prev = end;
          end = curr;
          result->global_end = end;
          }
        }

      global = iter;
      }
    while (iter != NULL);

    }

  /* parse nodes */
  iter = spec;
  delim = iter;

  while (delim != NULL)
    {
    pars_spec_node *node;

    delim = strchr(iter,'+');
    if (delim != NULL)
      {
      *delim = '\0';
      delim++;
      }

    node = parse_spec_node(iter);
    if (node == NULL)
      {
      free(spec);
      free_parsed_nodespec(result);
      return NULL;
      }

    if (result->nodes == NULL)
      {
      last = node;
      result->nodes = node;
      result->nodes_end = last;
      }
    else
      {
      node->prev = last;
      last->next = node;
      last = node;
      result->nodes_end = last;
      }

    result->total_nodes += node->node_count;
    result->total_procs += node->node_count*node->procs;
    iter = delim;
    }

  free(spec);
  return result;
  }


int concat_prop(pars_prop *prop, char *buff, int buff_size, int sep)
  {
  int req_size = sep;

  dbg_precondition(prop != NULL && buff != NULL,"This function requires a non-null param.");

  req_size += strlen(prop->name);
  if (prop->value != NULL)
    {
    req_size += 1; /* = */
    req_size += strlen(prop->value);
    }

  if (req_size >= buff_size)
    return -1;
  else
    {
    if (sep) strcat(buff,":");
    strcat(buff,prop->name);
    if (prop->value != NULL)
      {
      strcat(buff,"=");
      strcat(buff,prop->value);
      }
    return req_size;
    }
  }

int concat_node(pars_spec_node *node, char *buff, int buff_size)
  {
  pars_prop *prop = node->properties;
  int old_size = buff_size, curr_size = buff_size, diff = 0;

  dbg_precondition(node != NULL && buff != NULL,"This function requires a non-null param.");

  if ((diff = snprintf(buff,curr_size,"%d:ppn=%d",node->node_count,node->procs)) >= curr_size)
    return -1;
  curr_size -= diff;
  buff += diff;

  if (node->host != NULL)
    {
    if ((diff = snprintf(buff,curr_size,":host=%s",node->host)) >= curr_size)
      return -1;
    curr_size -= diff;
    buff += diff;
    }

  if (node->alternative != NULL)
    {
    if ((diff = snprintf(buff,curr_size,":alternative=%s",node->alternative)) >= curr_size)
      return -1;
    curr_size -= diff;
    buff += diff;
    }

  while (prop != NULL)
    {
    if ((diff = concat_prop(prop,buff,curr_size,1)) < 0)
      return -1;
    curr_size -= diff;
    buff += diff;
    prop = prop->next;
    }

  return old_size-curr_size;
  }


#define CONCAT_BUFF_SIZE (4*1024)

char *concat_nodespec(pars_spec *nodespec)
  {
  pars_spec_node *node;
  pars_prop *prop;
  char *buff, *iter, *result;
  int buff_size = CONCAT_BUFF_SIZE, diff;

  if ((buff = (char*)malloc(CONCAT_BUFF_SIZE)) == NULL)
    {
    return NULL;
    }
  iter = buff;

  memset(buff,0,CONCAT_BUFF_SIZE);

  node = nodespec->nodes;
  while (node != NULL)
    {
    if (node != nodespec->nodes)
      {
      if (buff_size > 1)
        {
        strcat(iter,"+");
        buff_size--;
        iter++;
        }
      else
        {
        free(buff);
        return NULL;
        }
      }

    if ((diff = concat_node(node, iter, buff_size)) < 0)
      { free(buff); return NULL; }

    buff_size -= diff;
    iter += diff;
    node = node->next;
    }

  if (nodespec->is_exclusive)
    {
    if (buff_size > 5)
      {
      strcat(iter,"#excl");
      buff_size--;
      iter++;
      }
    else
      { free(buff); return NULL; }
    }

  if (nodespec->global != NULL)
    {
    if (buff_size > 1)
      {
      strcat(iter,"#");
      buff_size--;
      iter++;
      }
    else
      { free(buff); return NULL; }
    }

  prop = nodespec->global;
  while (prop != NULL)
    {
    if ((diff = concat_prop(prop,iter,buff_size,(prop != nodespec->global))) < 0)
      { free(buff); return NULL; }

    buff_size -= diff;
    iter += diff;
    prop = prop->next;
    }

  result = strdup(buff);
  free(buff);
  return result;
  }

pars_prop* find_parsed_prop(pars_prop *prop, char *name)
  {
  pars_prop *iter = prop;

  while (iter != NULL)
    {
    if (strcmp(iter->name,name) == 0)
      return iter;
    iter = iter->next;
    }
  return NULL;
  }

void add_prop_to_nodespec(pars_spec *spec, pars_prop *prop)
  {
  pars_spec_node *node;
  pars_prop *tmp;

  node = spec->nodes;
  while (node != NULL)
    {
    tmp = clone_pars_prop(prop,NULL);
    if (node->properties_end == NULL && node->properties == NULL)
      {
      node->properties = tmp;
      node->properties_end = tmp;
      }
    else
      {
      if (find_parsed_prop(node->properties,tmp->name) == NULL)
        {
        node->properties_end->next = tmp;
        tmp->prev = node->properties_end;
        node->properties_end = tmp;
        }
      }
    node = node->next;
    }
  }

void add_res_to_nodespec(pars_spec *spec, char* name, char* value)
  {

  pars_prop *prop = init_pars_prop();
  prop->name = strdup(name);
  if (value != NULL)
    {
    prop->value = strdup(value);
    }
  else
    {
    prop->value = NULL;
    }
  add_prop_to_nodespec(spec,prop);
  free_pars_prop(&prop);
  }

void expand_nodespec(pars_spec *spec)
  {
  pars_prop *prop = spec->global, *tmp, *last;
  pars_spec_node *node;

  if (spec->global == NULL)
    return;

  node = spec->nodes;
  while (node != NULL)
    {
    tmp = clone_pars_prop(prop,&last);
    if (node->properties_end == NULL && node->properties == NULL)
      {
      node->properties = tmp;
      node->properties_end = last;
      }
    else
      {
      pars_prop *iter = tmp;

      while (iter != NULL)
        {
        tmp = tmp->next;
        iter->prev = NULL;
        iter->next = NULL;
        if (find_parsed_prop(node->properties,iter->name) == NULL)
          {
          node->properties_end->next = iter;
          iter->prev = node->properties_end;
          node->properties_end = iter;
          }
        else
          {
          free_pars_prop(&iter);
          }
        iter = tmp;
        }
      }
    node = node->next;
    }

  while (spec->global != NULL)
    {
    free_pars_prop(&spec->global);
    }

  spec->global_end = NULL;
  }
