#ifndef CONNECTIONMGR_H_
#define CONNECTIONMGR_H_

#include <map>
#include <string>

namespace Scheduler {
namespace Base {

/** \brief Manager for network connections to Torque servers
 */
class ConnectionMgr
  {
  public:
  /** \brief Try to establish a connection, mark it as master
   *
   * @param hostname FQDN of the server
   * @return connection ID
   */
  int make_master_connection(const std::string& hostname);

  /** \brief Try to establish a connection
   *
   * @param hostname FQDN of the server
   * @return connection ID
   */
  int make_remote_connection(const std::string& hostname);

  /** \brief Get the master connection ID
   *
   * @return connection ID
   */
  int get_master_connection() const;

  /** \brief Get the connection ID corresponding to this hostname
   *
   * @param hostname FQDN of the server
   * @return connection ID
   */
  int get_connection(const std::string& hostname) const;

  /** \brief Disconnect the corresponding server
   *
   * @param hostname FQDN of the server
   */
  void disconnect(const std::string& hostname);

  /** \brief Disconnect all servers */
  void disconnect_all();

  private:
  std::map<std::string,int> p_connections;
  std::string p_master;

  };

}}

#endif /* CONNECTIONMGR_H_ */
