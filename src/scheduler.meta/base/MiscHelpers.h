/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#ifndef MISCHELPERS_H_
#define MISCHELPERS_H_

#include <set>
#include <string>
#include "BaseFwd.h"

namespace Scheduler {
namespace Base {

/** \brief Convert a comma separated string into a set of strings
 * @param list Comma separated text
 * @param s set of strings to be filled
 */
void comma_list_to_set(char *list, std::set<std::string>& s);

/** \brief Convert a string to a resource value
 *
 * Converts units (k,m,g,t)
 */
sch_resource_t res_to_num(char * res_str);

}}


#endif /* MISCHELPERS_H_ */
