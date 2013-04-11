#include "utility.h"
#include "misc.h"

#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using namespace std;

void global_oom_handle()
  {
  /** TODO Implement better handling */
  sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, __PRETTY_FUNCTION__, "No memory, unrecoverable error. Calling exit().");
  exit(1);
  }

string get_errno_string()
  {
  char msg[256] = {0};
  if (strerror_r(errno,msg,256) != 0)
    throw runtime_error(string("System Error: \"strerror_r()\" failed in \"get_errno_string()\"."));

  return string(msg);
  }

