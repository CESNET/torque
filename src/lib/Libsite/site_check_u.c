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
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include "portability.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "job.h"
#include "log.h"
#include "batch_request.h"
#include "pbs_nodes.h"

#ifndef TRUE
#define TRUE 1
#endif

/* Global Data Items */

extern char *pbs_o_host;
extern char  server_host[];
extern char *msg_orighost;	/* error message: no PBS_O_HOST */

/*
 * site_check_u - site_check_user_map()
 *
 *	This routine determines if a user is privileged to execute a job
 *	on this host under the login name specified (in user-list attribute)
 *
 *	As provided, this routine uses ruserok(3N).  If this is a problem,
 *	It's replacement is "left as an exercise for the reader."
 *
 *      Return -1 for access denied, otherwise 0 for ok.
 */

int site_check_user_map(

  job  *pjob,	
  char *luser)

  {
  char *orighost;
  char  owner[PBS_MAXUSER + 1];
  char *p1;
  char *p2;
  int   rc;
 
  char  *dptr;

  /* get just the owner name, without the "@host" */
	
  p1 = pjob->ji_wattr[JOB_ATR_job_owner].at_val.at_str;
  p2 = owner;

  while ((*p1 != '@') && (*p1 != '\0'))
    *p2++ = *p1++;

  *p2 = '\0';

  orighost = get_variable(pjob,pbs_o_host);

  if (orighost == NULL) 
    {
    /* access denied */

    log_event(
      PBSEVENT_JOB, 
      PBS_EVENTCLASS_JOB, 
      pjob->ji_qs.ji_jobid, 
      msg_orighost);

    return(-1);
    }

  if (!strcmp(orighost,server_host) && !strcmp(owner,luser))
    {
    /* submitting from server host, access allowed */

    return(0);
    }

  /* make short host name */

  if ((dptr = strchr(orighost,'.')) != NULL)
    {
    *dptr = '\0';
    }

  if ((server.sv_attr[(int)SRV_ATR_AllowNodeSubmit].at_flags & ATR_VFLAG_SET) && \
      (server.sv_attr[(int)SRV_ATR_AllowNodeSubmit].at_val.at_long == 1) && \
      (find_nodebyname(orighost) != NULL))
    {
    /* job submitted from compute host, access allowed */

    if (dptr != NULL)
       *dptr = '.';

    return(0);
    }

  if (server.sv_attr[(int)SRV_ATR_SubmitHosts].at_flags & ATR_VFLAG_SET)
    {
    struct array_strings *submithosts = NULL;
    char                 *testhost;
    int                   hostnum = 0;

    submithosts = server.sv_attr[(int)SRV_ATR_SubmitHosts].at_val.at_arst;

    for (hostnum = 0;hostnum < submithosts->as_usedptr;hostnum++)
      {
      testhost = submithosts->as_string[hostnum];

      if (!strcasecmp(testhost,orighost))
        {
        /* job submitted from host found in trusted submit host list, access allowed */

        if (dptr != NULL)
          *dptr = '.';

        return(0);
        }
    } /* END for (hostnum) */
  } /* END if (SRV_ATR_SubmitHosts) */

  if (dptr != NULL)
    *dptr = '.';

#ifdef GSSAPI
  return 0;
#endif

  rc = ruserok(orighost,0,owner,luser);

#ifdef sun
  /* broken Sun ruserok() sets process so it appears to be owned	*/
  /* by the luser, change it back for cosmetic reasons		*/

  setuid(0);
#endif	/* sun */

  return(rc);
  }  /* END site_check_user_map() */




/*
 * site_check_u - site_acl_check()
 *
 *    This routine is a place holder for sites that wish to implement
 *    access controls that differ from the standard PBS user, group, host
 *    access controls.  It does NOT replace their functionality.
 *
 *    Return -1 for access denied, otherwise 0 for ok.
 */

int site_acl_check(pjob, pque)
	job		*pjob;
	pbs_queue	*pque;
{
	return (0);
}
