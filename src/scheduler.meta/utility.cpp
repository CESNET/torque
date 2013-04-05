#include "utility.h"
#include <stdexcept>
using namespace std;

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cstring>

void global_oom_handle()
  {
  /** TODO Implement better handling */
  exit(1);
  }

string get_errno_string()
  {
  char msg[256] = {0};
  if (strerror_r(errno,msg,256) != 0)
    throw runtime_error(string("System Error: \"strerror_r()\" failed in \"get_errno_string()\"."));

  return string(msg);
  }

bool verify_fqdn(const string& fqdn)
  {
  struct addrinfo addr = {0};
  addr.ai_family = AF_INET;
  addr.ai_socktype = SOCK_STREAM;
  addr.ai_flags = AI_CANONNAME;

  struct addrinfo *info = NULL;
  if (getaddrinfo(fqdn.c_str(),"15051",&addr,&info) != 0)
    {
    if (errno == EAI_MEMORY)
      global_oom_handle();
    else if (errno == EAI_AGAIN)
      throw runtime_error(string("Temporary Network Error: Couldn't resolve hostname in \"verify_fqdn()\"."));
    else if (errno == EAI_FAIL)
      throw runtime_error(string("Permanent Network Error: Couldn't resolve hostname in \"verify_fqdn()\"."));

    return false;
    }

  return string(info->ai_canonname) == fqdn;
  }

std::string get_fqdn(const std::string& host)
  {
  struct addrinfo addr = {0};
  addr.ai_family = AF_INET;
  addr.ai_socktype = SOCK_STREAM;
  addr.ai_flags = AI_CANONNAME;

  struct addrinfo *info = NULL;
  if (getaddrinfo(host.c_str(),"15051",&addr,&info) != 0)
    {
    if (errno == EAI_MEMORY)
      global_oom_handle();
    else if (errno == EAI_AGAIN)
      throw runtime_error(string("Temporary Network Error: Couldn't resolve hostname in \"get_fqdn()\"."));
    else
      throw runtime_error(string("Permanent Network Error: Couldn't resolve hostname in \"get_fqdn()\"."));
    }

  return string(info->ai_canonname);
  }

std::string get_local_fqdn()
  {
  char hostname[1024+1] = {0};

  if (gethostname(hostname,1024) != 0)
    throw runtime_error(string("Permanent Network Error: Couldn't get local hostname in \"get_local_fqdn()\" (") + get_errno_string() + string(")."));

  return get_fqdn(string(hostname));
  }
