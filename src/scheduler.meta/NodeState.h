#ifndef NODESTATE_H_
#define NODESTATE_H_

#include <set>

enum NodeStateRaw {
  NodeStateDown,        //!< NodeStateDown
  NodeStateFree,        //!< NodeStateFree
  NodeStateOffline,     //!< NodeStateOffline
  NodeStateUnknown,     //!< NodeStateUnknown
  NodeStateJobExclusive,//!< NodeStateJobExclusive
  NodeStateJobSharing,  //!< NodeStateJobSharing
  NodeStateReserved,    //!< NodeStateReserved
  NodeStateBusy,        //!< NodeStateBusy
  NodeStateNotUsable
};

class NodeState
  {
  public:
    void reset_state(const char *state);

    bool is_down() const { return p_is_down; }
    bool is_free() const { return p_is_free; }
    bool is_offline() const { return p_is_offline; }
    bool is_unknown() const { return p_is_unknown; }
    bool is_reserved() const { return p_is_reserved; }
    bool is_busy() const { return p_is_busy; }
    bool is_job_exclusive() const { return p_is_job_excl; }
    bool is_job_shared() const { return p_is_job_share; }
    bool is_notusable() const { return p_is_notusable; }

    void set_notusable() { p_is_notusable = true; }

  protected:
    NodeState();
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
    bool p_is_notusable;
  };

#endif /* NODESTATE_H_ */
