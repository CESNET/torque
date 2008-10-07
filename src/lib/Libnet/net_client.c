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

#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include "portability.h"
#include "server_limits.h"
#include "net_connect.h"
#include "mcom.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>


/**
 * Returns the max number of possible file descriptors (as
 * per the OS limits).
 *
 */

int get_max_num_descriptors(void)
  {
  static int max_num_descriptors = 0;

  if (max_num_descriptors <= 0)
    max_num_descriptors = getdtablesize();

  return(max_num_descriptors);
  }  /* END get_num_max_descriptors() */

/**
 * Returns the number of bytes needed to allocate
 * a fd_set array that can hold all of the possible
 * socket descriptors.
 */

int get_fdset_size(void)
  {
  unsigned int MaxNumDescriptors = 0;
  int NumFDSetsNeeded = 0;
  int NumBytesInFDSet = 0;
  int Result = 0;

  MaxNumDescriptors = get_max_num_descriptors();

  NumBytesInFDSet = sizeof(fd_set);
  NumFDSetsNeeded = MaxNumDescriptors / FD_SETSIZE;

  if (MaxNumDescriptors < FD_SETSIZE)
    {
    /* the default size already provides sufficient space */

    Result = NumBytesInFDSet;
    }
  else if ((MaxNumDescriptors % FD_SETSIZE) > 0)
    {
    /* we need to allocate more memory to cover extra
     * bits--add an extra FDSet worth of memory to the size */

    Result = (NumFDSetsNeeded + 1) * NumBytesInFDSet;
    }
  else
    {
    /* division was exact--we know exactly how many bytes we need */

    Result = NumFDSetsNeeded * NumBytesInFDSet;
    }

  return(Result);
  }  /* END get_fdset_size() */


/*
** wait for connect to complete.  We use non-blocking sockets,
** so have to wait for completion this way.
*/

static int await_connect(

  long timeout,   /* I */
  int sockd)      /* I */

  {
  int n, val, rc;

  int MaxNumDescriptors = 0;

  fd_set *BigFDSet = NULL;

  struct timeval tv;

  torque_socklen_t len;

  /* some operating systems (like FreeBSD) cannot have a value for tv.tv_usec larger than 1,000,000
   * so we need to split up the timeout duration between seconds and microseconds */

  tv.tv_sec = timeout / 1000000;
  tv.tv_usec = timeout % 1000000;

  /* calculate needed size for fd_set in select() */

  MaxNumDescriptors = get_max_num_descriptors();

  BigFDSet = (fd_set *)calloc(1,sizeof(char) * get_fdset_size());

  FD_SET(sockd, BigFDSet);

  if ((n = select(sockd+1,0,BigFDSet,0,&tv)) != 1)
    {
    /* FAILURE:  socket not ready for write */

    free(BigFDSet);
    return(-1);
    }
 
  len = sizeof(val);

  rc = getsockopt(sockd,SOL_SOCKET,SO_ERROR,&val,&len);

  if ((rc == 0) && (val == 0))
    {
    /* SUCCESS:  no failures detected */

    free(BigFDSet);
    return(0);
    }

  errno = val;

  /* FAILURE:  socket error detected */

  free(BigFDSet);
  return(-1);
  }  /* END await_connect() */



/* in microseconds */
#define TORQUE_MAXCONNECTTIMEOUT  5000000

/*
 * client_to_svr - connect to a server
 *
 *	Perform socket/tcp/ip stuff to connect to a server.
 *
 *	Returns: >=0 the socket obtained, or
 *		 PBS_NET_RC_FATAL (-1) if fatal error, just quit, or
 *		 PBS_NET_RC_RETRY (-2) if temp error, should retry
 *
 * NOTE: the server's host address and port were chosen as parameters
 * rather than their names to possibly save extra look-ups.  It seems likely
 * that the caller "might" make several calls to the same host or different
 * hosts with the same port.  Let the caller keep the addresses around
 * rather than look it up each time.
 *
 * NOTE:  will wait up to TORQUE_MAXCONNECTTIMEOUT seconds for transient network failures
 */

/* NOTE:  create new connection on reserved port to validate root/trusted authority */

int client_to_svr(

  pbs_net_t     hostaddr,	  /* I - internet addr of host */
  unsigned int  port,		    /* I - port to which to connect */
  int           local_port,	/* I - BOOLEAN:  not 0 to use local reserved port */
  char         *EMsg)       /* O (optional,minsize=1024) */

  {
  const char id[] = "client_to_svr";

  struct sockaddr_in local;
  struct sockaddr_in remote;
  int                sock;
  unsigned short     tryport;
  int                flags;
  int                one = 1;

  int                trycount;
 
  if (EMsg != NULL)
    EMsg[0] = '\0';

  errno = 0;
 
  local.sin_family      = AF_INET;
  local.sin_addr.s_addr = 0;
  local.sin_port        = 0;

  tryport = IPPORT_RESERVED - 1;

retry:  /* retry goto added (rentec) */

  /* get socket */

  sock = socket(AF_INET,SOCK_STREAM,0);

  if (sock < 0) 
    {
    if (EMsg != NULL)
      sprintf(EMsg,"cannot create socket in %s - errno: %d %s",
        id,
        errno,
        strerror(errno));

    return(PBS_NET_RC_FATAL);
    } 

  if (sock >= PBS_NET_MAX_CONNECTIONS) 
    {
    if (EMsg != NULL)
      sprintf(EMsg,"PBS_NET_MAX_CONNECTIONS exceeded in %s",
        id);

    close(sock);		/* too many connections */

    return(PBS_NET_RC_RETRY);
    }

#ifndef NOPRIVPORTS
  flags = fcntl(sock,F_GETFL);
  flags |= O_NONBLOCK;

  fcntl(sock,F_SETFL,flags);
#endif /* !NOPRIVPORTS */

  /* If local privilege port requested, bind to one */
  /* must be root privileged to do this	*/

  trycount = 0;

  if (local_port != FALSE) 
    {
    /* set REUSEADDR (rentec) */

    setsockopt(
      sock,
      SOL_SOCKET,
      SO_REUSEADDR,
      (void *)&one, 
      sizeof(one));

#ifndef NOPRIVPORTS

#ifdef HAVE_BINDRESVPORT
    /*
     * bindresvport seems to cause connect() failures in some odd corner case when
     * talking to a local daemon.  So we'll only try this once and fallback to
     * the slow loop around bind() if connect() fails with EADDRINUSE
     * or EADDRNOTAVAIL.
     * http://www.supercluster.org/pipermail/torqueusers/2006-June/003740.html
     */

    if (tryport == (IPPORT_RESERVED - 1))
      {
      if (bindresvport(sock,&local) < 0)
        {
        if (EMsg != NULL)
          sprintf(EMsg,"cannot bind to reserved port in %s",
            id);

        close(sock);

        return(PBS_NET_RC_FATAL);
        }
      }
    else
      {
#endif /* HAVE_BINDRESVPORT */

      local.sin_port = htons(tryport);

      while (bind(sock,(struct sockaddr *)&local,sizeof(local)) < 0) 
        {
#ifdef NDEBUG2
        fprintf(stderr,"INFO:  cannot bind to port %d, errno: %d - %s\n",
          tryport,
          errno,
          strerror(errno));
#endif /* NDEBUG2 */

        if ((errno != EADDRINUSE) && (errno != EADDRNOTAVAIL)) 
          {
          if (EMsg != NULL)
            sprintf(EMsg,"cannot bind to port %d in %s - errno: %d %s",
              tryport,
              id,
              errno,
              strerror(errno));

          close(sock);

          return(PBS_NET_RC_FATAL);
          } 

        trycount++;
   
        if (--tryport < (unsigned short)(IPPORT_RESERVED / 2)) 
          {
          if (EMsg != NULL)
            sprintf(EMsg,"cannot bind to port %d in %s - too many retries",
              tryport,
              id);

          close(sock);

          return(PBS_NET_RC_RETRY);
          }

        local.sin_port = htons(tryport);
        }  /* END while (bind() < 0) */
#ifdef HAVE_BINDRESVPORT
      }    /* END if (tryport == (IPPORT_RESERVED - 1)) else */
#endif     /* HAVE_BINDRESVPORT */
#endif     /* !NOPRIVPORTS */
    }      /* END if (local_port != FALSE) */

  /* bind successful!!! */
			
  /* connect to specified server host and port	*/

  remote.sin_addr.s_addr = htonl(hostaddr);
  remote.sin_port = htons((unsigned short)port);
  remote.sin_family = AF_INET;

  if (connect(sock,(struct sockaddr *)&remote,sizeof(remote)) >= 0)
    {
    /* SUCCESS */

    return(sock);
    }

#ifdef NDEBUG2
  fprintf(stderr,"INFO:  cannot connect to port %d, errno=%d - %s\n",
    tryport,
    errno,
    strerror(errno));
#endif /* NDEBUG2 */

  /* process failure */

  switch (errno) 
    {
    case EADDRINUSE:
    case EADDRNOTAVAIL:

#ifndef NOPRIVPORTS
      if (local_port != FALSE) 
        {
        /* continue port search (rentec) */

        close(sock);

        --tryport;

        goto retry;
        }
#endif /* NOPRIVPORTS */

      /* fall through to next case */

    case EINTR:
    case ETIMEDOUT:
    case EINPROGRESS:   

      if (await_connect(TORQUE_MAXCONNECTTIMEOUT,sock) == 0)
        {
        /* socket not ready for writing after TORQUE_MAXCONNECTTIMEOUT second timeout */
        /* no network failures detected */

        break;
        }

      /* fall through to next case */

    case ECONNREFUSED:

      if (EMsg != NULL)
        sprintf(EMsg,"cannot connect to port %d in %s - connection refused",
          tryport,
          id);

      close(sock);

      return(PBS_NET_RC_RETRY);

      /*NOTREACHED*/

      break;

    default:

      if (EMsg != NULL)
        sprintf(EMsg,"cannot connect to port %d in %s - errno:%d %s",
          tryport,
          id,
          errno,
          strerror(errno));

      close(sock);

      return(PBS_NET_RC_FATAL);

      /*NOTREACHED*/

      break;
    }  /* END switch (errno) */

  /* SUCCESS */
			
  return(sock);
  }  /* END client_to_svr() */

/* END net_client.c */


