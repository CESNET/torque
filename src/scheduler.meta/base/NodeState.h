/// \file Internal API
/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */
#ifndef NODESTATE_H_
#define NODESTATE_H_

#include <set>

namespace Scheduler {
namespace Base {

/// Node state constants
enum NodeStateRaw {
  NodeStateDown,        //!< NodeStateDown
  NodeStateFree,        //!< NodeStateFree
  NodeStateOffline,     //!< NodeStateOffline
  NodeStateUnknown,     //!< NodeStateUnknown
  NodeStateJobExclusive,//!< NodeStateJobExclusive
  NodeStateJobSharing,  //!< NodeStateJobSharing
  NodeStateReserved,    //!< NodeStateReserved
  NodeStateBusy,        //!< NodeStateBusy
  NodeStateNotUsable//!< NodeStateNotUsable
};

namespace Internals {

/** \brief Base class for the node data structure
 *
 * This class stores the current PBS state of the node.
 */
class NodeState
  {
  public:

    /// Re-initialize state from the state string
    void reset_state(const char *state);

    /// Test down state
    bool is_down() const { return p_is_down; }
    /// Test free state
    bool is_free() const { return p_is_free; }
    /// Test offline state
    bool is_offline() const { return p_is_offline; }
    /// Test unknown state
    bool is_unknown() const { return p_is_unknown; }
    /// Test reserved state
    bool is_reserved() const { return p_is_reserved; }
    /// Test busy state
    bool is_busy() const { return p_is_busy; }
    /// Test job-exclusive state
    bool is_job_exclusive() const { return p_is_job_excl; }
    /// Test job-shared state
    bool is_job_shared() const { return p_is_job_share; }

  protected:
    /// Protected constructor, this class is not meant to be used standalone
    NodeState();
    /// Protected parametric constructor (initialize from state string), this class is not meant to be used standalone
    NodeState(const char *state);
    /// Protected destructor, this class is not meant to be used standalone
    ~NodeState();

  private:
    bool p_is_down;
    bool p_is_free;
    bool p_is_offline;
    bool p_is_unknown;
    bool p_is_reserved;
    bool p_is_job_excl;
    bool p_is_job_share;
    bool p_is_busy;
  };

}}}

#endif /* NODESTATE_H_ */
