#include "ConnectionMgr.h"
#include "utility.h"

/* Helper functions */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <cstring>
#include <stdexcept>
using namespace std;

extern "C"{
#include "libpbs.h"
#include "net_connect.h"
#include "server_limits.h"
}

namespace Scheduler {
namespace Base {

bool verify_fqdn(const string& fqdn)
  {
  struct addrinfo addr;
  memset(&addr,0,sizeof(addr));

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

  bool result = (fqdn == info->ai_canonname);
  freeaddrinfo(info);

  return result;
  }

std::string get_fqdn(const std::string& host)
  {
  struct addrinfo addr;
  memset(&addr,0,sizeof(addr));

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

  string result = info->ai_canonname;
  freeaddrinfo(info);

  return result;
  }

std::string get_local_fqdn()
  {
  char hostname[1024+1] = {0};

  if (gethostname(hostname,1024) != 0)
    throw runtime_error(string("Permanent Network Error: Couldn't get local hostname in \"get_local_fqdn()\" (") + get_errno_string() + string(")."));

  return get_fqdn(string(hostname));
  }


/* sock refers to an opened socket */
int socket_to_conn(int sock)
  {
  int     i;

  for (i = 0; i < PBS_NET_MAX_CONNECTIONS; i++)
    {
    if (connection[i].ch_inuse == 0)
      {
      connection[i].ch_inuse = 1;
      connection[i].ch_errno = 0;
      connection[i].ch_socket = sock;
      connection[i].ch_errtxt = NULL;
      return (i);
      }
    }

  pbs_errno = PBSE_NOCONNECTS;

  return (-1);
  }


int privileged_connect(const char* server)
  {
  pbs_net_t hostaddr;
  if ((hostaddr = get_hostaddr((char*)server)) == (pbs_net_t)0)
    {
    return -1;
    }

  unsigned int port = 15051;
  int sock = client_to_svr(hostaddr, port, 1, NULL);

  if (sock < 0)
    {
    return(-1);
    }

  return socket_to_conn(sock);
  }

int ConnectionMgr::make_master_connection(const string& hostname)
  {
  /* pass-through errors */
  p_master = get_fqdn(hostname);

  int connector = privileged_connect(p_master.c_str());
  if (connector < 0)
    throw runtime_error(string("Connection to master server (\"" + p_master + "\") failed."));

  p_connections.insert(make_pair(p_master,connector));

  return connector;
  }

int ConnectionMgr::make_remote_connection(const string& hostname)
  {
  /* pass-through errors */
  string connect = get_fqdn(hostname);

  int connector = privileged_connect(connect.c_str());
  if (connector < 0)
    throw runtime_error(string("Connection to remote server (\"" + connect + "\") failed."));

  p_connections.insert(make_pair(connect,connector));

  return connector;
  }

void ConnectionMgr::disconnect(const string& hostname)
  {
  map<string,int>::iterator i = p_connections.find(hostname);
  if (i != p_connections.end())
    {
    pbs_disconnect(i->second);
    p_connections.erase(i);
    }
  }

void ConnectionMgr::disconnect_all()
  {
  map<string,int>::iterator i;
  for (i = p_connections.begin(); i != p_connections.end(); ++i)
    {
    pbs_disconnect(i->second);
    }
  p_connections.clear();
  }

int ConnectionMgr::get_master_connection() const
  {
  return get_connection(p_master);
  }

int ConnectionMgr::get_connection(const string& hostname) const
  {
  map<string,int>::const_iterator i = p_connections.find(hostname);
  if (i == p_connections.end())
    throw runtime_error(string("Unexpected hostname (\"") + hostname + string("\") in ConnectionMgr::get_connection()."));

  return i->second;
  }

}}
