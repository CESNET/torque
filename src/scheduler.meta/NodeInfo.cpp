#include <cstring>
#include <climits>
#include "data_types.h"
#include "NodeInfo.h"
#include "server_info.h"
#include "assertions.h"
#include "misc.h"
#include "RescInfoDb.h"


using namespace std;

int node_has_enough_resource(node_info *ninfo, char *name, char *value, enum ResourceCheckMode mode);

CheckResult result_helper(bool result)
  {
  if (result)
    return CheckAvailable;
  else
    return CheckNonFit;
  }

CheckResult node_info::has_prop(pars_prop* property, bool physical_only)
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
      return result_helper(!negative);

    set<string>::iterator it;

    it = physical_properties.find(string(prop_name));
    if (it != physical_properties.end())
      return result_helper(!negative);

    if (!physical_only)
      {
      it = virtual_properties.find(string(prop_name));
      if (it != virtual_properties.end())
        return result_helper(!negative);
      }
    }
  else /* resource or ppn */
    {
    if (strcmp(prop_name,"host") == 0 && strcmp(prop_value, name) == 0)
      return result_helper(!negative);

    return has_resc(property);
    }

  return result_helper(negative); /* if negative property and not found return true, otherwise return false */
  }

bool node_info::has_prop(const char* property)
  {
  char *buf;
  pars_prop *prop;
  bool ret;

  dbg_precondition(property != NULL, "This functions does not accept NULL.");

  buf = strdup(property);
  prop = parse_prop(buf);

  ret = has_prop(prop,true);

  free_pars_prop(&prop);
  free(buf);

  return ret;
  }

CheckResult node_info::has_proc(job_info *job, pars_spec_node *spec)
  {
  // for admin slots, check the admin slot instead
  if (job->queue->is_admin_queue)
    {
    if (p_admin_slot_avail)    // admin slot currently free
      return CheckAvailable;
    else if (p_admin_slot_enabled) // admin slot occupied, but enabled
      return CheckOccupied;
    else                         // admin slot not enabled
      return CheckNonFit;
    }
  else
    {
    if (p_core_total < static_cast<int>(spec->procs)) // This node simply doesn't have enough processors
      {
      return CheckNonFit;
      }

    if (p_exclusively_assigned) // Node is completely full
      {
      return CheckOccupied;
      }

    if (p_core_free - p_core_assigned == 0) // Node is completely full
      {
      return CheckOccupied;
      }

    if (job->is_exclusive)
      {
      if (p_core_free - p_core_assigned != p_core_total) // Some processors are in use
        return CheckOccupied;
      }
    else
      {
      if (p_core_free - p_core_assigned < static_cast<int>(spec->procs)) // Not enough currently free processors
        return CheckOccupied;
      }

    return CheckAvailable;
    }
  }

CheckResult node_info::has_mem(job_info *job, pars_spec_node *spec)
  {
  if (job->queue->is_admin_queue)
    return CheckAvailable;

  // vmem currently ignored
  struct resource *mem = find_resource(res,"mem");

  if (mem != NULL)
    {
    if (static_cast<long long>(spec->mem) > mem->max) // requested more than the node can ever provide
      return CheckNonFit;

    if (static_cast<long long>(spec->mem) + mem->assigned > mem->max) // requested more than the can currently provide
      return CheckOccupied;
    }
  else
    {
    if (spec->mem != 0) // requested node doesn't have memory, but memory requested
      return CheckNonFit;
    }

  return CheckAvailable;
  }

CheckResult check_scratch_helper(resource *reslist, const char *name, unsigned long long value)
  {
  struct resource *res = find_resource(reslist,name);

  if (res == NULL)
    return CheckNonFit;

  if (res->avail - res->assigned <= 0)
    return CheckOccupied;

  if (static_cast<unsigned long long>(res->avail - res->assigned) >= value)
    return CheckAvailable;

  return CheckOccupied;
  }

CheckResult node_info::has_scratch(job_info *job, pars_spec_node *spec, ScratchType *scratch)
  {
  if (job->queue->is_admin_queue) // admin jobs skip scratch check
    return CheckAvailable;

  if (spec->scratch_type == ScratchNone) // if no scratch was requested, atomatic avail
    return CheckAvailable;

  CheckResult has_local = check_scratch_helper(res,"scratch_local",spec->scratch);
  CheckResult has_ssd  = check_scratch_helper(res,"scratch_ssd",spec->scratch);
  CheckResult has_shared;

  if (scratch_pool.length() > 0)
    {
    map<string, DynamicResource>::iterator i = server->dynamic_resources.find(scratch_pool);
    if (i != server->dynamic_resources.end())
      {
      if (i->second.would_fit(spec->scratch)) // shared pool present and free
        {
        has_shared = CheckAvailable;
        }
      else // shared pool present, but currently doesn't provide enough
        {
        has_shared = CheckOccupied;
        }
      }
    else // no shared pool present
      {
      has_shared = CheckNonFit;
      }
    }

  if (spec->scratch_type == ScratchSSD && has_ssd == CheckNonFit)
    return CheckNonFit;

  if (spec->scratch_type == ScratchShared && has_shared == CheckNonFit)
    return CheckNonFit;

  if (spec->scratch_type == ScratchLocal && has_local == CheckNonFit)
    return CheckNonFit;

  if (has_local == CheckNonFit && has_ssd == CheckNonFit && has_shared == CheckNonFit)
    return CheckNonFit;

  if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchSSD) && has_ssd == CheckAvailable)
    {
    *scratch = ScratchSSD;
    return CheckAvailable;
    }

  if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchShared) && has_shared == CheckAvailable)
    {
    *scratch = ScratchShared;
    return CheckAvailable;
    }

  if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchLocal) && has_local == CheckAvailable)
    {
    *scratch = ScratchLocal;
    return CheckAvailable;
    }

  return CheckOccupied;
  }


CheckResult node_info::has_resc(pars_prop *prop)
  {
  if (res_check_type(prop->name) == ResCheckDynamic) // already checked elsewhere
    return CheckAvailable;

  if (res_check_type(prop->name) == ResCheckNone) // this resource shouldn't be checked
    return CheckAvailable;

  struct resource *resc;
  if ((resc = find_resource(res,prop->name)) == NULL)
    return CheckNonFit;

  if (resc->is_string) // string resources work as properties
    {
    if (strcmp(resc->str_avail,prop->value) != 0)
      return CheckNonFit;
    else
      return CheckAvailable;
    }

  sch_resource_t amount = res_to_num(prop->value);

  if (res->max != INFINITY && res->max != UNSPECIFIED && res->max < amount) // there is a max and it's lower than the requested amount
    return CheckNonFit;

  if (res->max == UNSPECIFIED || res->max == INFINITY) // no max value present, only current
    {
    if (res->avail - res->assigned >= amount)
      return CheckAvailable;
    else
      return CheckOccupied;
    }
  else // max value present
    {
    if (res->max - res->assigned >= amount)
      return CheckAvailable;
    else
      return CheckOccupied;
    }
  }


CheckResult node_info::has_props_boot(job_info *job, pars_spec_node *spec, repository_alternatives *virt_conf)
  {
  CheckResult result = CheckAvailable;
  pars_prop *prop = spec->properties;

  while (prop != NULL)
    {
    CheckResult test_phys = has_prop(prop,true);
    CheckResult test_virt = alternative_has_property(virt_conf,prop->name)?CheckNonFit:CheckAvailable;

    if (test_phys == CheckNonFit && test_virt == CheckNonFit)
      return CheckNonFit;

    if (test_phys != CheckAvailable && test_virt != CheckAvailable)
      result = CheckOccupied;

    prop = prop->next;
    }

  return result;
  }

CheckResult node_info::has_props_run(job_info *job, pars_spec_node *spec)
  {
  CheckResult result = CheckAvailable;
  pars_prop *prop = spec->properties;

  while (prop != NULL)
    {
    CheckResult test = has_prop(prop,false);
    if (test == CheckNonFit)
      return CheckNonFit;
    else if (test == CheckOccupied)
      result = CheckOccupied;

    prop = prop->next;
    }

  return result;
  }

CheckResult node_info::has_spec(job_info *job, pars_spec_node *spec, ScratchType *scratch)
  {
  CheckResult proc_result = has_proc(job,spec);
  CheckResult mem_result  = has_mem(job,spec);
  CheckResult scratch_result = has_scratch(job,spec,scratch);

  if (proc_result == CheckNonFit ||
      mem_result == CheckNonFit ||
      scratch_result == CheckNonFit)
    {
    return CheckNonFit;
    }

  if (proc_result == CheckOccupied ||
      mem_result == CheckOccupied ||
      scratch_result == CheckOccupied)
    {
    return CheckOccupied;
    }

  return CheckAvailable;
  }
