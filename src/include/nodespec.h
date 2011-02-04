/* 
(c) Simon Toth 2010 for CESNET

Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php
*/
#ifndef PARSED_NODESPEC_H_
#define PARSED_NODESPEC_H_

/** Default mode for exclusivity
 *
 *   0 - shared by default
 *   1 - exclusive by default
 *
 *   @see expand_nodespec()
 */
#define NODESPEC_DEFAULT_EXCLUSIVE 0

struct pars_prop;
struct pars_spec_node;
struct pars_spec;

struct pars_prop
  {
  char *name; /**< property name */
  char *value; /**< property value */
  struct pars_prop *next;
  struct pars_prop *prev;
  };

struct pars_spec_node
  {
  unsigned node_count; /**< count of nodes requested for this spec */
  char *alternative; /**< selected alternative name (if present) */
  char *host; /**< selected host name (if present) */
  unsigned procs; /**< count of procs */
  struct pars_prop *properties;
  struct pars_prop *properties_end;
  struct pars_spec_node *next;
  struct pars_spec_node *prev;
  };

struct pars_spec
  {
  unsigned is_exclusive : 1; /**< exclusive request flag */
  struct pars_spec_node *nodes;
  struct pars_spec_node *nodes_end;
  struct pars_prop *global;
  struct pars_prop *global_end;
  unsigned total_nodes;
  unsigned total_procs;
  };

typedef struct pars_prop pars_prop;
typedef struct pars_spec_node pars_spec_node;
typedef struct pars_spec pars_spec;

pars_prop *init_pars_prop();
void free_pars_prop(pars_prop **prop);
pars_prop *clone_pars_prop(pars_prop *prop, pars_prop **prop_last);
pars_prop *parse_prop(char *prop);

pars_spec_node *init_pars_spec_node();

/** Clone the selected one node spec
 *
 * @param node Node spec to be cloned
 * @return Copy of the spec
 */
pars_spec_node *clone_pars_spec_node(pars_spec_node *node);
void free_pars_spec_node(pars_spec_node **node);
pars_spec_node *parse_spec_node(char *node);

pars_spec *init_pars_spec();

/** Parse text nodespec representation
 *
 * @param nodespec Text nodespec representation
 * @return Parsed nodespec or NULL on error
 */
pars_spec *parse_nodespec(const char *nodespec);

/** Free a parsed nodespec
 *
 * @param nodespec Parsed nodespec to be freed
 */
void free_parsed_nodespec(pars_spec *nodespec);
/*pars_spec *clone_pars_spec(pars_spec *spec, pars_spec_node **node_last, pars_prop **prop_last);*/

int concat_prop(pars_prop *prop, char *buff, int buff_size, int sep);
int concat_node(pars_spec_node *node, char *buff, int buff_size);

void expand_nodespec(pars_spec *spec);
void add_prop_to_nodespec(pars_spec *spec, pars_prop *prop);
void add_res_to_nodespec(pars_spec *spec, char* name, char* value);

pars_prop* find_parsed_prop(pars_prop *prop, char *name);

/** Concat the parsed nodespec back to the string representation
 *
 * @param nodespec Node spec to be concated
 * @return concated nodespec
 */
char *concat_nodespec(pars_spec *nodespec);

#endif /* NODESPEC_H_ */
