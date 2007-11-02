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
 * queue_func.c - various functions dealing with queues
 *
 * Included functions are:
 *	que_alloc()	- allocacte and initialize space for queue structure
 *	que_free()	- free queue structure
 *	que_purge()	- remove queue from server
 *	find_queuebyname() - find a queue with a given name
 *	get_dfltque()	- get default queue
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include <memory.h>
#include <stdlib.h>
#include "pbs_ifl.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "list_link.h"
#include "log.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "pbs_error.h"
#if __STDC__ != 1
#include <memory.h>
#endif

/* Global Data */

extern char     *msg_err_unlink;
extern char	*path_queues;
extern struct    server server;
extern tlist_head svr_queues;

/*
 * que_alloc - allocate space for a queue structure and initialize 
 *	attributes to "unset"
 *
 *	Returns: pointer to structure or null is space not available.
 */

pbs_queue *que_alloc(

  char *name)

  {
  int        i;
  pbs_queue *pq;

  pq = (pbs_queue *)malloc(sizeof(pbs_queue));

  if (pq == NULL) 
    {
    log_err(errno,"que_alloc","no memory");

    return(NULL);
    }

  memset((char *)pq,(int)0,(size_t)sizeof(pbs_queue));

  pq->qu_qs.qu_type = QTYPE_Unset;

  CLEAR_HEAD(pq->qu_jobs);
  CLEAR_LINK(pq->qu_link);

  strncpy(pq->qu_qs.qu_name,name,PBS_MAXQUEUENAME);

  append_link(&svr_queues,&pq->qu_link,pq);

  server.sv_qs.sv_numque++;

  /* set the working attributes to "unspecified" */

  for (i = 0;i < (int)QA_ATR_LAST;i++) 
    {
    clear_attr(&pq->qu_attr[i],&que_attr_def[i]);
    }

  return(pq);
  }  /* END que_alloc() */




/*
 * que_free - free queue structure and its various sub-structures
 */

void que_free(

  pbs_queue *pq)

  {
  int		 i;
  attribute	*pattr;
  attribute_def	*pdef;
	
  /* remove any malloc working attribute space */

  for (i = 0;i < (int)QA_ATR_LAST;i++) 
    {
    pdef  = &que_attr_def[i];
    pattr = &pq->qu_attr[i];

    pdef->at_free(pattr);

    /* remove any acl lists associated with the queue */

    if (pdef->at_type == ATR_TYPE_ACL)  
      {
      pattr->at_flags |= ATR_VFLAG_MODIFY;

      save_acl(pattr,pdef,pdef->at_name,pq->qu_qs.qu_name);
      }
    }

  /* now free the main structure */

  server.sv_qs.sv_numque--;
  delete_link(&pq->qu_link);

  free((char *)pq);

  return;
  }  /* END que_free() */




/*
 * que_purge - purge queue from system
 *
 * The queue is dequeued, the queue file is unlinked.
 * If the queue contains any jobs, the purge is not allowed.
 */

int que_purge(

  pbs_queue *pque)

  {
  char     namebuf[MAXPATHLEN];

  if (pque->qu_numjobs != 0)
    {
    return(PBSE_QUEBUSY);
    }
	
  strcpy(namebuf,path_queues);	/* delete queue file */

  strcat(namebuf,pque->qu_qs.qu_name);

  if (unlink(namebuf) < 0) 
    {
    sprintf(log_buffer,msg_err_unlink,"Queue",namebuf);

    log_err(errno,"queue_purge",log_buffer);
    }

  que_free(pque);

  return(0);
  }





/*
 * find_queuebyname() - find a queue by its name
 */

pbs_queue *find_queuebyname(

  char *quename) /* I */

  {
  char  *pc;
  pbs_queue *pque;
  char   qname[PBS_MAXDEST + 1];

  strncpy(qname,quename,PBS_MAXDEST);

  qname[PBS_MAXDEST] = '\0';

  pc = strchr(qname,(int)'@');	/* strip off server (fragment) */
  
  if (pc != NULL)
    *pc = '\0';

  pque = (pbs_queue *)GET_NEXT(svr_queues);

  while (pque != NULL) 
    {
    if (!strcmp(qname,pque->qu_qs.qu_name))
      break;

    pque = (pbs_queue *)GET_NEXT(pque->qu_link);
    }

  if (pc != NULL)
    *pc = '@';	/* restore '@' server portion */

  return(pque);
  }  /* END find_queuebyname() */





/*
 * get_dfltque - get the default queue (if declared)
 */

pbs_queue *get_dfltque(void)

  {
  pbs_queue *pq = NULL;

  if (server.sv_attr[SRV_ATR_dflt_que].at_flags & ATR_VFLAG_SET)
    {
    pq = find_queuebyname(server.sv_attr[SRV_ATR_dflt_que].at_val.at_str);
    }

  return(pq);
  }  /* END get_dfltque() */

/* END queue_func.c */

