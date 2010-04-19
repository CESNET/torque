/* 
(c) Simon Toth 2010 for CESNET

Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "assertions.h"
#include "nodespec.h"

static const char *shared = "shared";
static const char *excl = "excl";

static void free_pars_prop(pars_prop **prop);

/** Count the parts in a nodespec
 *
 * (Only works for local specs)
 *
 * @param spec Nodespec to parse
 * @return Count of parts
 */
static int nodespec_part_count(const char *spec)
  {
  int result = 1;

  dbg_precondition(spec != NULL, "This function does not accept NULL");

  while (*spec != '\0')
    {
    if (*spec == '+')
      result++;
    spec++;
    }

  return result;
  }

/** Append requirements to each part of a spec
 *
 * @param spec the spec to be modified
 * @param app requirements to be appended
 * @return Modified nodespec
 */
static char *nodespec_app(const char *spec, const char *app)
  {
  char *cp, *result;
  unsigned len = nodespec_part_count(spec) * (strlen(app) + 1) + strlen(spec) + 1;
  /* number of local specs * (appended length + ':' ) + lenght of local specs + '\0' */

  result = (char*)malloc(len);
  if (result == NULL) /* alloc fail */
    return NULL;

  cp = result;

  while (*spec)
    {
    if (*spec == '+') /* add the requirements before each '+' */
      {
      *cp = ':'; cp++;

      strcpy(cp, app);

      cp += strlen(app);
      }

    *cp = *spec; cp++; spec++;
    } /* END while (*spec) */

  *cp = ':'; cp++; /* and also after the last part of the spec */

  strcpy(cp, app);

  return(result);
  }  /* END nodespec_app() */


/** Alloc and init a new parsed spec instance
 *
 * @return Allocated instance or NULL on error
 */
static pars_spec *init_pars_spec()
  {

  pars_spec *spec = (pars_spec*)malloc(sizeof(pars_spec));
  if (spec == NULL)
    return NULL;

  spec->is_exclusive = 0;
  spec->nodes = NULL;
  spec->total_nodes = 0;

  return spec;
  }

/** Alloc and init a new parsed one node spec instance
 *
 * @return Allocated instance or NULL on error
 */
static pars_spec_node *init_pars_spec_node()
  {
  pars_spec_node *spec = (pars_spec_node*)malloc(sizeof(pars_spec_node));
  if (spec == NULL)
    return NULL;

  spec->node_count = 0;
  spec->next = NULL;
  spec->properties = NULL;

  return spec;
  }

/** Alloc and init a new parsed property instance
 *
 * @return Allocated instance or NULL on error
 */
static pars_prop *init_pars_prop()
  {
  pars_prop *prop = (pars_prop*)malloc(sizeof(pars_prop));
  if (prop == NULL)
    return NULL;

  prop->name = NULL;
  prop->value = NULL;
  prop->next = NULL;

  return prop;
  }

/** Clone parsed properties
 *
 * @param prop Properties to clone
 * @return Cloned properties or NULL on error
 */
static pars_prop *clone_pars_prop(pars_prop *prop)
  {
  pars_prop *iter = prop, *result = NULL;
  while (iter != NULL)
    {
    pars_prop *tmp = result;

    if ((result = init_pars_prop()) == NULL)
      {
      free_pars_prop(&tmp);
      return NULL;
      }

    if ((result->name = strdup(iter->name)) == NULL)
      {
      free_pars_prop(&tmp);
      return NULL;
      }

    if (iter->value != NULL)
    if ((result->value = strdup(iter->value)) == NULL)
      {
      free(result->name);
      free_pars_prop(&tmp);
      return NULL;
      }

    result->next = tmp;
    iter = iter->next;
    }

  return result;
  }

pars_spec_node *clone_pars_spec_node(pars_spec_node *node)
  {
  pars_spec_node *result;
	if ((result = init_pars_spec_node()) == NULL)
    return NULL;

  result->node_count = node->node_count;
  if ((result->properties = clone_pars_prop(node->properties)) == NULL &&
      node->properties != NULL)
    {
    free_pars_spec_node(&result);
	  return NULL;
    }

  return result;
  }


/** Free one parsed property from the list of properties
 *
 * @param prop List of properties
 */
static void free_pars_prop(pars_prop **prop)
  {
  pars_prop *tmp;

  dbg_precondition(prop != NULL && (*prop) != NULL,
      "This function does not accept NULL param");

  tmp = *prop;
  *prop = (*prop)->next;

  free(tmp->name);
  free(tmp->value);
  free(tmp);
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

  while ((*node)->properties != NULL)
    {
    free_pars_prop(&(*node)->properties);
    }

  tmp = *node;
  *node = (*node)->next;
  free(tmp);
  }

/** Parse a text property representation
 *
 * @param prop Property
 * @return Parsed property or NULL on error
 */
static pars_prop *parse_prop(char *prop)
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

/** Parse a text representation of one node spec
 *
 * @param node One node spec
 * @return Parsed one node spec or NULL on error
 */
static pars_spec_node *parse_spec_node(char *node)
  {
  char *iter, *delim;
  pars_spec_node *result;

  /* nodespec can start with a number */
  int count = strtol(node,&iter,10);
  if (*iter == ':' && count > 0) /* ok */
    iter++;
  else if (*iter == '\0' && count > 0)
    {
    result = init_pars_spec_node();
    if (result == NULL)
      return NULL;

    result->node_count = count;

    return result;
    }
  else
    count = 1;

  result = init_pars_spec_node();
  if (result == NULL)
    return NULL;

  result->node_count = count;
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

    prop->next = result->properties;
    result->properties = prop;

    iter = delim;
    }

  return result;
  }

pars_spec *parse_nodespec(const char *nodespec)
  {
  int is_exclusive;
  char *expanded, *iter, *delim;
  pars_spec *parsed;
  pars_spec_node *node;

  expanded = expand_nodespec(nodespec,&is_exclusive);

  if (expanded == NULL)
    return NULL;

  if ((parsed = init_pars_spec()) == NULL)
  {
    free(expanded);
    return NULL;
  }

  parsed->is_exclusive = is_exclusive;

  iter = expanded;
  delim = iter;

  while (delim != NULL)
    {
    delim = strchr(iter,'+');
    if (delim != NULL)
      {
      *delim = '\0';
      delim++;
      }

    node = parse_spec_node(iter);
    if (node == NULL)
      {
      free(expanded);
      free_parsed_nodespec(parsed);
      return NULL;
      }

    node->next = parsed->nodes;
    parsed->nodes = node;
    parsed->total_nodes += node->node_count;
    iter = delim;
    }

  free(expanded);
  return parsed;
  }

void free_parsed_nodespec(pars_spec *nodespec)
  {
  dbg_precondition(nodespec != NULL,"This function requires a non-null param.");

  while (nodespec->nodes != NULL)
    {
    free_pars_spec_node(&nodespec->nodes);
    }

  free(nodespec);
  }

char *expand_nodespec(const char* nodespec, int *is_exclusive)
  {
  char *result, *globs;

  dbg_precondition(nodespec != NULL, "This function does not accept NULL nodespec.");

  if (is_exclusive != NULL)
    *is_exclusive = NODESPEC_DEFAULT_EXCLUSIVE;

  if ((result = strdup(nodespec)) == NULL) /* memory alloc fail */
    return NULL;

  if ((globs = strchr(result, '#')) != NULL) /* is there a global part? */
    {
    char *cp, *tmp;

    *globs++ = '\0';

    if ((globs = strdup(globs)) == NULL) /* memory alloc fail */
      {
      free(result);
      return NULL;
      }

    /* now parse all global parts */
    while ((cp = strrchr(globs, '#')) != NULL)
      {
      *cp++ = '\0';

      if (!strcmp(cp, shared)) /* #shared */
        {
        if (is_exclusive != NULL)
          *is_exclusive = 0;
        continue;
        }

      if (!strcmp(cp, excl)) /* #excl */
        {
        if (is_exclusive != NULL)
          *is_exclusive = 1;
        continue;
        }

      tmp = nodespec_app(result, cp);
      if (tmp == NULL) /* alloc failure */
        {
        free(result);
        free(globs);
        return NULL;
        }

      free(result);
      result = tmp;
      } /* END while ((cp = strrchr(globs, '#')) != NULL) */

    /* now parse the first part of the global nodespec */
    if (!strcmp(globs, shared)) /* #shared */
      {
      if (is_exclusive != NULL)
        *is_exclusive = 0;
      free(globs);
      return result;
      }

    if (!strcmp(globs, excl)) /* #excl */
      {
      if (is_exclusive != NULL)
        *is_exclusive = 1;
      free(globs);
      return result;
      }

    tmp = nodespec_app(result, globs);
    if (tmp == NULL) /* alloc failure */
      {
      free(result);
      free(globs);
      return NULL;
      }

    free(result);
    result = tmp;

    free(globs);
    }  /* END if ((globs = strchr(spec,'#')) != NULL) */

  return result;
  }
