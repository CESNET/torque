#include "NodeLogic.h"
#include "JobInfo.h"
#include "RescInfoDb.h"
#include "base/MiscHelpers.h"

#include <map>
using namespace std;

namespace Scheduler {
namespace Logic {

NodeLogic::NodeLogic(struct batch_status *node_data) : NodeData(node_data), p_core_assigned(0), p_scratch_priority(3)
  {
  p_scratch_priority[0]=ScratchSSD;
  p_scratch_priority[1]=ScratchShared;
  p_scratch_priority[2]=ScratchLocal;
  }

NodeLogic::~NodeLogic() {}

void NodeLogic::deplete_proc(int count)
  {
  p_core_assigned += count;
  }

void NodeLogic::freeup_proc(int count)
  {
  p_core_assigned -= count;
  }

void NodeLogic::set_scratch_priority(size_t order, ScratchType scratch)
  {
  p_scratch_priority[order]=scratch;
  }

CheckResult NodeLogic::has_proc(const job_info *job, const pars_spec_node *spec) const
  {
  // If job is comming from an admin queue, skip checking CPU cores
  if (job->queue->is_admin_queue)
    {
    // admin slot currently free
    if (this->get_admin_slot_avail())
      return CheckAvailable;
    // admin slot occupied, but enabled
    else if (this->get_admin_slot_enabled())
      return CheckOccupied;
    // admin slot not enabled
    else
      return CheckNonFit;
    }

  // This node simply doesn't have enough processors
  if (this->get_cores_total() < static_cast<int>(spec->procs))
    return CheckNonFit;

  // Node is completely full
  if (this->is_exclusively_assigned())
    return CheckOccupied;

  // Node is completely full
  if (this->get_cores_free() - this->p_core_assigned == 0)
    return CheckOccupied;

  if (job->is_exclusive)
    {
    // Some processors are in use, job requires all
    if (this->get_cores_free() - this->p_core_assigned != this->get_cores_total())
      return CheckOccupied;
    }
  else
    {
    // Not enough currently free processors
    if (this->get_cores_free() - this->p_core_assigned < static_cast<int>(spec->procs))
      return CheckOccupied;
    }

  return CheckAvailable;
  }

CheckResult NodeLogic::has_mem(const job_info *job, const pars_spec_node *spec) const
  {
  // No memory checks for admin jobs
  if (job->queue->is_admin_queue)
    return CheckAvailable;

  // Only checking mem, vmem currently skipped
  Resource *mem = this->get_resource("mem");
  if (mem == NULL)
    {
    if (spec->mem != 0)
      return CheckNonFit;
    else
      return CheckAvailable;
    }
  else
    {
    return mem->check_numeric_fit(spec->mem);
    }
  }

static CheckResult check_scratch_helper(const NodeLogic * const ninfo, const char *name, unsigned long long value)
  {
  Resource *res = ninfo->get_resource(name);
  if (res == NULL)
    return CheckNonFit;

  return res->check_numeric_fit(value);
  }

CheckResult NodeLogic::has_scratch(const job_info *job, const pars_spec_node *spec, ScratchType *scratch) const
  {
  // admin jobs skip scratch check
  if (job->queue->is_admin_queue)
    return CheckAvailable;

  // if no scratch was requested, node is fitting
  if (spec->scratch_type == ScratchNone)
    return CheckAvailable;

  CheckResult has_local = check_scratch_helper(this,"scratch_local",spec->scratch);
  CheckResult has_ssd  = check_scratch_helper(this,"scratch_ssd",spec->scratch);
  CheckResult has_shared = CheckNonFit;

  if (this->get_scratch_pool().length() > 0)
    {
    map<string, DynamicResource>::iterator i = this->get_parent_server()->dynamic_resources.find(this->get_scratch_pool());
    if (i != this->get_parent_server()->dynamic_resources.end())
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

  // Test scratch according to its priority
  for (size_t i = 0; i < 3; i++)
    {
    switch (this->p_scratch_priority[i])
      {
      case ScratchSSD:
        if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchSSD) && has_ssd == CheckAvailable)
          {
          *scratch = ScratchSSD;
          return CheckAvailable;
          }
      case ScratchShared:
        if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchShared) && has_shared == CheckAvailable)
          {
          *scratch = ScratchShared;
          return CheckAvailable;
          }
      case ScratchLocal:
        if ((spec->scratch_type == ScratchAny || spec->scratch_type == ScratchLocal) && has_local == CheckAvailable)
          {
          *scratch = ScratchLocal;
          return CheckAvailable;
          }
      default: // ScratchNone already handled above
        return CheckNonFit;
      };
    }

  return CheckOccupied;
  }


CheckResult NodeLogic::has_resc(const pars_prop *prop) const
  {
  if (res_check_type(prop->name) == ResCheckDynamic) // already checked elsewhere
    return CheckAvailable;

  if (res_check_type(prop->name) == ResCheckNone) // this resource shouldn't be checked
    return CheckAvailable;

  if (strcmp(prop->name,"minspec")==0)
    {
    double value = atof(prop->value);
    if (this->get_node_spec() >= value)
      return CheckAvailable;
    else
      return CheckNonFit;
    }

  if (strcmp(prop->name,"maxspec")==0)
    {
    double value = atof(prop->value);
    if (this->get_node_spec() <= value)
      return CheckAvailable;
    else
      return CheckNonFit;
    }

  Resource *resc;
  if ((resc = this->get_resource(prop->name)) == NULL)
    return CheckNonFit;

  return resc->check_fit(prop->value);
  }

}}
