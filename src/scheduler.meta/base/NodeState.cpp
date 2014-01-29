/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include "NodeState.h"

// include for ND_ constants
#include "pbs_ifl.h"

#include <cstring>

namespace Scheduler {
namespace Base {
namespace Internals {

NodeState::NodeState() : p_is_down(false), p_is_free(false), p_is_offline(false),
                         p_is_unknown(false), p_is_reserved(false), p_is_job_excl(false),
                         p_is_job_share(false), p_is_busy(false)
  {
  }

NodeState::NodeState(const char *state)
                       : p_is_down(false), p_is_free(false), p_is_offline(false),
                         p_is_unknown(false), p_is_reserved(false), p_is_job_excl(false),
                         p_is_job_share(false), p_is_busy(false)
  {
  this->reset_state(state);
  }

NodeState::~NodeState()
  {
  }

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

}}}
