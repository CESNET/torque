#include <cstring>
#include "data_types.h"
#include "NodeInfo.h"
#include "assertions.h"

using namespace std;

int node_has_enough_resource(node_info *ninfo, char *name, char *value, enum ResourceCheckMode mode);

bool node_info::has_prop(pars_prop* property, int preassign_starving, bool physical_only)
  {
  bool negative = false;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");

  char *prop_name  = property->name;
  char *prop_value = property->value;

  if (prop_name[0] == '^')
    {
    negative = true;
    prop_name++;
    }

  if (prop_value == NULL) /* property, not a resource */
    {
    if (strcmp(name,prop_name) == 0)
      return !negative;

    set<string>::iterator it;

    it = physical_properties.find(string(prop_name));
    if (it != physical_properties.end())
      return !negative;

    if (!physical_only)
      {
      it = virtual_properties.find(string(prop_name));
      if (it != virtual_properties.end())
        return !negative;
      }
    }
  else /* resource or ppn */
    {
    if (strcmp(prop_name,"host") == 0 && strcmp(prop_value, name) == 0)
      return !negative;

    return node_has_enough_resource(this, prop_name, prop_value, preassign_starving?MaxOnly:Avail);
    }

  return negative; /* if negative property and not found return true, otherwise return false */
  }

bool node_info::has_prop(const char* property)
  {
  char *buf;
  pars_prop *prop;
  bool ret;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");

  buf = strdup(property);
  prop = parse_prop(buf);

  ret = has_prop(prop,0,true);

  free_pars_prop(&prop);
  free(buf);

  return ret;
  }
