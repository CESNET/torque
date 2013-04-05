#include "JobLog.h"
#include "unistd.h"

#include <stdexcept>
using namespace std;

#define BASE_JOBLOG_PATH "/var/spool/torque/sched_logs/jobs/"

JobLog::JobLog(const string& jobid)
  {
  string old_name = string(BASE_JOBLOG_PATH)+jobid;
  string new_name = string(BASE_JOBLOG_PATH)+jobid+string(".tmp");

  unlink(old_name.c_str());
  rename(new_name.c_str(),old_name.c_str());

  p_log.open(new_name.c_str(),ios::trunc | ios::out);
  if (!p_log.is_open())
    {
    throw runtime_error("Can't open job log file.");
    }
  }

JobLog::~JobLog()
  {
  p_log.close();
  }

void JobLog::log_line(const char* line)
  {
  p_log << line;
  }

void JobLog::log_line(const std::string& line)
  {
  p_log << line;
  }

JobLog& JobLog::operator << (ostream& (&manip)(ostream&))
  {
  p_log << manip;
  return *this;
  }
