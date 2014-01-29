/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include "MiscHelpers.h"

#include <cstring>
#include <cctype>

using namespace Scheduler;
using namespace Base;
using namespace std;

namespace Scheduler {
namespace Base {

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

}}
