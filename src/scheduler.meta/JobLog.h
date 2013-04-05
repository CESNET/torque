#ifndef JOBLOG_H_
#define JOBLOG_H_

#include <string>
#include <fstream>

class JobLog
  {
  public:
    JobLog(const std::string& jobid);
    ~JobLog();

    void log_line(const char* line);
    void log_line(const std::string& line);

    template < typename T >
    JobLog& operator << (const T& data)
      {
      p_log << data;
      return *this;
      }

    JobLog& operator << (std::ostream& (&manip)(std::ostream&));

  private:
    std::ofstream p_log;
  };

#endif /* JOBLOG_H_ */
