/* $Id: net.c,v 1.19 2008/10/01 14:48:19 xdenemar Exp $ */

/** @file
 * Network functions for magrathea.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.19 $ $Date: 2008/10/01 14:48:19 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

#if !defined(NET_NO_LOG) || !defined(NET_NO_IO)
# include "utils.h"
#endif

#include "net.h"


static sa_family_t family = PF_INET;

net_addr_t net_inaddr_any = { .ip4 = { INADDR_ANY } };
const char *net_inaddr_any_str = "0.0.0.0";
static net_addr_t local_address = { .ip4 = { INADDR_ANY } };


void _net_proto(enum net_protocol proto)
{/*{{{*/
    family = PF_INET;

#if IPv6
    if (proto == NPROTO_AUTO || proto == NPROTO_IPV6) {
        int sock;
        struct sockaddr_in6 addr;

        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = 0;

        if ((sock = socket(PF_INET6, SOCK_STREAM, 0)) >= 0
            && bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == 0)
            family = PF_INET6;
# ifndef NET_NO_LOG
        else
            log_msg(LOG_ERR, errno, "Cannot create IPv6 socket");
# endif
    }

    if (family == PF_INET6) {
        net_inaddr_any.ip6 = in6addr_any;
        net_inaddr_any_str = "::";
# ifndef NET_NO_LOG
        log_msg(LOG_INFO, 0, "Using IPv6 protocol");
# endif
    }
    else
#endif
    {
        net_inaddr_any.ip4.s_addr = INADDR_ANY;
        net_inaddr_any_str = "0.0.0.0";
#ifndef NET_NO_LOG
        log_msg(LOG_INFO, 0, "Using IPv4 protocol");
#endif
    }

    local_address = net_inaddr_any;
}/*}}}*/


int _net_addr_equal(net_addr_t ip1, net_addr_t ip2)
{/*{{{*/
#if IPv6
    if (family == PF_INET6)
        return IN6_ARE_ADDR_EQUAL(&ip1, &ip2);
    else
#endif
        return (ip1.ip4.s_addr == ip2.ip4.s_addr);
}/*}}}*/


enum net_addr_type _net_addr_type(net_addr_t ip)
{/*{{{*/
#if IPv6
    if (family == PF_INET6) {
        if (IN6_IS_ADDR_V4MAPPED(&ip.ip6))
            return NADDR_IPV6_MAPPED_IPV4;
        else
            return NADDR_IPV6;
    }
#endif

    return NADDR_IPV4;
}/*}}}*/


const char *_net_addr_str(net_addr_t ip)
{/*{{{*/
#if IPv6
    static char str[INET6_ADDRSTRLEN];

    if (family == PF_INET6) {
        if (IN6_IS_ADDR_V4MAPPED(&ip.ip6)) {
            struct in_addr ip4 = { ip.ip6.s6_addr32[3] };
            return inet_ntoa(ip4);
        }

        *str = '\0';
        inet_ntop(PF_INET6, &ip.ip6, str, sizeof(str));

        return str;
    }
    else
#endif
        return inet_ntoa(ip.ip4);
}/*}}}*/


int _net_addr_parse(const char *str, net_addr_t *ip)
{/*{{{*/
    struct in_addr ip4;
    int valid_ip4;

    valid_ip4 = (inet_pton(AF_INET, str, &ip4) > 0);

#if IPv6
    if (family == PF_INET6) {
        if (valid_ip4) {
            /* make it IPv6-mapped IPv4 address */
            ip->ip6.s6_addr32[0] = 0;
            ip->ip6.s6_addr32[1] = 0;
            ip->ip6.s6_addr32[2] = htonl(0xffff);
            ip->ip6.s6_addr32[3] = ip4.s_addr;
            return 1;
        }
        else
            return (inet_pton(AF_INET6, str, ip) > 0);
    }
#endif

    ip->ip4 = ip4;
    return valid_ip4;
}/*}}}*/


net_addr_t *_net_addr_resolve(const char *hostname, int *count)
{/*{{{*/
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *r;
    net_addr_t *list;
    int size = 0;
    int i;

    memset(&hints, '\0', sizeof(hints));

#if IPv6
    if (family == PF_INET6)
        hints.ai_family = AF_UNSPEC;
    else
#endif
        hints.ai_family = AF_INET;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, NULL, &hints, &res)) {
#ifndef NET_NO_LOG
        log_msg(LOG_ERR, 0, "Cannot resolve address for %s: %s",
                hostname, gai_strerror(errno));
#endif
        return NULL;
    }

    for (r = res, size = 0; r != NULL; r = r->ai_next)
        size++;

    if (size == 0
        || (list = calloc(size, sizeof(net_addr_t))) == NULL)
        return NULL;

    i = 0;

#if IPv6
    /* first take IPv6 addresses if requested */
    if (family == PF_INET6) {
        for (r = res, size = 0; r != NULL; r = r->ai_next) {
            if (r->ai_family == AF_INET6) {
                list[i++].ip6 =
                    ((struct sockaddr_in6 *) r->ai_addr)->sin6_addr;
            }
        }
    }
#endif

    /* then take IPv4 address and map them to IPv6 if requested */
    for (r = res, size = 0; r != NULL; r = r->ai_next) {
        if (r->ai_family == AF_INET) {
#if IPv6
            if (family == PF_INET6) {
                list[i].ip6.s6_addr32[0] = 0;
                list[i].ip6.s6_addr32[1] = 0;
                list[i].ip6.s6_addr32[2] = htonl(0xffff);
                list[i].ip6.s6_addr32[3] =
                    ((struct sockaddr_in *) r->ai_addr)->sin_addr.s_addr;
                i++;
            }
            else
#endif
                list[i++].ip4 = ((struct sockaddr_in *) r->ai_addr)->sin_addr;
        }
    }

    if ((*count = i) == 0) {
        free(list);
        return NULL;
    }
    else
        return list;
}/*}}}*/


void _net_sockaddr_set_ip(net_sockaddr_t *addr, net_addr_t ip)
{/*{{{*/
#if IPv6
    if (family == PF_INET6) {
        addr->sin6.sin6_family = AF_INET6;
        addr->sin6.sin6_addr = ip.ip6;
    }
    else
#endif
    {
        addr->sin.sin_family = AF_INET;
        addr->sin.sin_addr = ip.ip4;
    }
}/*}}}*/


net_addr_t _net_sockaddr_get_ip(const net_sockaddr_t *addr)
{/*{{{*/
    net_addr_t ip;

#if IPv6
    if (family == PF_INET6)
        ip.ip6 = addr->sin6.sin6_addr;
    else
#endif
        ip.ip4 = addr->sin.sin_addr;

    return ip;
}/*}}}*/


void _net_sockaddr_set_port(net_sockaddr_t *addr, in_port_t port)
{/*{{{*/
#if IPv6
    if (family == PF_INET6)
        addr->sin6.sin6_port = htons(port);
    else
#endif
        addr->sin.sin_port = htons(port);
}/*}}}*/


in_port_t _net_sockaddr_get_port(const net_sockaddr_t *addr)
{/*{{{*/
#if IPv6
    if (family == PF_INET6)
        return ntohs(addr->sin6.sin6_port);
    else
#endif
        return ntohs(addr->sin.sin_port);
}/*}}}*/


void _net_local_address(net_addr_t ip)
{/*{{{*/
    local_address = ip;
}/*}}}*/


int _net_socket(net_addr_t ip, int *port, int priv, int reuse)
{/*{{{*/
    int sock;
    net_sockaddr_t addr;

    net_sockaddr_set_ip(&addr, ip);
    net_sockaddr_set_port(&addr, *port);

    if ((sock = socket(family, SOCK_STREAM, 0)) < 0) {
#ifndef NET_NO_LOG
        log_msg(LOG_ERR, errno, "Cannot create socket");
#endif
        return -1;
    }

    if (reuse) {
        int reuse_addr = 1;

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                       (void *) &reuse_addr, sizeof(reuse_addr))) {
#ifndef NET_NO_LOG
            log_msg(LOG_ERR, errno, "Cannot set SO_REUSEADDR option");
#endif
            close(sock);
            return -1;
        }
    }

    if (*port == 0 && priv) {
        int i;
        int bound = 0;

        for (i = 1023; i > 0; i--) {
            net_sockaddr_set_port(&addr, i);
            if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == 0) {
                bound = 1;
                break;
            }
        }

        if (!bound) {
#ifndef NET_NO_LOG
            log_msg(LOG_ERR, errno, "Cannot bind socket to a privileged port");
#endif
            close(sock);
            return -1;
        }
    }
    else {
        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
#ifndef NET_NO_LOG
            log_msg(LOG_ERR, errno, "Cannot bind socket to %s port %d",
                    net_addr_str(ip), *port);
#endif
            close(sock);
            return -1;
        }
    }

    if (*port == 0) {
        unsigned int len = sizeof(addr);

        if (getsockname(sock, (struct sockaddr *) &addr, &len)) {
#ifndef NET_NO_LOG
            log_msg(LOG_ERR, errno, "Cannot get socket name");
#endif
            close(sock);
            return -1;
        }

        *port = net_sockaddr_get_port(&addr);
    }

    return sock;
}/*}}}*/


int _net_socket_un(const char *path)
{/*{{{*/
    struct sockaddr_un addr;
    int sock;
    mode_t umask_old;

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, UNIX_PATH_MAX);
    addr.sun_path[UNIX_PATH_MAX - 1] = '\0';

    unlink(path);
    umask_old = umask(0077);

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0
        || bind(sock, (struct sockaddr *) &addr, sizeof(addr))
        || listen(sock, 1)) {
#ifndef NET_NO_LOG
        log_msg(LOG_ERR, errno, "Cannot create unix socket \"%s\"", path);
#endif
        umask(umask_old);
        return -1;
    }

    umask(umask_old);
    return sock;
}/*}}}*/


int _net_connect(net_addr_t ip, int port, int priv)
{/*{{{*/
    int sock;
    net_sockaddr_t addr;
    int port_auto = 0;

    if ((sock = net_socket(local_address, &port_auto, priv, 0)) < 0)
        return -1;

    net_sockaddr_set_ip(&addr, ip);
    net_sockaddr_set_port(&addr, port);

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr))) {
#ifndef NET_NO_LOG
        log_msg(LOG_ERR, errno, "Cannot connect to %s port %d",
                net_addr_str(ip), port);
#endif
        close(sock);
        return -1;
    }

    return sock;
}/*}}}*/


int _net_connect_name(const char *hostname, int port, int priv)
{/*{{{*/
    int sock;
    net_sockaddr_t addr;
    int port_auto = 0;
    net_addr_t *ips;
    int count = 0;
    int i;
    int found = -1;

    if ((sock = net_socket(local_address, &port_auto, priv, 0)) < 0)
        return -1;

    net_sockaddr_set_port(&addr, port);

    if ((ips = net_addr_resolve(hostname, &count)) == NULL
        || count == 0) {
        close(sock);
        return -1;
    }

#ifndef NET_NO_LOG
    for (i = 0; i < count; i++) {
        log_msg(LOG_NOTICE, 0, "Found %s address for %s: %s",
                (net_addr_type(ips[i]) == NADDR_IPV6) ? "IPv6" : "IPv4",
                hostname, net_addr_str(ips[i]));
    }
#endif

    for (i = 0; i < count; i++) {
        net_sockaddr_set_ip(&addr, ips[i]);
        if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == 0) {
            found = i;
            break;
        }
#ifndef NET_NO_LOG
        else {
            log_msg(LOG_ERR, errno, "Cannot connect to %s (%s) port %d",
                    hostname, net_addr_str(ips[i]), port);
        }
#endif
    }

    if (found == -1) {
#ifndef NET_NO_LOG
        log_msg(LOG_ERR, 0, "Cannot connect to %s port %d", hostname, port);
#endif
        free(ips);
        close(sock);
        return -1;
    }

#ifndef NET_NO_LOG
    log_msg(LOG_INFO, 0, "Connected to %s (%s) port %d",
            hostname, net_addr_str(ips[found]), port);
#endif

    free(ips);

    return sock;
}/*}}}*/


#ifndef NET_NO_IO

char *_net_read(struct connection *conn,
                char *buf,
                int *pos,
                int *len,
                int size,
                long timeout)
{/*{{{*/
    char *line;
    char *peer = net_peer(conn);

    line = line_read(conn, buf, pos, len, size, timeout);

# ifndef NET_NO_LOG
    if (line != NULL)
        log_msg(LOG_INFO, 0, "Data received from %s: \"%s\"", peer, buf);
# endif

    free(peer);

    return line;
}/*}}}*/


void _net_write(struct connection *conn, const char *msg, ...)
{/*{{{*/
    va_list args;
    char buf[MAX_BUF];
    size_t len;
    int ret;
    char *peer = net_peer(conn);

    va_start(args, msg);
    vsnprintf(buf, MAX_BUF - 1, msg, args);
    buf[MAX_BUF - 2] = '\0';
    va_end(args);

    len = strlen(buf);
    buf[len++] = '\n';
    buf[len] = '\0';

    ret = line_write(conn, buf, len);
    buf[len - 1] = '\0';

# ifndef NET_NO_LOG
    if (ret)
        log_msg(LOG_ERR, errno, "Cannot send data to %s: \"%s\"", peer, buf);
    else
        log_msg(LOG_INFO, 0, "Data sent to %s: \"%s\"", peer, buf);
# endif

    free(peer);
}/*}}}*/


char *_net_peer(struct connection *conn)
{/*{{{*/
    char *name;
    struct sockaddr_storage addr;
    unsigned int len = sizeof(addr);
    net_addr_t ip;
    int size = UNIX_PATH_MAX;

#if GSSAPI
    if (conn->gss) {
        if (conn->client != NULL)
            size += strlen(conn->client) + 13;
        else if (conn->server != NULL)
            size += strlen(conn->server) + 13;
        else
            size += 18;
    }
#endif

    if ((name = calloc(1, size)) == NULL)
        return NULL;

#if GSSAPI
    if (conn->gss) {
        if (conn->client != NULL)
            sprintf(name, "GSS client %s, ", conn->client);
        else if (conn->server != NULL)
            sprintf(name, "GSS server %s, ", conn->server);
        else
            strcpy(name, "GSS peer unknown, ");
    }
#endif

    if (!getpeername(conn->sock, (struct sockaddr *) &addr, &len)) {
        switch (addr.ss_family) {
        case AF_UNIX:
            strncpy(name + strlen(name), "unix socket", UNIX_PATH_MAX);
            break;

        case AF_INET:
#if IPv6
        case AF_INET6:
#endif
            ip = net_sockaddr_get_ip((net_sockaddr_t *) &addr);
            strncpy(name + strlen(name), net_addr_str(ip), UNIX_PATH_MAX);
            sprintf(name + strlen(name), " port %d",
                    net_sockaddr_get_port((net_sockaddr_t *) &addr));
            break;
        }
    }

    return name;
}/*}}}*/

#endif
