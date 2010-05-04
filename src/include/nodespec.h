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
  };

struct pars_spec_node
  {
  unsigned node_count; /**< count of nodes requested for this spec */
  struct pars_prop *properties;
  struct pars_spec_node *next;
  };

struct pars_spec
  {
  unsigned is_exclusive : 1; /**< exclusive request flag */
  struct pars_spec_node *nodes;
  unsigned total_nodes;
  };

typedef struct pars_prop pars_prop;
typedef struct pars_spec_node pars_spec_node;
typedef struct pars_spec pars_spec;

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

/** Expand function, that expands all global nodespec parts into local parts
 *
 * Function determines the exclusivity request (shared/exclusive nodes) and
 * expands all global nodespec parts (#prop) into all local parts.
 *
 * @see NODESPEC_DEFAULT_EXCLUSIVE
 *
 * @param nodespec Nodespec to be expanded
 * @param is_exclusive Output variable, exclusivity is stored here
 * @return Expanded nodespec
 */
char *expand_nodespec(const char* nodespec, int *is_exclusive);

/** Free one node spec from the list of node specs
 *
 * @param node List of node specs
 */
void free_pars_spec_node(pars_spec_node **node);

/** Clone the selected one node spec
 *
 * @param node Node spec to be cloned
 * @return Copy of the spec
 */
pars_spec_node *clone_pars_spec_node(pars_spec_node *node);

/** Concat the parsed nodespec back to the string representation
 *
 * @param nodespec Node spec to be concated
 * @return concated nodespec
 */
char *concat_nodespec(pars_spec *nodespec);

#endif /* NODESPEC_H_ */
