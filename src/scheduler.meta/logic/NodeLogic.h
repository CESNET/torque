#ifndef NODE_LOGIC_H_
#define NODE_LOGIC_H_

#include "base/NodeData.h"
#include "nodespec.h"
#include "LogicFwd.h"

struct job_info; // TODO cleanup, move to Fwd header

namespace Scheduler {
namespace Logic {

class NodeLogic : public Base::NodeData
  {
  public:
    NodeLogic(struct batch_status *node_data) : NodeData(node_data), p_core_assigned(0) {}
    ~NodeLogic() {}

    /** \brief Check whether the node has currently enough empty CPU cores for the job/nodespec */
    CheckResult has_proc(const job_info *job, const pars_spec_node *spec) const;
    /** \brief Check whether the node has currently enough memory for the job/nodespec */
    CheckResult has_mem(const job_info *job, const pars_spec_node *spec) const;

  // ACCESS METHODS
    /** \brief Schedule processors */
    void deplete_proc(int count) { p_core_assigned += count; }
    /** \brief Unschedule processors */
    void freeup_proc(int count) { p_core_assigned -= count; }

  private:
    int p_core_assigned; ///< Number of scheduled cores
  };

}}

#endif
