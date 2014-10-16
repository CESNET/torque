#ifndef NODE_LOGIC_H_
#define NODE_LOGIC_H_

#include "base/NodeData.h"
#include "nodespec.h"
#include "LogicFwd.h"
#include <vector>

struct job_info; // TODO cleanup, move to Fwd header

namespace Scheduler {
namespace Logic {

class NodeLogic : public Base::NodeData
  {
  public:
    NodeLogic(struct batch_status *node_data);
    ~NodeLogic();

    /** \brief Check whether the node has currently enough empty CPU cores for the job/nodespec */
    CheckResult has_proc(const job_info *job, const pars_spec_node *spec) const;
    /** \brief Check whether the node has currently enough memory for the job/nodespec */
    CheckResult has_mem(const job_info *job, const pars_spec_node *spec) const;
    /** \brief Check whether the node has the desired scratch
     *
     * ScratchAny is a supported mode, where the first available scratch is allocated.
     * The order of scratches is determined by the scratch_priority cache metric (see query_external_cache).
     */
    CheckResult has_scratch(const job_info *job, const pars_spec_node *spec, ScratchType *scratch) const;
    /** \brief Check whether the node has a particular resource */
    CheckResult has_resc(const pars_prop *prop) const;

  // ACCESS METHODS
    /** \brief Schedule processors */
    void deplete_proc(int count);
    /** \brief Unschedule processors */
    void freeup_proc(int count);

    void set_scratch_priority(size_t order, ScratchType scratch);

  protected:
    int p_core_assigned; ///< Number of scheduled cores
  private:
    std::vector<ScratchType> p_scratch_priority; ///< Order of scratch allocation
  };

}}

#endif
