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
 * encode_DIS_reply() - encode a Batch Protocol Reply Structure
 *
 *  batch_reply structure defined in libpbs.h, it must be allocated
 * by the caller.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "libpbs.h"
#include "list_link.h"
#include "attribute.h"
#include "dis.h"
#include "batch_request.h"



int
encode_DIS_reply(int sock, struct batch_reply *reply)
  {
  int      ct;
  int      i;

  struct brp_select  *psel;

  struct brp_status  *pstat;
  svrattrl    *psvrl;

  int rc;

  /* first encode "header" consisting of protocol type and version */

  if ((rc = diswui(sock, PBS_BATCH_PROT_TYPE))   ||
      (rc = diswui(sock, PBS_BATCH_PROT_VER)))
    return rc;

  /* next encode code, auxcode and choice (union type identifier) */

  if ((rc = diswsi(sock, reply->brp_code))  ||
      (rc = diswsi(sock, reply->brp_auxcode)) ||
      (rc = diswui(sock, reply->brp_choice)))
    return rc;

  switch (reply->brp_choice)
    {

    case BATCH_REPLY_CHOICE_NULL:
      break; /* no more to do */

    case BATCH_REPLY_CHOICE_Queue:

    case BATCH_REPLY_CHOICE_RdytoCom:

    case BATCH_REPLY_CHOICE_Commit:

      if ((rc = diswst(sock, reply->brp_un.brp_jid)))
        return (rc);

      break;

    case BATCH_REPLY_CHOICE_Select:

      /* have to send count of number of strings first */

      ct = 0;

      psel = reply->brp_un.brp_select;

      while (psel)
        {
        ++ct;
        psel = psel->brp_next;
        }

      if ((rc = diswui(sock, ct)))
        return rc;

      psel = reply->brp_un.brp_select;

      while (psel)
        {
        /* now encode each string */
        if ((rc = diswst(sock, psel->brp_jobid)))
          return rc;

        psel = psel->brp_next;
        }

      break;

    case BATCH_REPLY_CHOICE_Status:

      /* encode "server version" of status structure.
       *
       * Server always uses svrattrl form.
       * Commands decode into their form.
       *
       * First need to encode number of status objects and then
       * the object itself.
       */

      ct = 0;
      pstat = (struct brp_status *)GET_NEXT(reply->brp_un.brp_status);

      while (pstat)
        {
        ++ct;
        pstat = (struct brp_status *)GET_NEXT(pstat->brp_stlink);
        }

      if ((rc = diswui(sock, ct)))
        return rc;

      pstat = (struct brp_status *)GET_NEXT(reply->brp_un.brp_status);

      while (pstat)
        {
        if ((rc = diswui(sock, pstat->brp_objtype)) ||
            (rc = diswst(sock, pstat->brp_objname)))
          return rc;

        psvrl = (svrattrl *)GET_NEXT(pstat->brp_attr);

        if ((rc = encode_DIS_svrattrl(sock, psvrl)))
          return rc;

        pstat = (struct brp_status *)GET_NEXT(pstat->brp_stlink);
        }

      break;

    case BATCH_REPLY_CHOICE_Text:

      /* text reply */

      if ((rc = diswcs(sock, reply->brp_un.brp_txt.brp_str,
                       reply->brp_un.brp_txt.brp_txtlen)))
        return rc;

      break;

    case BATCH_REPLY_CHOICE_Locate:

      /* Locate Job Reply */

      if ((rc = diswst(sock, reply->brp_un.brp_locate)))
        return rc;

      break;

    case BATCH_REPLY_CHOICE_RescQuery:

      /* Query Resources Reply */

      ct = reply->brp_un.brp_rescq.brq_number;

      if ((rc = diswui(sock, ct)))
        return rc;

      for (i = 0; (i < ct) && (rc == 0); ++i)
        {
        rc = diswui(sock, *(reply->brp_un.brp_rescq.brq_avail + i));
        }

      if (rc) return rc;

      for (i = 0; (i < ct) && (rc == 0); ++i)
        {
        rc = diswui(sock, *(reply->brp_un.brp_rescq.brq_alloc + i));
        }

      if (rc) return rc;

      for (i = 0; (i < ct) && (rc == 0); ++i)
        {
        rc = diswui(sock, *(reply->brp_un.brp_rescq.brq_resvd + i));
        }

      if (rc) return rc;

      for (i = 0; (i < ct) && (rc == 0); ++i)
        {
        rc = diswui(sock, *(reply->brp_un.brp_rescq.brq_down + i));
        }

      if (rc) return rc;

      break;

    default:
      return -1;
    }

  return 0;
  }
