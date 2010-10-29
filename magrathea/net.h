/* $Id: net.h,v 1.14 2008/10/01 14:48:20 xdenemar Exp $ */

/** @file
 * Network functions for magrathea.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.14 $ $Date: 2008/10/01 14:48:20 $
 */

#ifndef NET_H
#define NET_H

#include <sys/socket.h>
#include <netinet/in.h>
#ifndef NET_NO_IO
# include "utils.h"
#endif

#ifndef IPv6
# define IPv6 0
#endif


/** Default port number. */
#define DEFAULT_PORT        5678

/** Maximum length of UNIX path. */
#define UNIX_PATH_MAX       108


/** Union to hold IPv4 or IPv6 address. */
typedef union {
    struct in_addr ip4;
#if IPv6
    struct in6_addr ip6;
#endif
} net_addr_t;


/** Union to hold IPv4 or IPv6 sockaddr. */
typedef union {
    struct sockaddr_in sin;
#if IPv6
    struct sockaddr_in6 sin6;
#endif
} net_sockaddr_t;


/** Protocol type. */
enum net_protocol {
    NPROTO_AUTO,
    NPROTO_IPV4,
    NPROTO_IPV6
};


/** Address type. */
enum net_addr_type {
    NADDR_IPV4,
    NADDR_IPV6_MAPPED_IPV4,
    NADDR_IPV6
};


/** INADDR_ANY or IN6ADDR_ANY depending on protocol family. */
extern net_addr_t net_inaddr_any;

/** String representation of net_inaddr_any. */
extern const char *net_inaddr_any_str;


/** Set IP protocol used for current session.
 * This function has to be called before any other network related function.
 * Otherwise, behaviour of a whole program is undefined.
 *
 * @param[in] proto
 *      target protocol. If proto is equal to NPROTO_AUTO, the function tries
 *      to create an IPv6 socket and bind it to ::, port 0. If this succeeds,
 *      IPv6 will be used, otherwise IPv4 will be used.
 *
 * @return
 *      nothing.
 */
extern void _net_proto(enum net_protocol proto);
#define net_proto(p) _net_proto((p))


/** Compare two IP addresses.
 *
 * @param[in] ip1
 *      the first IP address.
 *
 * @param[in] ip2
 *      the second IP address.
 *
 * @return
 *      nonzero iff ip1 is equal to ip2.
 */
extern int _net_addr_equal(net_addr_t ip1, net_addr_t ip2);
#define net_addr_equal(ip1, ip2) _net_addr_equal((ip1), (ip2))


/** Get IP address type.
 *
 * @param[in] ip
 *      the IP address in question.
 *
 * @return
 *      type of the IP address.
 */
extern enum net_addr_type _net_addr_type(net_addr_t ip);
#define net_addr_type(ip) _net_addr_type((ip))


/** Get string representation of the address.
 * When running in IPv6 mode, IPv6-mapped IPv4 addresses are still converted
 * as standard IPv4 addresses.
 *
 * Currently, the result is stored in a static buffer, so this function is
 * NOT reentrant.
 *
 * @param[in] ip
 *      an IP address to be stringified.
 *
 * @return
 *      string representation of the IP address.
 */
extern const char *_net_addr_str(net_addr_t ip);
#define net_addr_str(ip) _net_addr_str((ip))


/** Convert IP address from text to binary form.
 * IPv4 addresses are automatically translated into IPv6-mapped IPv4 addresses
 * when running in IPv6 mode.
 *
 * @param[in] str
 *      text representation of the IP address.
 *
 * @param[out] ip
 *      where the result will be stored.
 *
 * @return
 *      nonzero on success, zero if the address is invalid.
 */
extern int _net_addr_parse(const char *str, net_addr_t *ip);
#define net_addr_parse(s, ip) _net_addr_parse((s), (ip))


/** Translate host's domain name into a list of host's IP addresses.
 *
 * @param[in] hostname
 *      domain name of the host.
 *
 * @param[out] count
 *      number of IP addresses in the list returned.
 *
 * @return
 *      list of host's IP addresses. If current protocol is NPROTO_IPV6, IPv6
 *      addresses are in the beginning of the list. Otherwise, only IPv4
 *      addresses are returned. The list is dynamically allocated and the
 *      caller has to free it by calling free(). @c NULL is returned on error.
 */
extern net_addr_t *_net_addr_resolve(const char *hostname, int *count);
#define net_addr_resolve(h, c) _net_addr_resolve((h), (c))


/** Assign an IP address into a net_sockaddr_t structure.
 * The function also sets address family.
 *
 * @param[out] addr
 *      where to assign the IP address.
 *
 * @param[in] ip
 *      the IP address to be assigned.
 *
 * @return
 *      nothing.
 */
extern void _net_sockaddr_set_ip(net_sockaddr_t *addr, net_addr_t ip);
#define net_sockaddr_set_ip(a, ip) _net_sockaddr_set_ip((a), (ip))


/** Get an IP address from a net_sockaddr_t structure.
 *
 * @param[in] addr
 *      pointer to the net_sockaddr_t structure.
 *
 * @return
 *      IP address from the structure.
 */
extern net_addr_t _net_sockaddr_get_ip(const net_sockaddr_t *addr);
#define net_sockaddr_get_ip(a) _net_sockaddr_get_ip((a))


/** Assign a port number into a net_sockaddr_t structure.
 * The port number is automatically converted from host to network byte order.
 *
 * @param[out] addr   
 *      where to assign the port number.
 *
 * @param[in] port
 *      the port number to be assigned.
 *
 * @return            
 *      nothing.      
 */
extern void _net_sockaddr_set_port(net_sockaddr_t *addr, in_port_t port);
#define net_sockaddr_set_port(a, p) _net_sockaddr_set_port((a), (p))


/** Get a port number from a net_sockaddr_t structure.
 * The port number is automatically converted from network to host byte order.
 *
 * @param[in] addr    
 *      pointer to the net_sockaddr_t structure.
 *
 * @return
 *      the port number.
 */
extern in_port_t _net_sockaddr_get_port(const net_sockaddr_t *addr);
#define net_sockaddr_get_port(a) _net_sockaddr_get_port((a))


/** Set local address to be used as a source address when opening connections.
 *
 * @param[in] ip
 *      local IP address.
 *
 * @return
 *      nothing.
 */
extern void _net_local_address(net_addr_t ip);
#define net_local_address(ip) _net_local_address((ip))


/** Create a socket and bind it to a specified address and port.
 *
 * @param[in] ip
 *      IP address to bind the socket to.
 *
 * @param[inout] port
 *      port number to bind the socket to. When this function is called with
 *      port == 0, the automatic port number the socket is bound to will be
 *      set in port after the function returns successfully.
 *
 * @param[in] priv
 *      nonzero when the port is required to be between 0 and 1024.
 *      Has any sense only for automatic port number (i.e., port == 0).
 *
 * @param[in] reuse
 *      nonzero when SO_REUSEADDR socket option should be set.
 *
 * @return
 *      file descriptor of the socket or -1 on error.
 */
extern int _net_socket(net_addr_t ip, int *port, int priv, int reuse);
#define net_socket(ip, port, priv, r) _net_socket((ip), (port), (priv), (r))


/** Create a unix socket and start listening on it.
 *
 * @param path
 *      socket name.
 *
 * @return
 *      file descriptor of the socket or -1 on error.
 */
extern int _net_socket_un(const char *path);
#define net_socket_un(p) _net_socket_un((p))


/** Open a connection to a specified host and port.
 * Source IP address can be set using net_local_address().
 *
 * @param[in] ip
 *      IP address to connect to.
 *
 * @param[in] port
 *      port number to connect to.
 *
 * @param[in] priv
 *      nonzero when source port is required to be between 0 and 1024.
 *
 * @return
 *      file descriptor of the socket or -1 on error.
 */
extern int _net_connect(net_addr_t ip, int port, int priv);
#define net_connect(ip, port, priv) _net_connect((ip), (port), (priv))


/** Open a connection to a specified host and port.
 * Source IP address can be set using net_local_address().
 *
 * The function is similar to net_connect() with one exception: the host is
 * specified using its domain name (or a string representation of its IP
 * address). The name is resolved and the IP addresses returned are used for
 * connecting to the host. First, IPv6 addresses are used if current protocol
 * is NPROTO_IPV6, otherwise only IPv4 addresses are used. If none of the
 * addresses can be connected, the function fails.
 *
 * @param[in] hostname
 *      domain name or of the host.
 *
 * @param[in] port
 *      port number to connect to.
 *
 * @param[in] priv
 *      nonzero when source port is required to be between 0 and 1024.
 *
 * @return
 *      file descriptor of the socket of -1 on error.
 */
extern int _net_connect_name(const char *hostname, int port, int priv);
#define net_connect_name(h, port, priv) _net_connect_name((h), (port), (priv))


#ifndef NET_NO_IO

/** Read a line from a socket.
 * @sa line_read()
 */
extern char *_net_read(struct connection *conn,
                       char *buf,
                       int *pos,
                       int *len,
                       int size,
                       long timeout);
#define net_read(c, b, p, l, s, t) _net_read((c), (b), (p), (l), (s), (t))


/** Write a line to a socket.
 *
 * @param[in] conn
 *      connection to write the data into.
 *
 * @param[in] msg, ...
 *      format string and its parameters.
 *
 * @return
 *      nothing.
 */
extern void _net_write(struct connection *conn, const char *msg, ...);
#define net_write(c, msg...) _net_write((c), msg)


/** Get peer's address.
 *
 * @param[in] sock
 *      socket the peer is connected to.
 *
 * @return
 *      peer's address. Dynamically allocated, the caller is supposed to
 *      free it.
 */
extern char *_net_peer(struct connection *conn);
#define net_peer(c) _net_peer((c))

#endif

#endif

