/*
(c) Simon Toth 2010 for CESNET

Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php
*/

extern "C" {
#include "assertions.h"
#include "nodespec.h"
}

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sstream>
using namespace std;

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

pars_prop *clone_one_pars_prop(pars_prop *prop)
  {
  pars_prop *result = init_pars_prop();
  if (result == NULL) return NULL;

  result->name = strdup(prop->name);
  if (result->name == NULL)
    {
    free_pars_prop(&result);
    return NULL;
    }

  if (prop->value != NULL)
    {
    result->value = strdup(prop->value);
    if (result->value == NULL)
      {
      free_pars_prop(&result);
      return NULL;
      }
    }

  return result;
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
    pars_prop *tmp = clone_one_pars_prop(iter);
    if (tmp == NULL)
      {
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
  spec->mem = 0;
  spec->mem_str = NULL;
  spec->vmem = 0;
  spec->vmem_str = NULL;
  spec->host = NULL;
  spec->next = NULL;
  spec->prev = NULL;
  spec->properties = NULL;
  spec->properties_end = NULL;

  return spec;
  }

void set_node_mem(pars_spec_node *node, unsigned value)
  {
  node->mem = value;
  free(node->mem_str);
  stringstream s;
  s << node->mem << "KB";
  node->mem_str = strdup(s.str().c_str());
  }

void set_node_vmem(pars_spec_node *node, unsigned value)
  {
  node->vmem = value;
  free(node->vmem_str);
  stringstream s;
  s << node->vmem << "KB";
  node->vmem_str = strdup(s.str().c_str());
  }

pars_spec_node *clone_pars_spec_node(pars_spec_node *node)
  {
  pars_spec_node *result;
	if ((result = init_pars_spec_node()) == NULL)
    return NULL;

  result->node_count = node->node_count;
  result->procs      = node->procs;
  set_node_mem(result,node->mem);
  set_node_vmem(result,node->vmem);

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
  free((*node)->mem_str);
  (*node)->mem_str = NULL;
  free((*node)->vmem_str);
  (*node)->vmem_str = NULL;

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

#define SIZE_OF_WORD 8

int str_res_to_num(const char *res, unsigned *value)
  {
  unsigned result = 0;
  char *tail;

  result = strtol(res,&tail,10);

  if (tail == res)
    return 1;

  switch (*tail)
    {
    case ':': /* time resource */
      {
      unsigned minutes;
      char *tail2;

      minutes = strtol(tail+1,&tail2,10);

      if (tail2 == tail+1)
        return 1;

      if (*tail2 == ':')
        {
        unsigned seconds;
        char *tail3;

        seconds = strtol(tail2+1,&tail3,10);

        if (tail3 == tail2+1)
          return 1;

        if (*tail3 != '\0')
          return 1;

        *value = result*60*60 + minutes*60 + seconds;
        return 0;
        }
      else if (*tail2 == '\0') /* MM:SS format */
        {
        *value = result*60 + minutes;
        }
      else
        return 1;

      break;
      }
    case 'K': case 'k':
      {
      /* keep */
      break;
      }
    case 'M': case 'm':
      {
      result *= 1024;
      break;
      }
    case 'G': case 'g':
      {
      result *= 1024*1024;
      break;
      }
    case 'B': case 'b':
      {
      result /= 1024;
      break;
      }
    case 'w':
      {
      result /= 1024/SIZE_OF_WORD;
      break;
      }
    }

  if (tail[0] != '\0' && tail[1] == 'w')
    result *= SIZE_OF_WORD;

  *value = result;

  return 0;
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
    else if (strcmp(prop->name,"mem") == 0 && prop->value != NULL)
      {
      unsigned value;
      str_res_to_num(prop->value,&value);
      set_node_mem(result,value);
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"vmem") == 0 && prop->value != NULL)
      {
      unsigned value;
      str_res_to_num(prop->value,&value);
      set_node_vmem(result,value);
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

pars_spec_node *remove_node_from_nodespec(pars_spec *spec, pars_spec_node *node)
  {
  spec->total_nodes--;
  spec->total_procs -= node->procs;

  if (node->prev != NULL)
    node->prev->next = node->next;
  if (node->next != NULL)
    node->next->prev = node->prev;

  if (node == spec->nodes)
    spec->nodes = node->next;
  if (node == spec->nodes_end)
    spec->nodes_end = node->prev;

  node->next = NULL;
  node->prev = NULL;

  return node;
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

static void concat_prop(stringstream& s, pars_prop *prop, bool add_sep)
  {
  assert(prop != NULL);

  if (add_sep) s << ":";
  s << prop->name;
  if (prop->value != NULL)
    s << "=" << prop->value;
  }

static void concat_node(stringstream& s, pars_spec_node *node, alter_flag with_alter)
  {
  assert(node != NULL);

  s << node->node_count << ":ppn=" << node->procs;
  s << ":mem=" << node->mem << "KB";
  s << ":vmem=" << node->vmem << "KB";

  if (node->host != NULL)
    s << ":host=" << node->host;

  if (node->alternative != NULL && with_alter == with_alternative)
    s << ":alternative=" << node->alternative;

  pars_prop *prop = node->properties;
  while (prop != NULL)
    {
    concat_prop(s,prop,true);
    prop = prop->next;
    }
  }

char *concat_nodespec(pars_spec *nodespec, int with_excl, alter_flag with_alter, const char** ign_props)
  {
  stringstream s;

  pars_spec_node *node = nodespec->nodes;
  bool first = true;
  while (node != NULL)
    {
    if (!first) s << "+";
    first = false;

    concat_node(s,node,with_alter);
    node = node->next;
    }

  if (with_excl && nodespec->is_exclusive)
    s << "#excl";

  if (nodespec->global != NULL)
    s << "#";

  pars_prop *prop = nodespec->global;
  first = true;
  while (prop != NULL)
    {
    concat_prop(s,prop,!first);
    first = false;
    prop = prop->next;
    }

  return strdup(s.str().c_str());
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
    if (strcmp(prop->name,"mem") == 0)
      {

      if (node->mem == 0)
        {
        unsigned value;
        str_res_to_num(prop->value,&value);
        set_node_mem(node,value);
        }
      node = node->next;
      continue;
      }

    if (strcmp(prop->name,"vmem") == 0)
      {
      if (node->vmem == 0)
        {
        unsigned value;
        str_res_to_num(prop->value,&value);
        set_node_vmem(node,value);
        }
      node = node->next;
      continue;
      }

    tmp = clone_one_pars_prop(prop);
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

  while (prop != NULL)
    {
    add_prop_to_nodespec(spec,prop);
    prop = prop->next;
    }

  while (spec->global != NULL)
    {
    free_pars_prop(&spec->global);
    }

  spec->global_end = NULL;
  }
