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
#include <sys/types.h>
#include "portability.h"
#include <errno.h>
#include <netinet/in.h>
#include "log.h"
#include "pbs_ifl.h"
#include "net_connect.h"
#include "svrfunc.h"
#include "sched_cmds.h"
#include "pbs_error.h"

/* Global Data */

extern pbs_net_t pbs_scheduler_addr;
extern unsigned int pbs_scheduler_port;
extern char      server_name[];
extern struct connection svr_conn[];
extern int	 svr_do_schedule;
extern char     *msg_sched_called;
extern char     *msg_sched_nocall;

int scheduler_sock = -1;
int scheduler_jobct = 0;


/* Functions private to this file */

static int  put_4byte A_((int sock, unsigned int val));
static void scheduler_close A_((int));


extern void bad_node_warning(pbs_net_t);
extern ssize_t write_nonblocking_socket(int,const void *,ssize_t);


                

/* sync w/sched_cmds.h */

const char *PSchedCmdType[] = {
  "NULL",
  "new",
  "term",
  "time",
  "recyc",
  "cmd",
  "N/A",
  "configure",
  "quit",
  "ruleset",
  "scheduler_first",
  NULL };





/*
 * contact_sched - open connection to the scheduler and send it a command
 */

static int contact_sched(

  int cmd)

  {
  int sock;

  char  tmpLine[1024];

  char *id = "contact_sched";

	/* connect to the Scheduler */
#if 0   /* don't check if scheduler runs on same node as server */
        if (!addr_ok(pbs_scheduler_addr)) {
	    pbs_errno = EHOSTDOWN;
            return -1;
        }
#endif

  sock = client_to_svr(pbs_scheduler_addr,pbs_scheduler_port,1);

  if (sock < 0) 
    {
    bad_node_warning(pbs_scheduler_addr);

    sprintf(tmpLine,"%s - port %d",
      msg_sched_nocall,
      pbs_scheduler_port);
    
    log_err(errno,id,tmpLine);

    return(-1);
    }

  add_conn(
    sock, 
    FromClientDIS, 
    pbs_scheduler_addr, 
    pbs_scheduler_port, 
    process_request);

  svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;

  net_add_close_func(sock,scheduler_close);

  /* send command to Scheduler */

  if (put_4byte(sock,cmd) < 0) 
    {
    sprintf(tmpLine,"%s - port %d",
      msg_sched_nocall,
      pbs_scheduler_port);

    log_err(errno,id,tmpLine);

    close_conn(sock);

    return(-1);
    }

  sprintf(log_buffer,msg_sched_called,
    (cmd != SCH_ERROR) ? PSchedCmdType[cmd] : "ERROR");

  log_event(
    PBSEVENT_SCHED, 
    PBS_EVENTCLASS_SERVER,
    server_name,
    log_buffer);

  return (sock);
  }  /* END contact_sched() */





/*
 * schedule_jobs - contact scheduler and direct it to run a scheduling cycle
 *	If a request is already outstanding, skip this one.
 *
 *	Returns: -1 = error
 *		  0 = scheduler notified
 *		 +1 = scheduler busy
 */

int schedule_jobs()

  {
  int cmd;

  static int first_time = 1;

  if (first_time)
    cmd = SCH_SCHEDULE_FIRST;
  else
    cmd = svr_do_schedule;

  svr_do_schedule = SCH_SCHEDULE_NULL;

  if (scheduler_sock == -1) 
    {
    scheduler_jobct = 0;

    if ((scheduler_sock = contact_sched(cmd)) < 0)
      {   
      return(-1);
      }

    first_time = 0;

    return(0);
    } 

  return(1);	
  }  /* END schedule_jobs() */




/*
 * scheduler_close - connection to scheduler has closed, clear scheduler_called
 */

static void scheduler_close(

  int sock)

  {
  scheduler_sock = -1;

/*
 *	This bit of code is intended to support the scheduler - server - mom
 *	sequence.  A scheduler script may bes written to run only one job per
 *	cycle to  ensure its newly taken resources are considered by the
 *	scheduler before selecting another job.  In that case, rather than
 *	wait a full cycle before scheduling the next job, we check for
 *	one (and only one) job was run by the scheduler.  If true, then
 *	recycle the scheduler.
 */

	if (scheduler_jobct == 1) {
		/* recycle the scheduler */
		svr_do_schedule = SCH_SCHEDULE_RECYC;
	}
}

/*
 * put_4byte() - write a 4 byte integer in network order
 *
 *	Returns:  0 for sucess, -1 if error.
 */

static int put_4byte(sock, val)
	int		sock;	/* socket to read from */
	unsigned int	val;	/* 4 byte interger to write */
{
	
	int amt;
	union {
		int unl;
		char unc[sizeof (unsigned int)];
	} un;

	un.unl = htonl(val);
	amt = write(sock, (char *)(un.unc+sizeof(unsigned int)-4), 4);
	if (amt == 4)
		return (0);
	else 
		return (-1);
}
