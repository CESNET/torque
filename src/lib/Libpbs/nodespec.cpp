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

  if (prop == NULL || *prop == NULL)
    return;

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
  spec->scratch = 0;
  spec->scratch_type = ScratchNone;

  return spec;
  }

void set_node_mem(pars_spec_node *node, unsigned long long value)
  {
  node->mem = value;
  free(node->mem_str);
  stringstream s;
  s << node->mem << "KB";
  node->mem_str = strdup(s.str().c_str());
  }

void set_node_vmem(pars_spec_node *node, unsigned long long value)
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

  result->scratch = node->scratch;
  result->scratch_type = node->scratch_type;

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

  if (node == NULL || *node == NULL)
    return;

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

int str_res_to_num(const char *res, unsigned long long *value)
  {
  unsigned long long result = 0;
  char *tail;

  result = strtoll(res,&tail,10);

  if (tail == res)
    return 1;

  switch (*tail)
    {
    case ':': /* time resource */
      {
      unsigned minutes;
      char *tail2;

      minutes = strtoll(tail+1,&tail2,10);

      if (tail2 == tail+1)
        return 1;

      if (*tail2 == ':')
        {
        unsigned seconds;
        char *tail3;

        seconds = strtoll(tail2+1,&tail3,10);

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
    case 'T' : case 't':
      {
      result *= 1024*1024*1024;
      break;
      }
    case 'B': case 'b':
      {
      result += 1023; // ensure rounding up
      result /= 1024;
      break;
      }
    case 'w':
      {
      result += 1024/SIZE_OF_WORD - 1; // ensure rounding up
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
      unsigned long long value;
      str_res_to_num(prop->value,&value);
      set_node_mem(result,value);
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"vmem") == 0 && prop->value != NULL)
      {
      unsigned long long value;
      str_res_to_num(prop->value,&value);
      set_node_vmem(result,value);
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"scratch_type") == 0)
      {
      if (strcmp(prop->value,"any") == 0)
        result->scratch_type = ScratchAny;
      else if (strcmp(prop->value,"ssd") == 0)
        result->scratch_type = ScratchSSD;
      else if (strcmp(prop->value,"shared") == 0)
        result->scratch_type = ScratchShared;
      else if (strcmp(prop->value,"local") == 0)
        result->scratch_type = ScratchLocal;
      else
        result->scratch_type = ScratchNone;
      free_pars_prop(&prop);
      }
    else if (strcmp(prop->name,"scratch_volume") == 0)
      {
      str_res_to_num(prop->value,&result->scratch);
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
  if (nodespec == NULL)
    return;

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
  if (node->mem != 0)
    s << ":mem=" << node->mem << "KB";
  if (node->vmem != 0)
    s << ":vmem=" << node->vmem << "KB";

  if (node->host != NULL)
    s << ":host=" << node->host;

  if (node->alternative != NULL && with_alter == with_alternative)
    s << ":alternative=" << node->alternative;

  if (node->scratch_type != ScratchNone)
    {
    if (node->scratch_type == ScratchAny) { s << ":scratch_type=any"; }
    if (node->scratch_type == ScratchSSD) { s << ":scratch_type=ssd"; }
    if (node->scratch_type == ScratchLocal) { s << ":scratch_type=local"; }
    if (node->scratch_type == ScratchShared) { s << ":scratch_type=shared"; }
    s << ":scratch_volume=" << (node->scratch + 1023) / 1024 << "mb";
    }

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

  first = true;
  if (nodespec->global != NULL)
    s << "#";

  pars_prop *prop = nodespec->global;
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
        unsigned long long value;
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
        unsigned long long value;
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
  pars_prop *prop = spec->global;

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

pars_spec_node* find_node_in_spec(pars_spec *nodespec, const char* name)
  {
	pars_spec_node *node = nodespec->nodes;
	while(node != NULL)
	  {
		if (node->host != NULL)
		if (strcmp(node->host,name) == 0)
			return node;

		pars_prop *prop = node->properties;

		while (prop != NULL)
		{
			if (strcmp(prop->name,name) == 0)
				return node;

			prop = prop->next;
		}

		node = node->next;
	  }
	return NULL;
  }

void add_scratch_to_nodespec(pars_spec *spec, char *scratch)
  {
  pars_spec_node *node = spec->nodes;

  while (node != NULL)
    {
    node->scratch = 0;
    node->scratch_type = ScratchNone;
    node = node->next;
    }

  bool first_only = false;
  enum ScratchType scratch_type = ScratchAny;
  unsigned long long scratch_size = 0;

  char *delim1 = NULL;
  char *delim2 = NULL;

  char *value = strdup(scratch);
  if (value == NULL)
    return;

  delim1 = strchr(value,':');
  if (delim1 != NULL)
    {
    *delim1 = '\0';
    ++delim1;
    }

  if (str_res_to_num(value,&scratch_size) != 0)
      return;

  free(value);

  if (scratch_size == 0)
    return;

  if (delim1 == NULL)
    goto finished;

  delim2 = strchr(delim1,':');
  if (delim2 != NULL)
    {
    *delim2 = '\0';
    ++delim2;
    }

  if (strcmp(delim1,"local") == 0)
    scratch_type = ScratchLocal;
  else if (strcmp(delim1,"shared") == 0)
    scratch_type = ScratchShared;
  else if (strcmp(delim1,"ssd") == 0)
    scratch_type = ScratchSSD;
  else if (strcmp(delim1,"first") == 0)
    first_only = true;

  if (delim2 == NULL)
    goto finished;

  if (strcmp(delim2,"local") == 0)
    scratch_type = ScratchLocal;
  else if (strcmp(delim2,"shared") == 0)
    scratch_type = ScratchShared;
  else if (strcmp(delim2,"ssd") == 0)
    scratch_type = ScratchSSD;
  else if (strcmp(delim2,"first") == 0)
    first_only = true;

finished:
  node = spec->nodes;
  while (node != NULL)
    {
    node->scratch = scratch_size;
    node->scratch_type = scratch_type;

    if (first_only)
      {
      /* if the first node is for multiple nodes, create a new node in the nodespec */
      if (node->node_count > 1)
        {
        pars_spec_node *newnode = clone_pars_spec_node(node);

        newnode->scratch = 0;
        newnode->scratch_type = ScratchNone;

        pars_spec_node *next = node->next;

        if (next != NULL)
          next->prev = newnode;

        node->next = newnode;
        newnode->next = next;
        newnode->prev = node;

        newnode->node_count -= 1;
        node->node_count = 1;
        }
      break;
      }
    node = node->next;
    }
  }
