#ifndef PBS_PORTABILITY6_H
#define PBS_PORTABILITY6_H

#define TORQUE_PROTO_IPV4 4
#define TORQUE_PROTO_IPV6 6

/* The following macros are for IPv6 support */
#define ENABLE_IPV6 1
#if defined(ENABLE_IPV6) && defined(HAVE_STRUCT_SOCKADDR_IN6)
#define TORQUE_WANT_IPV6 1
/* GET_PORT accepts a struct sockaddr_storage* and returns the port in it */
#define GET_PORT(a) \
  ((*a).ss_family == AF_INET) ?\
  (((struct sockaddr_in*)(a))->sin_port) :\
  (((struct sockaddr_in6*)(a))->sin6_port)
/* SET_PORT accepts a struct sockaddr_storage* and sets the port in it */
#define SET_PORT(a,port) \
  ((*a).ss_family == AF_INET) ?\
  (((struct sockaddr_in*)(a))->sin_port = port ) :\
  (((struct sockaddr_in6*)(a))->sin6_port = port )

#ifndef SINLEN
#ifdef HAS_SA_LEN
#define SINLEN(a) ((*a).sa_len)
#else
#define SINLEN(a) \
  ((*a).sa_family == AF_INET ?\
  sizeof(struct sockaddr_in) :\
  sizeof(struct sockaddr_in6))
#endif
#endif

/* beware of side effects when using this! it evaluates (a) two times! */
#define IS_INET(a) (PBS_SOCK_INET == (a).sa_family || PBS_SOCK_INET6 == (a).sa_family)

#else
#undef TORQUE_WANT_IPV6

#define GET_PORT(a) (((struct sockaddr_in*)(a))->sin_port)
#define SET_PORT(a,port) (((struct sockaddr_in*)(a))->sin_port = port)
#define SINLEN(a) (sizeof(struct sockaddr_in))
#define SOCK_L(a) (ntohl(((struct sockaddr_in*)&a)->sin_addr.s_addr))

#define IS_INET(a) (PBS_SOCK_INET == (a).sa_family)
#endif

#ifndef HAVE_GETADDRINFO
#define getaddrinfo fake_getaddrinfo
#define freeaddrinfo fake_freeaddrinfo

struct addrinfo;
static int getaddrinfo(const char *hostname, struct addrinfo *ai, in_port_t port);
static void freeaddrinfo(struct addrinfo *ai);
#endif

#if !defined(HAVE_STRUCT_SOCKADDR_STORAGE)
#define sockaddr_storage sockaddr
#define ss_family sa_family
#endif

/* #if !defined(HAVE_STRUCT_SOCKADDR_IN6) */
#if !defined(TORQUE_WANT_IPV6)
/*#define sockaddr_in6 sockaddr_in
#define sin6_addr sin_addr
#define sin6_len sin_len
#define sin6_family sin_family
#define sin6_port sin_port */
#endif

#if 0
#if defined ENABLE_IPV6 && !defined HAVE_STRUCT_SOCKADDR_IN6
typedef struct my_in6_addr {
    union {
        uint32_t u6_addr32[4];
        struct _my_in6_addr {
            struct in_addr _addr_inet;
            uint32_t _pad[3];
        } _addr__inet;
    } _union_inet;
} in6_addr;
#endif
#endif

#endif
