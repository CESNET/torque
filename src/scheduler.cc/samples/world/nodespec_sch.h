#ifndef NODESPEC_H_
#define NODESPEC_H_

/** Check if the node has a property
 *
 * @param ninfo Node to be checked
 * @param property Property to be searched for
 * @return 1 if found, 0 if not found
 */
int get_node_has_property(node_info *ninfo, const char* property);

#endif /* NODESPEC_H_ */
