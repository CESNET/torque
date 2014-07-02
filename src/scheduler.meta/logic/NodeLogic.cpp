#include "NodeLogic.h"
#include "JobInfo.h"

namespace Scheduler {
namespace Logic {

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
    if (this->get_cores_free() - p_core_assigned < static_cast<int>(spec->procs))
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
  struct resource *mem = this->get_resource("mem");

  if (mem != NULL)
    {
    // requested more than the node can ever provide
    if (static_cast<long long>(spec->mem) > mem->max)
      return CheckNonFit;

    // requested more than the can currently provide
    if (static_cast<long long>(spec->mem) + mem->assigned > mem->max)
      return CheckOccupied;
    }
  else
    {
    // requested node doesn't have memory, but memory requested
    if (spec->mem != 0)
      return CheckNonFit;
    }

  return CheckAvailable;
  }

}}
