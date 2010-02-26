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

/*
 * stat_job.c
 *
 * Functions which support the Status Job Batch Request.
 *
 * Included funtions are:
 * status_job()
 * status_attrib()
 */
#include <sys/types.h>
#include <stdlib.h>
#include "libpbs.h"
#include <ctype.h>
#include <stdio.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "queue.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "resource.h"


extern int     svr_authorize_jobreq A_((struct batch_request *, job *));
int status_attrib A_((svrattrl *, attribute_def *, attribute *, int, int, tlist_head *, int *, int));

/* Global Data Items: */

extern attribute_def job_attr_def[];
extern int      resc_access_perm; /* see encode_resc() in attr_fn_resc.c */

extern struct server server;
extern time_t time_now;





/**
 * status_job - Build the status reply for a single job.
 *
 * @see req_stat_job_step2() - parent
 * @see status_attrib() - child
 */

int status_job(

  job      *pjob, /* ptr to job to status */
  struct batch_request *preq,
  svrattrl   *pal, /* specific attributes to status */
  tlist_head *pstathd, /* RETURN: head of list to append status to */
  int        *bad) /* RETURN: index of first bad attribute */

  {

  struct brp_status *pstat;

  int IsOwner = 0;

  /* see if the client is authorized to status this job */

  if (svr_authorize_jobreq(preq, pjob) == 0)
    IsOwner = 1;

  if (!server.sv_attr[(int)SRV_ATR_query_others].at_val.at_long)
    {
    if (IsOwner == 0)
      {
      return(PBSE_PERM);
      }
    }

  /* allocate reply structure and fill in header portion */

  pstat = (struct brp_status *)malloc(sizeof(struct brp_status));

  if (pstat == NULL)
    {
    return(PBSE_SYSTEM);
    }

  CLEAR_LINK(pstat->brp_stlink);

  pstat->brp_objtype = MGR_OBJ_JOB;

  strcpy(pstat->brp_objname, pjob->ji_qs.ji_jobid);

  CLEAR_HEAD(pstat->brp_attr);

  append_link(pstathd, &pstat->brp_stlink, pstat);

  /* add attributes to the status reply */

  *bad = 0;

  if (status_attrib(
        pal,
        job_attr_def,
        pjob->ji_wattr,
        JOB_ATR_LAST,
        preq->rq_perm,
        &pstat->brp_attr,
        bad,
        IsOwner))
    {
    return(PBSE_NOATTR);
    }

  return (0);
  }  /* END status_job() */






/**
 * status_attrib - add each requested or all attributes to the status reply
 *
 *   Returns: 0 on success
 *           -1 on error (bad attribute), "bad" set to ordinal of attribute
 *
 * @see status_job() - parent
 * @see find_attr() - child
 * @see *->at_encode() - child
 */

int status_attrib(

  svrattrl *pal,      /* I */
  attribute_def *padef,
  attribute *pattr,
  int   limit,
  int   priv,
  tlist_head *phead,
  int  *bad,
  int            IsOwner)  /* 0 == FALSE, 1 == TRUE */

  {
  int   index;
  int   nth = 0;

  priv &= ATR_DFLAG_RDACC;  /* user-client privilege  */

  resc_access_perm = priv;  /* pass privilege to encode_resc() */

  /* for each attribute asked for or for all attributes, add to reply */

  if (pal != NULL)
    {
    /* client specified certain attributes */

    while (pal != NULL)
      {
      ++nth;

      index = find_attr(padef, pal->al_name, limit);

      if (index < 0)
        {
        *bad = nth;

        /* FAILURE */

        return(-1);
        }

      if ((padef + index)->at_flags & priv)
        {
        if (!(((padef + index)->at_flags & ATR_DFLAG_PRIVR) && (IsOwner == 0)))
          {
          (padef + index)->at_encode(
            pattr + index,
            phead,
            (padef + index)->at_name,
            NULL,
            ATR_ENCODE_CLIENT);
          }
        }

      pal = (svrattrl *)GET_NEXT(pal->al_link);
      }

    /* SUCCESS */

    return(0);
    }    /* END if (pal != NULL) */

  /* attrlist not specified, return all readable attributes */

  for (index = 0;index < limit;index++)
    {
    if (((padef + index)->at_flags & priv) &&
        !((padef + index)->at_flags & ATR_DFLAG_NOSTAT))
      {
      if (!(((padef + index)->at_flags & ATR_DFLAG_PRIVR) && (IsOwner == 0)))
        {
        (padef + index)->at_encode(
          pattr + index,
          phead,
          (padef + index)->at_name,
          NULL,
          ATR_ENCODE_CLIENT);

        /* add walltime remaining if started */
        if (index == JOB_ATR_start_time)
          {
          /* encode walltime remaining, this is custom because walltime 
           * remaining isn't an attribute */
          int       len;
          char      buf[MAXPATHLEN];
          char     *pname;
          svrattrl *pal;
          resource *pres;

          int found = 0;
          unsigned long remaining;

          if (((pattr + JOB_ATR_resource)->at_val.at_list.ll_next != NULL) &&
              ((pattr + JOB_ATR_resource)->at_flags & ATR_VFLAG_SET))
            {
            pres = (resource *)GET_NEXT((pattr + JOB_ATR_resource)->at_val.at_list);
            
            /* find the walltime resource */
            for (;pres != NULL;pres = (resource *)GET_NEXT(pres->rs_link))
              {
              pname = pres->rs_defin->rs_name;
              
              if (strcmp(pname, "walltime") == 0)
                {
                /* found walltime */
                unsigned long value = (unsigned long)pres->rs_value.at_val.at_long;
                if (((pattr+index)->at_flags & ATR_VFLAG_SET) != FALSE)
                  {
                  remaining = value - (time_now - (pattr + index)->at_val.at_long);
                  }
                else
                  {
                  remaining = value;
                  }
                found = TRUE;
                break;
                }
              }
            }
         
          if (found == TRUE)
            {
            snprintf(buf,sizeof(buf),"%ld",remaining);

            len = strlen(buf+1);
            pal = attrlist_create("Walltime","Remaining",len);

            if (pal != NULL)
              {
              memcpy(pal->al_value,buf,len);
              pal->al_flags = ATR_VFLAG_SET;
              append_link(phead,&pal->al_link,pal);
              }
            }
          } /* END if (index == JOB_ATR_start_time) */
        }
      }
    }    /* END for (index) */

  /* SUCCESS */

  return(0);
  }  /* END status_attrib() */

/* END stat_job.c */

