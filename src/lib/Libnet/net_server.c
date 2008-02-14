/*
*         OpenPBS (Portable Batch System) v2.3 Software License
* 
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
* 
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
* 
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
* 
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
* 
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
* 
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
* 
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
* 
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
* 
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
* 
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
* 
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information 
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
* 
* 7. DISCLAIMER OF WARRANTY
* 
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
* 
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
#include <pbs_config.h>   /* the master config generated by configure */

#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>  /* added - CRI 9/05 */
#include <unistd.h>    /* added - CRI 9/05 */
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(FD_SET_IN_SYS_SELECT_H)
#  include <sys/select.h>
#endif
#if defined(NTOHL_NEEDS_ARPA_INET_H) && defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif


#include "portability.h"
#include "portability6.h"
#include "server_limits.h"
#include "net_connect.h"
#include "log.h"

extern int LOGLEVEL;


/* External Functions Called */

extern void process_request A_((int));

extern time_t time();

/* Global Data (I wish I could make it private to the library, sigh, but
 * C don't support that scope of control.)
 *
 * This array of connection structures is used by the server to maintain 
 * a record of the open I/O connections, it is indexed by the socket number.  
 */

struct connection svr_conn[PBS_NET_MAX_CONNECTIONS];

/*
 * The following data is private to this set of network interface routines.
 */

static int	max_connection = PBS_NET_MAX_CONNECTIONS;
static int	num_connections = 0;
static fd_set	readset;
static void	(*read_func[2]) A_((int));
static enum     conn_type settype[2];		/* temp kludge */

struct sockaddr_storage pbs_server_addr;

/* Private function within this file */

static void accept_conn();


static struct netcounter nc_list[60];

void netcounter_incr()
  {
  time_t now, lastmin;
  int i;

  now = time(NULL);
  lastmin = now - 60;

  if (nc_list[0].time == now)
    {
    nc_list[0].counter++;
    }
  else 
    {
    memmove(&nc_list[1],&nc_list[0],sizeof(struct netcounter)*59);
    
    nc_list[0].time=now;
    nc_list[0].counter=1;

    for (i=0;i<60;i++)
      {
      if (nc_list[i].time < lastmin)
        {
        nc_list[i].time=0;
        nc_list[i].counter=0;
        }
      }
    }
  }

int *netcounter_get()
  {
  static int netrates[3];
  int netsums[3]={0,0,0};
  int i;
  
  for (i=0;i<5;i++)
    {
    netsums[0]+=nc_list[i].counter;
    netsums[1]+=nc_list[i].counter;
    netsums[2]+=nc_list[i].counter;
    }
  for (i=5;i<30;i++)
    {
    netsums[1]+=nc_list[i].counter;
    netsums[2]+=nc_list[i].counter;
    }
  for (i=30;i<60;i++)
    {
    netsums[2]+=nc_list[i].counter;
    }

  if (netsums[0] > 0)
    {
    netrates[0]=netsums[0]/5;
    netrates[1]=netsums[1]/30;
    netrates[2]=netsums[2]/60;
    }
  else
    {
    netrates[0]=0;
    netrates[1]=0;
    netrates[2]=0;
    }

  return netrates;
  }
    
    

/*
 * init_network - initialize the network interface
 *	allocate a socket and bind it to the service port,
 *	add the socket to the readset for select(),
 *	add the socket to the connection structure and set the
 *	processing function to accept_conn()
 */
	
int init_network(

  unsigned int port,
  void         (*readfunc)(),
  sa_family_t af_family
  )

  {
  int                 i;
  static int          initialized = 0;
  int                 sock;
  struct sockaddr_storage socname;
  int                 addrlen;
  enum conn_type      type;
#ifdef ENABLE_UNIX_SOCKETS
  struct sockaddr_un unsocname;
  int unixsocket;
#endif

  if (initialized == 0) 
    {
    for (i = 0;i < PBS_NET_MAX_CONNECTIONS;i++) 
      svr_conn[i].cn_active = Idle;

    FD_ZERO(&readset);

    type = Primary;
    } 
  else if (initialized == 1)
    {
    type = Secondary;
    }
  else 
    {
    return(-1);	/* too many main connections */
    }

  net_set_type(type,FromClientDIS);

  /* save the routine which should do the reading on connections	*/
  /* accepted from the parent socket				*/

  read_func[initialized++] = readfunc;

  if ((sock = socket(af_family, SOCK_STREAM, 0)) < 0) {
      return(-1);
  }

  if (FD_SETSIZE < PBS_NET_MAX_CONNECTIONS)
    max_connection = FD_SETSIZE;

  i = 1;

  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&i,sizeof(i));

    /* bind to all addresses and dynamically assigned port */
#if TORQUE_WANT_IPV6
        struct addrinfo hints, *res = NULL;
        int error;

        memset (&hints, 0, sizeof(hints));
        hints.ai_family = af_family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((error = getaddrinfo(NULL, "0", &hints, &res))) {
            return(-1);
        }

        memcpy (&socname, res->ai_addr, res->ai_addrlen);
        addrlen = res->ai_addrlen;
        freeaddrinfo (res);

#ifdef IPV6_V6ONLY
        /* in case of AF_INET6, disable v4-mapped addresses */
        if (AF_INET6 == af_family) {
            int flg = 0;
            if (setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
                            &flg, sizeof (flg)) < 0) {
                log_err(-1, "init_network",
                            "Failed to disable IPv4 mapped addresses");
            }
        }
#endif /* IPV6_V6ONLY */

#else /* !TORQUE_WANT_IPV6 */
  addrlen = sizeof(struct sockaddr_in);
  memset(&socname, 0, addrlen);
  ((struct sockaddr_in*)&socname)->sin_port= htons((unsigned short)port);
  ((struct sockaddr_in*)&socname)->sin_addr.s_addr = INADDR_ANY;
  ((struct sockaddr_in*)&socname)->sin_family = AF_INET;
#endif

  /* Bind-Listen Combo */
  if (bind(sock, (struct sockaddr *)&socname, addrlen) < 0) {
    close(sock);
    return(-1);
  }

  /* record socket in connection structure and select set */
  add_conn(sock, type, socname, accept_conn);

  /* start listening for connections */
  if (listen(sock, 512) < 0) {
    return(-1);
  }

#ifdef ENABLE_UNIX_SOCKETS
  /* setup unix domain socket */

  unixsocket=socket(AF_UNIX,SOCK_STREAM,0);
  if (unixsocket < 0) 
	{
	  return(-1);
	}

  unsocname.sun_family=AF_UNIX;
  strncpy(unsocname.sun_path,TSOCK_PATH,107);  /* sun_path is defined to be 108 bytes */

  unlink(TSOCK_PATH);  /* don't care if this fails */

  if (bind(unixsocket,
				  (struct sockaddr *)&unsocname,
				  strlen(unsocname.sun_path) + sizeof(unsocname.sun_family)) < 0) 
	{
	  close(unixsocket);

	  return(-1);
	}

  if (chmod(TSOCK_PATH,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) != 0)
	{
	  close(unixsocket);

	  return(-1);
	}

  add_conn(unixsocket,type,(pbs_net_t)0,0,PBS_SOCK_UNIX,accept_conn);
  if (listen(unixsocket,512) < 0) 
	{

	  return(-1);
	}
#endif /* END ENABLE_UNIX_SOCKETS */

  /* allocate a minute's worth of counter structs */

  for (i = 0;i < 60;i++)
    {
    nc_list[i].time = 0;
    nc_list[i].counter = 0;
    }

  return (0);
  }  /* END init_network() */





/*
 * wait_request - wait for a request (socket with data to read)
 *	This routine does a select on the readset of sockets,
 *	when data is ready, the processing routine associated with
 *	the socket is invoked.
 */

int wait_request(

  time_t  waittime,   /* I (seconds) */
  long   *SState)     /* I (optional) */

  {
  extern char *PAddrToString(struct sockaddr_storage *);

  int i;
  int n;

  time_t now;

  fd_set selset;

  char tmpLine[1024];

  struct timeval timeout;
  void close_conn();

  long OrigState = 0;

  if (SState != NULL)
    OrigState = *SState;

  timeout.tv_usec = 0;
  timeout.tv_sec  = waittime;

  selset = readset;  /* readset is global */

  n = select(FD_SETSIZE,&selset,(fd_set *)0,(fd_set *)0,&timeout);

  if (n == -1) 
    {
    if (errno == EINTR)
      {
      n = 0;	/* interrupted, cycle around */
      }
    else 
      {
      int i;
      struct stat fbuf;
     

      /* check all file descriptors to verify they are valid */

      /* NOTE:  selset may be modified by failed select() */

      for (i = 0;i < (int)FD_SETSIZE;i++)
        {
        if (FD_ISSET(i,&readset) == 0)
          continue;

        if (fstat(i,&fbuf) == 0)
          continue;

        /* clean up SdList and bad sd... */

        FD_CLR(i,&readset);
        }    /* END for (i) */
  
      return(-1);
      }  /* END else (errno == EINTR) */
    }    /* END if (n == -1) */

  for (i = 0;(i < max_connection) && (n != 0);i++) 
    {
    if (FD_ISSET(i,&selset)) 
      {	
      /* this socket has data */

      n--;

      svr_conn[i].cn_lasttime = time((time_t *)0);

      if (svr_conn[i].cn_active != Idle) 
        {
        netcounter_incr();
        svr_conn[i].cn_func(i);

        /* NOTE:  breakout if state changed (probably received shutdown request) */

        if ((SState != NULL) && (OrigState != *SState))
          break;
        } 
      else 
        {
        FD_CLR(i,&readset);

        close(i);

        num_connections--;  /* added by CRI - should this be here? */

        }
      }
    }    /* END for (i) */

  /* NOTE:  break out if shutdown request received */

  if ((SState != NULL) && (OrigState != *SState))
    {
    return(0);
    }

  /* have any connections timed out ?? */

  now = time((time_t *)0);

  for (i = 0;i < max_connection;i++) 
    {
    struct connection *cp;

    cp = &svr_conn[i];

    if ((cp->cn_active != FromClientASN) && (cp->cn_active != FromClientDIS))
      continue;

    if ((now - cp->cn_lasttime) <= PBS_NET_MAXCONNECTIDLE)
      continue;

    if (cp->cn_authen & PBS_NET_CONN_NOTIMEOUT)
      continue;	/* do not time-out this connection */

    /* NOTE:  add info about node associated with connection - NYI */

    snprintf(tmpLine,sizeof(tmpLine),"connection %d to host %s has timed out out after %d seconds - closing stale connection\n",
      i,
      PAddrToString(&cp->cn_addr),
      PBS_NET_MAXCONNECTIDLE);

    log_err(-1,"wait_request",tmpLine);

    /* locate node associated with interface, mark node as down until node responds */

    /* NYI */

    close_conn(i);
    }  /* END for (i) */
		
  return(0);
  }  /* END wait_request() */





/*
 * accept_conn - accept request for new connection
 *	this routine is normally associated with the main socket,
 *	requests for connection on the socket are accepted and
 *	the new socket is added to the select set and the connection
 *	structure - the processing routine is set to the external
 *	function: process_request(socket)
 */

static void accept_conn(

  int sd)  /* main socket with connection request pending */

  {
  int newsock;
  struct sockaddr_storage from;
  struct sockaddr_un unixfrom;

  torque_socklen_t fromsize;
	
  /* update lasttime of main socket */

  svr_conn[sd].cn_lasttime = time((time_t *)0);

  if (PBS_SOCK_INET == svr_conn[sd].cn_socktype) {
      fromsize = sizeof(struct sockaddr_in);
      newsock = accept(sd,(struct sockaddr *)&from,&fromsize);
  } else if (PBS_SOCK_INET6 == svr_conn[sd].cn_socktype) {
      fromsize = sizeof(struct sockaddr_in6);
      newsock = accept(sd,(struct sockaddr *)&from,&fromsize);
  } else {
      fromsize = sizeof(unixfrom);
      newsock = accept(sd,(struct sockaddr *)&unixfrom,&fromsize);
  }

  if (newsock == -1) 
    {
    return;
    }

  if ((num_connections >= max_connection) ||
      (newsock >= PBS_NET_MAX_CONNECTIONS)) 
    {
    close(newsock);

    return;		/* too many current connections */
    }
	
  /* add the new socket to the select set and connection structure */

  add_conn(
    newsock, 
    FromClientDIS, 
    from,
    read_func[(int)svr_conn[sd].cn_active]);

  return;
  }  /* END accept_conn() */




/*
 * add_conn - add a connection to the svr_conn array.
 *	The params addr and port are in host order.
 */

void add_conn(

  int            sock,	   /* socket associated with connection */
  enum conn_type type,	   /* type of connection */
  struct sockaddr_storage addr,	   /* IP address of connected host */
  void (*func) A_((int)))  /* function to invoke on data rdy to read */

  {
  num_connections++;

  FD_SET(sock,&readset);

  svr_conn[sock].cn_active   = type;
  svr_conn[sock].cn_addr     = addr;
  svr_conn[sock].cn_lasttime = time((time_t *)0);
  svr_conn[sock].cn_func     = func;
  svr_conn[sock].cn_oncl     = 0;
  svr_conn[sock].cn_socktype = addr.sa_family;

#ifndef NOPRIVPORTS
  if (IS_INET(addr) && GET_PORT(&addr) < IPPORT_RESERVED)
    svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
  else
    svr_conn[sock].cn_authen = 0;
#else /* !NOPRIVPORTS */
  svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
#endif /* NOPRIVPORTS */

  return;
  }  /* END add_conn() */
	



	
/*
 * close_conn - close a network connection
 *	does physical close, also marks the connection table
 */

void close_conn(

  int sd) /* I */

  {
  if ((sd < 0) || (max_connection <= sd))
    {
    return;
    }

  if (svr_conn[sd].cn_active == Idle)
    {
    return;
    }

  close(sd);

  /* if there is a function to call on close, do it */

  if (svr_conn[sd].cn_oncl != 0)
    svr_conn[sd].cn_oncl(sd);

  FD_CLR(sd,&readset);

  /* svr_conn[sd].cn_addr = 0; */
  svr_conn[sd].cn_handle = -1;
  svr_conn[sd].cn_active = Idle;
  svr_conn[sd].cn_func = (void (*)())0;
  svr_conn[sd].cn_authen = 0;

  num_connections--;

  return;
  }  /* END close_conn() */




/*
 * net_close - close all network connections but the one specified,
 *	if called with impossible socket number (-1), all will be closed.
 *	This function is typically called when a server is closing down and
 *	when it is forking a child.
 *
 *	We clear the cn_oncl field in the connection table to prevent any
 *	"special on close" functions from being called.
 */

void net_close(

  int but)  /* I */

  {
  int i;

  for (i = 0;i < max_connection;i++) 
    {
    if (i != but) 
      {
      svr_conn[i].cn_oncl = 0;

      close_conn(i);
      }
    }    /* END for (i) */

  return;
  }  /* END net_close() */




/*
 * get_connectaddr - return address of host connected via the socket
 *	This is in host order.
 */

/* FIXME: do we return the struct or a pointer to it? */
struct sockaddr_storage * get_connectaddr(

  int sock)  /* I */

  {
  return(&svr_conn[sock].cn_addr);
  }





int find_conn(

  struct sockaddr_storage* addr)  /* I */

{
    int index;

    /* NOTE:  there may be multiple connections per addr (not handled) */

    for (index = 0;index < PBS_NET_MAX_CONNECTIONS;index++) {
        if (compare_ip(addr, &svr_conn[index].cn_addr)) {
            return(index);
        }
    }    /* END for (index) */

    return(-1);
}  /* END find_conn() */





/*
 * get_connecthost - return name of host connected via the socket
 */

int get_connecthost(

  int   sock,     /* I */
  char *namebuf,  /* O (minsize=size) */
  int   size)     /* I */

{
    struct sockaddr_storage addr;

    static struct sockaddr_storage serveraddr;

    static char server_name[NI_MAXHOST];

#ifdef TORQUE_WANT_IPV6
    int                 addrlen, error = 0;
#else
    struct hostent      *phe;
    int namesize = 0;
#endif

    /* Assume that if the ss_family field is populated, the structure holds
     * valid data */
    if ((server_name == '\0') && (pbs_server_addr.ss_family != 0)) {
        /* cache local server addr info */

        serveraddr = pbs_server_addr;

#ifdef TORQUE_WANT_IPV6
        addrlen = SINLEN(serveraddr);
        error = getnameinfo((struct sockaddr *)serveraddr, addrlen,
                    server_name, NI_MAXHOST,
                    NULL, 0, 0);
        if (error) {
            log_err(-1, "get_connecthost", gai_strerror());
            return(-1);
        }
#else
        if (NULL == (phe = gethostbyaddr(
                            &((struct sockaddr_in*)&serveraddr)->sin_addr,
                            sizeof(struct in_addr),
                            AF_INET))) {
            strncpy(server_name, inet_ntoa(((struct sockaddr_in*)&serveraddr)->sin_addr), NI_MAXHOST);
        } else {
            strncpy(server_name, phe->h_name, NI_MAXHOST);
        }
#endif
    }

    if ((server_name != '\0') && compare_ip(&addr, &serveraddr)) {
        /* lookup request is for local server, use cached name */
        strncpy(namebuf, server_name, size);
    }
#ifdef TORQUE_WANT_IPV6
    else {
        addr = svr_conn[sock].cn_addr;

        error = getnameinfo((struct sockaddr *)addr, addrlen,
                    namebuf, size,
                    NULL, 0, 0);
        if (error) {
            log_err(-1, "get_connecthost", gai_strerror());
        }
    }

    if (EAI_OVERFLOW == error) {
        /* FAILURE - buffer too small (conform to historic return code) */
        return(-1);
    }
#else /* !TORQUE_WANT_IPV6 */
    else if ((phe = gethostbyaddr(
                        &((struct sockaddr_in*)&addr)->sin_addr,
                        sizeof(struct in_addr),
                        AF_INET)) == NULL) {
        strncpy(namebuf, inet_ntoa(((struct sockaddr_in*)&addr)->sin_addr), size);
    }
    else {
        namesize = strlen(phe->h_name);

        strncpy(namebuf,phe->h_name,size);

        *(namebuf + size) = '\0';
    }

    if (namesize > size) {
        /* FAILURE - buffer too small */
        return(-1);
    }
#endif

    /* SUCCESS */

    return(0);
}  /* END get_connecthost() */





/*
 * net_set_type() - a temp kludge for supporting two protocols during
 *	the conversion from ASN.1 to PBS DIS
 */

void net_set_type(

  enum conn_type which,
  enum conn_type type)

  {
  settype[(int)which] = type;

  return;
  }  /* END net_set_type() */


/* END net_server.c */

