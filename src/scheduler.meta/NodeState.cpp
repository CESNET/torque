#include "NodeState.h"

// include for ND_ constants
#include "pbs_ifl.h"

#include <cstring>

NodeState::NodeState() : p_is_down(false), p_is_free(false), p_is_offline(false), p_is_unknown(false), p_is_reserved(false), p_is_job_excl(false), p_is_job_share(false), p_is_busy(false), p_is_notusable(false) {}
NodeState::~NodeState() {}

/** \brief Reset the node states from state string */
void NodeState::reset_state(const char *state)
  {
  if (strstr(state,ND_down) != NULL)    p_is_down = true;
  if (strstr(state,ND_free) != NULL)    p_is_free = true;
  if (strstr(state,ND_offline) != NULL) p_is_offline = true;
  if (strstr(state,ND_reserve) != NULL) p_is_reserved = true;
  if (strstr(state,ND_busy) != NULL)    p_is_busy = true;
  if (strstr(state,ND_job_sharing) != NULL)   p_is_job_share = true;
  if (strstr(state,ND_job_exclusive) != NULL) p_is_job_excl = true;
  if (strstr(state,ND_state_unknown) != NULL) p_is_unknown = true;
  }
