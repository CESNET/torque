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
 * queue_recov.c - This file contains the functions to record a queue
 *	data struture to disk and to recover it from disk.
 *
 *	The data is recorded in a file whose name is the queue name
 *
 *	The following public functions are provided:
 *		que_save()  - save the disk image 
 *		que_recov()  - recover (read) queue from disk
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include "pbs_ifl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "queue.h"
#include "svrfunc.h"
#include "log.h"


/* data global to this file */

extern char *path_queues;
extern time_t        time_now;

/*
 * que_save() - Saves a queue structure image on disk
 *
 *
 *	For a save, to insure no data is ever lost due to system crash:
 *	1. write (with O_SYNC) new image to a new file using a temp name
 *	2. unlink the old (image) file
 *	3. link the correct name to the new file
 *	4. unlink the temp name
 *	
 *	Then, if the queue has any access control lists, they are saved
 *	to their own files.
 */

int que_save(

  pbs_queue *pque)	/* pointer to queue structure */

  {
  int	fds;
  int	i;
  char   *myid = "que_save";
  char	namebuf1[MAXPATHLEN];
  char	namebuf2[MAXPATHLEN];

  pque->qu_attr[QA_ATR_MTime].at_val.at_long = time_now;
  pque->qu_attr[QA_ATR_MTime].at_flags = ATR_VFLAG_SET;

  strcpy(namebuf1, path_queues);
  strcat(namebuf1, pque->qu_qs.qu_name);
  strcpy(namebuf2, namebuf1);
  strcat(namebuf2, ".new");

  fds = open(namebuf2,O_CREAT|O_WRONLY|O_Sync,0600);

  if (fds < 0) 
    {
    log_err(errno,myid,"open error");

    return(-1);
    }

  /* set up save buffering system */

  save_setup(fds);

  /* save basic queue structure (fixed length stuff) */

  if (save_struct((char *)&pque->qu_qs,sizeof(struct queuefix)) != 0) 
    {
    log_err(-1,myid,"save_struct failed");

    close(fds);

    return(-1);
    }

  /* save queue attributes  */

  if (save_attr(que_attr_def,pque->qu_attr,(int)QA_ATR_LAST) != 0) 
    {
    log_err(-1,myid,"save_attr failed");

    close(fds);

    return(-1);
    }

	if (save_flush() != 0) {	/* flush the save buffer */
		log_err(-1, myid,"save_flush failed");
		(void)close(fds);
		return (-1);
	}

	(void)close(fds);

	(void)unlink(namebuf1);	
	if (link(namebuf2, namebuf1) < 0) {
		log_err(errno, myid, "unable to link queue name");
	} else {
		(void)unlink(namebuf2);
	}
	
  /* 
   * now search queue's attributes for access control lists,
   * they are saved separately in their own file:
   * ../priv/(attr name)/(queue name)
   */

  for (i = 0;i < (int)QA_ATR_LAST;i++) 
    {
    if (pque->qu_attr[i].at_type == ATR_TYPE_ACL) 
      {
      save_acl(
        &pque->qu_attr[i], 
        &que_attr_def[i],
        que_attr_def[i].at_name, 
        pque->qu_qs.qu_name);
      }
    }
	
  return(0);
  }




/*
 * que_recov() - load (recover) a queue from its save file
 *
 *	This function is only needed upon server start up.
 *
 *	The queue structure is recovered from the disk.
 *	Space to hold the above is malloc-ed as needed.
 *
 *	Returns: pointer to new queue structure if successful
 *		 null if error
 */

pbs_queue *que_recov(

  char *filename)	/* pathname to queue save file */

  {
  int	   fds;
  int	   i;
  pbs_queue *pq;
  char	   namebuf[MAXPATHLEN];

  pq = que_alloc(filename);  /* allocate & init queue structure space */

  if (pq == NULL) 
    {
    log_err(-1,"que_recov","que_alloc failed");

    return(NULL);
    }

  strcpy(namebuf, path_queues);
  strcat(namebuf, filename);

  fds = open(namebuf,O_RDONLY,0);

  if (fds < 0) 
    {
    log_err(errno,"que_recov","open error");

    que_free(pq);

    return(NULL);
    }

	/* read in queue save sub-structure */

	if (read(fds, (char *)&pq->qu_qs, sizeof(struct queuefix)) !=
	    sizeof(struct queuefix)) {
		log_err(errno, "que_recov", "read error");
		que_free(pq);
		(void)close(fds);
		return ((pbs_queue *)0);
	}

	/* read in queue attributes */

	if (recov_attr(fds, pq, que_attr_def, pq->qu_attr,
	    (int)QA_ATR_LAST, 0) != 0) {
		log_err(-1, "que_recov", "recov_attr[common] failed");
		que_free(pq);
		(void)close(fds);
		return ((pbs_queue *)0);
	}

  /*
   * now reload the access control lists, these attributes were
   * saved separately
   */

  for (i = 0;i < (int)QA_ATR_LAST;i++)  
    {
    if (pq->qu_attr[i].at_type == ATR_TYPE_ACL) 
      {
      recov_acl(
        &pq->qu_attr[i], 
        &que_attr_def[i],
        que_attr_def[i].at_name, 
        pq->qu_qs.qu_name);
      }
    }
	
  /* all done recovering the queue */

  close(fds);

  if ((pq->qu_attr[QA_ATR_MTime].at_flags & ATR_VFLAG_SET) == 0)
    {
    /* if we are recovering a pre-2.1.2 queue, save a new mtime */

    pq->qu_attr[QA_ATR_MTime].at_val.at_long = time_now;
    pq->qu_attr[QA_ATR_MTime].at_flags = ATR_VFLAG_SET;

    que_save(pq);
    }

  return(pq);
  }
