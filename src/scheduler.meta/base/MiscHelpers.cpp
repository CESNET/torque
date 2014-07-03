/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include "MiscHelpers.h"

#include <cstring>
#include <cctype>
#include <cstdlib>
#include "constant.h"

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

sch_resource_t res_to_num(char * res_str)
  {
  sch_resource_t count = UNSPECIFIED; /* convert string resource to numeric */
  sch_resource_t count2 = UNSPECIFIED; /* convert string resource to numeric */
  char *endp;    /* used for strtol() */
  char *endp2;    /* used for strtol() */
  long multiplier;   /* multiplier to count */

  count = strtol(res_str, &endp, 10);

  if (*endp == ':')   /* time resource -> convert to seconds */
    {
    count2 = strtol(endp + 1, &endp2, 10);

    if (*endp2 == ':')   /* form of HH:MM:SS */
      {
      count *= 3600;
      count += count2 * 60;
      count += strtol(endp2 + 1, &endp, 10);

      if (*endp != '\0')
        count = UNSPECIFIED;
      }
    else   /* form of MM:SS */
      {
      count *= 60;
      count += count2;
      }

    multiplier = 1;
    }
  else if (*endp == 'k' || *endp == 'K')
    multiplier = 1;
  else if (*endp == 'm' || *endp == 'M')
    multiplier = MEGATOKILO;
  else if (*endp == 'g' || *endp == 'G')
    multiplier = GIGATOKILO;
  else if (*endp == 't' || *endp == 'T')
    multiplier = TERATOKILO;
  else if (*endp == 'b' || *endp == 'B')
    {
    count /= KILO;
    multiplier = 1;
    }
  else if (*endp == 'w')
    {
    count /= KILO;
    multiplier = SIZE_OF_WORD;
    }
  else /* catch all */
    multiplier = 1;

  if (*endp != '\0' && *(endp + 1) == 'w')
    multiplier *= SIZE_OF_WORD;

  return count * multiplier;
  }

}}
