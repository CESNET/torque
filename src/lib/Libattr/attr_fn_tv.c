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

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"
#include "pbs_job.h"

int timeval_subtract( struct timeval *result, struct timeval *x, struct timeval *y);

/*
 * This file contains functions for manipulating attributes of type
 * struct timeval
 *
 * Each set has functions for:
 * Decoding the value string to the machine representation.
 * Encoding the internal attribute to external form
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * --------------------------------------------------
 * The Set of Attribute Functions for attributes with
 * value type "struct timeval"
 * --------------------------------------------------
 */

#define CVNBUFSZ 256

/*
 * decode_tv - decode struct timeval into attribute structure 
 *  
 * Returns: 0 if ok 
 *  		>0 error number
 *  		*patr elements set
 */

int decode_tv(
   struct attribute *patr,
   char *name,  /* attribute name */
   char *rescn, /* resource name*/
   char *val	/* attribute value */
   )
  {
  char 		*pc;
  time_t 		secs = 0;
  suseconds_t	usec = 0;
  char 		*workval;
  char			*workvalsv;
  int			ncolon = 0;


  if ((val == NULL) || (strlen(val) == 0))
    {
	patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) | ATR_VFLAG_MODIFY;

	patr->at_val.at_long = 0;

	/* SUCCESS */

	return(0);
	}

  workval = strdup(val);
  workvalsv = workval; /* We keep track of the beginning of this value so we can free it later */

  for(pc = workval; *pc; ++pc)
		{
		if(*pc == ':')
			{
			if(++ncolon > 1)
				goto badval;
			
			/* At this point we should have the seconds
			 	 portion of the timeval structure */
			*pc = 0;
	
			secs = atoi(workval);
	
			workval = pc + 1;
			}
		else
			{
			if(!isdigit((int)*pc))
				 goto badval;
			}
		}

	/* workval should be pointing to the usec portion
	 	 of the timeval structure */
	usec = atoi(workval);

	patr->at_val.at_timeval.tv_sec = secs;
	patr->at_val.at_timeval.tv_usec = usec;
  patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
   
	free(workvalsv);
	
	/* SUCCESS */   

	return(0);

badval:
  free(workvalsv);

  return(PBSE_BADATVAL);
  } /* END decode_tv */

 /*
  * encode_tv - encode attribute of type struct timeval into attr_extern 
  * with the value in form of [seconds:microseconds] 
  *  
  * Returns: >0 if ok
  *   =0 if no value, no attrlist link added
  *   <0 if error
  */

int encode_tv(
  attribute *attr,   /* ptr to attribute (value in attr->at_val.at_long) */
  tlist_head *phead,  /* head of attrlist list (optional) */
  char      *atname, /* attribute name */
  char      *rsname, /* resource name (optional) */
  int 		mode)	/* endcode mode (not used) */

  {
  size_t 		 ct;
  char 	 		 cvnbuf[CVNBUFSZ];
  time_t	 	 secs;
  suseconds_t	 usecs;
  struct timeval *tv;
  svrattrl 		 *pal;


  if ( attr == NULL )
		{
		/* FAILURE */
		return(-1);
		}

  if ( !(attr->at_flags & ATR_VFLAG_SET))
		{
		return(0);
		}

  tv = &attr->at_val.at_timeval;
  secs = tv->tv_sec;
  usecs = tv->tv_usec;

  sprintf(cvnbuf, "%ld.%06ld", secs, usecs);

  ct = strlen(cvnbuf) + 1;

  if(phead != NULL)
		{
		pal = attrlist_create(atname, rsname, ct);
	
		if(pal == NULL)
		  {
			/* FAILURE */
			return(-1);
		  }
	
		memcpy(pal->al_value, cvnbuf, ct);
	
		pal->al_flags = attr->at_flags;
	
		append_link(phead, &pal->al_link, pal);
		}
  else
	  {
	  strcpy(atname, cvnbuf);
	  }

  return(1);
  }

/*
 * set_tv - set attribute A to attribute B,
 * either A=B, A += B, or A -= B
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int
set_tv(
	struct attribute *attr, 
	struct attribute *new, 
	enum batch_op op)
  {
  assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

  switch (op)
    {

		case SET:
			attr->at_val.at_timeval.tv_sec = new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec = new->at_val.at_timeval.tv_usec;
      break;

    case INCR:
			attr->at_val.at_timeval.tv_sec += new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec += new->at_val.at_timeval.tv_usec;
      break;

    case DECR:
			attr->at_val.at_timeval.tv_sec -= new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec -= new->at_val.at_timeval.tv_usec;
      break;

    default:
      return (PBSE_INTERNAL);
    }

  attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return (0);
  }

/*
 * comp_tv - compare two attributes of type struct timeval
 *
 * Returns: +1 if 1st > 2nd
 *    0 if 1st == 2nd
 *   -1 if 1st < 2nd
 */

int comp_tv(struct attribute *attr, struct attribute *with)
  {

  if (!attr || !with)
  	return (-1);
    
  if(attr->at_val.at_timeval.tv_sec < with->at_val.at_timeval.tv_sec)
  	{
  	return(-1);
  	}
  else if (attr->at_val.at_timeval.tv_sec > with->at_val.at_timeval.tv_sec)
  	{
  	return(1);
  	}
  else
    {
  	if(attr->at_val.at_timeval.tv_usec < with->at_val.at_timeval.tv_usec)
  	  {
  		return(-1);
  	  }
  	else if(attr->at_val.at_timeval.tv_usec < with->at_val.at_timeval.tv_usec)
  		{
  		return(1);
  		}
  	}
    	
  return(0);
  }

int job_radix_action (
  attribute *new,
  void      *pobj,
  int       actmode
  )
  {
    job *pjob;
    int rc = PBSE_NONE;

    pjob = (job *)pobj;

    switch (actmode)
      {

      case ATR_ACTION_NEW:
		pjob->ji_radix = new->at_val.at_long;
        break;

	  case ATR_ACTION_ALTER:
		pjob->ji_radix = new->at_val.at_long;
        break;

      default:

        rc = PBSE_INTERNAL;
      }

    return rc;

  }

/* timeval_subtract - subtract timeval y from timeval x and
 * store the difference in result.
 *
 * Returns 0 if the difference is >= 0
 * Returns 1 if the difference is < 0
 *
 */

int timeval_subtract(
  struct timeval *result, 
  struct timeval *x, 
  struct timeval *y)
  {
  suseconds_t nsec;

  /* perform the carry for the later subtraction by updating y */
  if(x->tv_usec < y->tv_usec)
	{
	nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
	}

  if(x->tv_usec - y->tv_usec > 1000000)
	{
	nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
	}

  /* compute the difference */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* return 1 is negative */
  return(x->tv_sec < y->tv_sec);
  }

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

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"
#include "pbs_job.h"

int timeval_subtract( struct timeval *result, struct timeval *x, struct timeval *y);

/*
 * This file contains functions for manipulating attributes of type
 * struct timeval
 *
 * Each set has functions for:
 * Decoding the value string to the machine representation.
 * Encoding the internal attribute to external form
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * --------------------------------------------------
 * The Set of Attribute Functions for attributes with
 * value type "struct timeval"
 * --------------------------------------------------
 */

#define CVNBUFSZ 256

/*
 * decode_tv - decode struct timeval into attribute structure 
 *  
 * Returns: 0 if ok 
 *  		>0 error number
 *  		*patr elements set
 */

int decode_tv(
   struct attribute *patr,
   char *name,  /* attribute name */
   char *rescn, /* resource name*/
   char *val	/* attribute value */
   )
  {
  char 		*pc;
  time_t 		secs = 0;
  suseconds_t	usec = 0;
  char 		*workval;
  char			*workvalsv;
  int			ncolon = 0;


  if ((val == NULL) || (strlen(val) == 0))
    {
	patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) | ATR_VFLAG_MODIFY;

	patr->at_val.at_long = 0;

	/* SUCCESS */

	return(0);
	}

  workval = strdup(val);
  workvalsv = workval; /* We keep track of the beginning of this value so we can free it later */

  for(pc = workval; *pc; ++pc)
		{
		if(*pc == ':')
			{
			if(++ncolon > 1)
				goto badval;
			
			/* At this point we should have the seconds
			 	 portion of the timeval structure */
			*pc = 0;
	
			secs = atoi(workval);
	
			workval = pc + 1;
			}
		else
			{
			if(!isdigit((int)*pc))
				 goto badval;
			}
		}

	/* workval should be pointing to the usec portion
	 	 of the timeval structure */
	usec = atoi(workval);

	patr->at_val.at_timeval.tv_sec = secs;
	patr->at_val.at_timeval.tv_usec = usec;
  patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
   
	free(workvalsv);
	
	/* SUCCESS */   

	return(0);

badval:
  free(workvalsv);

  return(PBSE_BADATVAL);
  } /* END decode_tv */

 /*
  * encode_tv - encode attribute of type struct timeval into attr_extern 
  * with the value in form of [seconds:microseconds] 
  *  
  * Returns: >0 if ok
  *   =0 if no value, no attrlist link added
  *   <0 if error
  */

int encode_tv(
  attribute *attr,   /* ptr to attribute (value in attr->at_val.at_long) */
  tlist_head *phead,  /* head of attrlist list (optional) */
  char      *atname, /* attribute name */
  char      *rsname, /* resource name (optional) */
  int 		mode)	/* endcode mode (not used) */

  {
  size_t 		 ct;
  char 	 		 cvnbuf[CVNBUFSZ];
  time_t	 	 secs;
  suseconds_t	 usecs;
  struct timeval *tv;
  svrattrl 		 *pal;


  if ( attr == NULL )
		{
		/* FAILURE */
		return(-1);
		}

  if ( !(attr->at_flags & ATR_VFLAG_SET))
		{
		return(0);
		}

  tv = &attr->at_val.at_timeval;
  secs = tv->tv_sec;
  usecs = tv->tv_usec;

  sprintf(cvnbuf, "%ld.%06ld", secs, usecs);

  ct = strlen(cvnbuf) + 1;

  if(phead != NULL)
		{
		pal = attrlist_create(atname, rsname, ct);
	
		if(pal == NULL)
		  {
			/* FAILURE */
			return(-1);
		  }
	
		memcpy(pal->al_value, cvnbuf, ct);
	
		pal->al_flags = attr->at_flags;
	
		append_link(phead, &pal->al_link, pal);
		}
  else
	  {
	  strcpy(atname, cvnbuf);
	  }

  return(1);
  }

/*
 * set_tv - set attribute A to attribute B,
 * either A=B, A += B, or A -= B
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int
set_tv(
	struct attribute *attr, 
	struct attribute *new, 
	enum batch_op op)
  {
  assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

  switch (op)
    {

		case SET:
			attr->at_val.at_timeval.tv_sec = new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec = new->at_val.at_timeval.tv_usec;
      break;

    case INCR:
			attr->at_val.at_timeval.tv_sec += new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec += new->at_val.at_timeval.tv_usec;
      break;

    case DECR:
			attr->at_val.at_timeval.tv_sec -= new->at_val.at_timeval.tv_sec;
			attr->at_val.at_timeval.tv_usec -= new->at_val.at_timeval.tv_usec;
      break;

    default:
      return (PBSE_INTERNAL);
    }

  attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return (0);
  }

/*
 * comp_tv - compare two attributes of type struct timeval
 *
 * Returns: +1 if 1st > 2nd
 *    0 if 1st == 2nd
 *   -1 if 1st < 2nd
 */

int comp_tv(struct attribute *attr, struct attribute *with)
  {

  if (!attr || !with)
  	return (-1);
    
  if(attr->at_val.at_timeval.tv_sec < with->at_val.at_timeval.tv_sec)
  	{
  	return(-1);
  	}
  else if (attr->at_val.at_timeval.tv_sec > with->at_val.at_timeval.tv_sec)
  	{
  	return(1);
  	}
  else
    {
  	if(attr->at_val.at_timeval.tv_usec < with->at_val.at_timeval.tv_usec)
  	  {
  		return(-1);
  	  }
  	else if(attr->at_val.at_timeval.tv_usec < with->at_val.at_timeval.tv_usec)
  		{
  		return(1);
  		}
  	}
    	
  return(0);
  }

int job_radix_action (
  attribute *new,
  void      *pobj,
  int       actmode
  )
  {
    job *pjob;
    int rc = PBSE_NONE;

    pjob = (job *)pobj;

    switch (actmode)
      {

      case ATR_ACTION_NEW:
		pjob->ji_radix = new->at_val.at_long;
        break;

	  case ATR_ACTION_ALTER:
		pjob->ji_radix = new->at_val.at_long;
        break;

      default:

        rc = PBSE_INTERNAL;
      }

    return rc;

  }

/* timeval_subtract - subtract timeval y from timeval x and
 * store the difference in result.
 *
 * Returns 0 if the difference is >= 0
 * Returns 1 if the difference is < 0
 *
 */

int timeval_subtract(
  struct timeval *result, 
  struct timeval *x, 
  struct timeval *y)
  {
  suseconds_t nsec;

  /* perform the carry for the later subtraction by updating y */
  if(x->tv_usec < y->tv_usec)
	{
	nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
	}

  if(x->tv_usec - y->tv_usec > 1000000)
	{
	nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
	}

  /* compute the difference */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* return 1 is negative */
  return(x->tv_sec < y->tv_sec);
  }

