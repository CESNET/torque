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
/*
 * req_getcred.c
 *
 * This file contains function relating to the PBS credential system,
 * it includes the major functions:
 *   req_authenuser - Authenticate a user connection based on pbs_iff  (new)
 *   req_connect    - validate the credential in a Connection Request (old)
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "credential.h"
#include "net_connect.h"
#include "batch_request.h"

/* External Global Data Items Referenced */

extern time_t time_now;

extern struct connection svr_conn[];

/* Global Data Home in this file */

struct credential conn_credent[PBS_NET_MAX_CONNECTIONS];
/* there is one per possible connectinn */

/*
 * req_connect - process a Connection Request
 *  Almost does nothing.
 */

void req_connect(

  struct batch_request *preq)

  {
  int  sock = preq->rq_conn;

  if (svr_conn[sock].cn_authen == 0)
    {
    reply_ack(preq);
    }
  else
    {
    req_reject(PBSE_BADCRED, 0, preq, NULL, NULL);
    }

  return;
  }  /* END req_connect() */





/*
 * req_authenuser - Authenticate a user connection based on the (new)
 * pbs_iff information.  pbs_iff will contact the server on a privileged
 * port and identify the user who has made an existing, but yet unused,
 * non-privileged connection.  This connection is marked as authenticated.
 */

void req_authenuser(

  struct batch_request *preq)  /* I */

  {
  int s;

  /*
   * find the socket whose client side is bound to the port named
   * in the request
   */

  for (s = 0;s < PBS_NET_MAX_CONNECTIONS;++s)
    {
    if (get_connectaddr(preq->rq_conn) != svr_conn[s].cn_addr)
      {
      continue;
      }
    
    if (preq->rq_ind.rq_authen.rq_port != svr_conn[s].cn_port)
      {
      continue;
      }

#ifndef NOPRIVPORTS
    if (svr_conn[s].cn_authen == 0)
#endif
      {
      strcpy(conn_credent[s].username, preq->rq_user);
      strcpy(conn_credent[s].hostname, preq->rq_host);

      /* time stamp just for the record */

      conn_credent[s].timestamp = time_now;

      svr_conn[s].cn_authen = PBS_NET_CONN_AUTHENTICATED;
      }

    reply_ack(preq);

    /* SUCCESS */

    return;
    }  /* END for (s) */

  req_reject(PBSE_BADCRED, 0, preq, NULL, "cannot authenticate user");

  /* FAILURE */

  return;
  }  /* END req_authenuser() */

/* END req_getcred.c */

