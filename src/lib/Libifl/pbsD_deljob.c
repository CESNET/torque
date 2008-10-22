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

/*	pbsD_deljob.c

	Send the Delete Job request to the server
	really just an instance of the manager request
*/

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include "libpbs.h"

  /* NOTE:  routes over to X on server side

    qdel                      pbs_server
    ----------------------    ------------------------
    pbsD_deljob()                (lib/Libifl/pbsD_deljob.c)
      PBSD_manager()             (lib/Libifl/PBSD_manager.c)
        PBSD_mgr_put()           (lib/Libifl/PBSD_manage2.c)
          DIS_tcp_wflush() -->
        PBSD_rdrpy()


                                          pbs_mom
                                          --------------------------
                                          main()                     (resmom/mom_main.c)
                                            finish_loop()            (resmom/mom_main.c)
                                              wait_request()         (lib/Libnet/net_server.c)
                                                accept_conn()
                                                obit_reply()         (resmom/catch_child.c)
                                                process_request()    (server/process_request.c)
                                                  dispatch_request() (server/process_request.c)
                                                    req_cpyfile
                                                    req_deletejob()  (resmom/requests.c)
                                                      mom_deljob()   (resmom/catch_child.c)
                                                        job_purge()  (server/job_func.c)
                                                      reply_ack()
 
                              wait_request()                       (net_server.c)
                                process_request()                  (server/process_request.c)
                                  dis_request_read()               (server/dis_read.c)
                                    decode_DIS_RunJob()            (lib/Libifl/dec_RunJob.c)
                                dispatch_request()                 (server/process_request.c)
                                  req_deletejob()                  (server/req_delete.c)
                                    set_task()                     (server/svr_task.c)
                                dispatch_task()                    (server/svr_task.c)
                                      reply_text()                 (server/reply_send.c)
                                        reply_send()               (server/reply_send.c)
   *   PBSD_rdrpy      <-----
   */

  /* NOTE:  set_task sets WORK_Deferred_Child : request remains until child terminates */

/* NOTE:  track pbs_server to pbs_mom interactions */



int pbs_deljob(

  int   c,
  char *jobid,
  char *extend)

  {
  int             rc;

  struct attropl *aoplp = NULL;
	
  if ((extend == NULL) ||
        strncmp(extend,PURGECOMP,strlen(PURGECOMP)))
    {
    if ((c < 0) || (jobid == NULL) || (*jobid == '\0'))
      {
      pbs_errno = PBSE_IVALREQ;

      return(pbs_errno);
      }
    }

  rc = PBSD_manager(
    c,
    PBS_BATCH_DeleteJob,
    MGR_CMD_DELETE,
    MGR_OBJ_JOB,
    jobid,
    aoplp,
    extend);

  return(rc);
  }  /* END pbs_deljob() */

