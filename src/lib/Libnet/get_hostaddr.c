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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <memory.h>
#if defined(NTOHL_NEEDS_ARPA_INET_H) && defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#include "server_limits.h"
#include "net_connect.h"
#include "pbs_error.h"
#include "portability6.h"


#if !defined(H_ERRNO_DECLARED)
extern int h_errno;
#endif

/*
 * get_hostaddr.c - contains functions to provide the internal
 * internet address for a host and to provide the port
 * number for a service.
 *
 * get_hostaddr - get internal internet address of a host
 *
 * Returns a string containing the network address in host byte order.  A Null
 * value is returned on error.
 */


char *PAddrToString(

  const struct sockaddr_storage *Addr)

  {
  static char tmpLine[NI_MAXHOST];
#ifdef TORQUE_WANT_IPV6
  int addrlen;
#endif

  if (NULL != Addr)
    {

#ifdef TORQUE_WANT_IPV6
    addrlen = SINLEN(Addr);

    if (getnameinfo((struct sockaddr *)Addr, addrlen, tmpLine, NI_MAXHOST,
                    NULL, 0, NI_NUMERICHOST))
      {
      return(NULL);
      }

#else
    sprintf(tmpLine, "%lu", *(unsigned long *)ntohl(((struct sockaddr_in *)Addr)->sin_addr.s_addr));

#endif
    }

  return(tmpLine);
  }





int get_hostaddr(

  char *hostname,                /* I */
  struct sockaddr_storage *ret)  /* O */

  {
#ifdef TORQUE_WANT_IPV6
  struct addrinfo       *addr, hints;
  struct addrinfo       *res = NULL;
  int                    error;
#else

  struct hostent        *hp;
#endif

  extern int pbs_errno;

#ifdef TORQUE_WANT_IPV6
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;

  error = getaddrinfo(hostname, NULL, &hints, &addr);

  if (0 == error)
    {
    for (res = addr; res != NULL; res = addr->ai_next)
      {
      int sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sfd == -1)
        continue;

      if (bind(sfd, res->ai_addr, res->ai_addrlen) == 0)
        break;                  /* Success */

      close(sfd);
      }

    /* no address succeeded */
    if (NULL == res)
      {
      pbs_errno = PBS_NET_RC_FATAL;
      return(pbs_errno);
      }

    memcpy(&ret, res->ai_addr, res->ai_addrlen);
    }
  else
    {
    pbs_errno = (EAI_AGAIN == error) ? PBS_NET_RC_RETRY : PBS_NET_RC_FATAL;
    freeaddrinfo(res);
    return(error);
    }

#else
  hp = gethostbyname(hostname);

  if (hp == NULL)
    {
    if (h_errno == TRY_AGAIN)
      pbs_errno = PBS_NET_RC_RETRY;
    else
      pbs_errno = PBS_NET_RC_FATAL;

    return(h_errno);
    }

  memcpy(&((struct sockaddr_in*)ret)->sin_addr, hp->h_addr, hp->h_length);

#endif

  return 0;
  }  /* END get_hostaddr() */

/* END get_hostaddr.c */

