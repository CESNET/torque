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

#include "portability.h"
#include "list_link.h"
#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#endif


/*
 * tlist_link.c - general routines for maintenance of a double
 *	linked list.  A user defined structure can be managed as
 *	a double linked list if the first element in the user structure
 *	is the "tlist_link" struct defined in list_link.h and the list
 *	is headed by a "tlist_link" struct also defined in list_link.h.
 *
 *	There are the routines provided:
 *		insert_link - inserts a new entry before or after an old
 *		append_link - adds a new entry to the end of the list
 *		delete_link - removes an entry from the list
 *		is_linked   - returns 1 if entry is in the list
 */




/*
 * insert_link - adds a new entry to a list.
 *	Entry is added either before (position=0) or after (position !=0)
 *	an old entry.
 */

void insert_link(

  struct tlist_link *old,	/* ptr to old entry in list */
  struct tlist_link *new,	/* ptr to new link entry    */
  void             *pobj,	/* ptr to object to link in */
  int               position)	/* 0=before old, else after */

  {
#ifndef NDEBUG
  /* first make sure unlinked entries are pointing to themselves	    */

  if ((pobj == (void *)0) ||
      (old == (struct tlist_link *)0) ||
      (old->ll_prior == (tlist_link *)0) ||
      (old->ll_next  == (tlist_link *)0) ||
      (new->ll_prior != (tlist_link *)new) ||
      (new->ll_next  != (tlist_link *)new))  
    {
    fprintf(stderr, "Assertion failed, bad pointer in insert_link\n");

    abort();
    }
#endif  /* END NDEBUG */

  if (position == LINK_INSET_AFTER) 
    {  /* insert new after old */
    new->ll_prior = old;
    new->ll_next  = old->ll_next;

    (old->ll_next)->ll_prior = new;
    old->ll_next = new;
    } 
  else 
    {				/* insert new before old */
    new->ll_next = old;
    new->ll_prior = old->ll_prior;
    (old->ll_prior)->ll_next = new;
    old->ll_prior = new;
    }

  /*
   * its big trouble if ll_struct is null, it would make this
   * entry appear to be the head, so we never let that happen
   */

  if (pobj != NULL)
    new->ll_struct = pobj;
  else
    new->ll_struct = (void *)new;

  return;
  }




/*
 * append_link - append a new entry to the end of the list
 */

void append_link(

  tlist_head *head, /* ptr to head of list */
  tlist_link *new,  /* ptr to new entry */
  void      *pobj) /* ptr to object to link in */

  {
#ifndef NDEBUG
  /* first make sure unlinked entries are pointing to themselves	    */

  if ((pobj == NULL) ||
      (head->ll_prior == NULL) ||
      (head->ll_next  == NULL) ||
      (new->ll_prior  != (tlist_link *)new) ||
      (new->ll_next   != (tlist_link *)new)) 
    {
    if (pobj == NULL)
      fprintf(stderr,"ERROR:  bad pobj pointer in append_link\n");

    if (head->ll_prior == NULL)
      fprintf(stderr,"ERROR:  bad head->ll_prior pointer in append_link\n");

    if (head->ll_next == NULL)
      fprintf(stderr,"ERROR:  bad head->ll_next pointer in append_link\n");

    if (new->ll_prior == NULL)
      fprintf(stderr,"ERROR:  bad new->ll_prior pointer in append_link\n");

    if (new->ll_next == NULL)
      fprintf(stderr,"ERROR:  bad new->ll_next pointer in append_link\n");

    abort();
    }  /* END if ((pobj == NULL) || ...) */
#endif  /* NDEBUG */

  /*
   * its big trouble if ll_struct is null, it would make this
   * entry appear to be the head, so we never let that happen
   */

  if (pobj)
    {
    new->ll_struct = pobj;
    }
  else
    {
    /* WARNING: This mixes tlist_link pointers and ll_struct
         pointers, and may break if the tlist_link we are operating
         on is not the first embeded tlist_link in the surrounding
         structure, e.g. work_task.wt_link_obj */

    new->ll_struct = (void *)new;
    }

  new->ll_prior = head->ll_prior;
  new->ll_next  = head;
  head->ll_prior = new;
  new->ll_prior->ll_next = new; /* now visible to forward iteration */

  return;
  }  /* END append_link() */





/*
 * delete_link - delete an entry from the list
 *
 *	Checks to be sure links exist before breaking them
 *	Note: the old entry is unchanged other than the list links
 *	are cleared.
 */

void delete_link(old)
	struct tlist_link *old;		/* ptr to link to delete */
{

	if ((old->ll_prior != (tlist_link *)0) && 
	    (old->ll_prior != old) && (old->ll_prior->ll_next == old))
		(old->ll_prior)->ll_next = old->ll_next;

	if ((old->ll_next != (tlist_link *)0) &&
	    (old->ll_next != old) && (old->ll_next->ll_prior == old))
		(old->ll_next)->ll_prior = old->ll_prior;

	old->ll_next  = old;
	old->ll_prior = old;
}




/*
 * swap_link - swap the positions of members of a list
 */

void swap_link(pone, ptwo)
	tlist_link *pone;
	tlist_link *ptwo;
{
	tlist_link *p1p;
	tlist_link *p2p;


	if (pone->ll_next == ptwo) {
		delete_link(pone);
		insert_link(ptwo, pone, pone->ll_struct, LINK_INSET_AFTER);
	} else if (ptwo->ll_next == pone) {
		delete_link(ptwo);
		insert_link(pone, ptwo, ptwo->ll_struct, LINK_INSET_AFTER);
	} else {
		p1p = pone->ll_prior;
		p2p = ptwo->ll_prior;
		delete_link(pone);
		insert_link(p2p, pone, pone->ll_struct, LINK_INSET_AFTER);
		delete_link(ptwo);
		insert_link(p1p, ptwo, ptwo->ll_struct, LINK_INSET_AFTER);
	}
}

/*
 * is_linked - determine if entry is in the list 
 *
 * Returns: 1 if in list
 *	    0 if not in list
 */

int is_linked(head, entry)
	tlist_link *head;
	tlist_link *entry;
{
	tlist_link *pl;

	pl = head->ll_next;
	while (pl != head) {
		if (pl == entry)
			return (1);
		pl = pl->ll_next;
	}
	return (0);
}




/*
 * The following routines are replaced by in-line code with the
 * GET_NEXT / GET_PRIOR macroes when NDEBUG is defined, see list_link.h
 */

#ifndef NDEBUG

void *get_next(

  tlist_link  pl,   /* I */
  char	    *file, /* I */
  int	     line) /* I */

  {
  if ((pl.ll_next == NULL) ||
     ((pl.ll_next == &pl) && (pl.ll_struct != NULL))) 
    {
    fprintf(stderr,"Assertion failed, bad pointer in link: file \"%s\", line %d\n", 
      file, 
      line);

    abort();
    }

  return(pl.ll_next->ll_struct);
  }  /* END get_next() */




void *get_prior(

  tlist_link  pl,
  char	    *file,
  int	     line)

  {
  if ((pl.ll_prior == NULL) ||
     ((pl.ll_prior == &pl) && (pl.ll_struct != NULL))) 
    {
    fprintf(stderr,"Assertion failed, null pointer in link: file \"%s\", line %d\n", 
      file, 
      line);

    abort();
    }

  return(pl.ll_prior->ll_struct);
  }  /* END get_prior() */

#endif /* !NDEBUG */


/*
 * list_move - move an entire list from one head to another 
 */

void list_move(from, to)
	tlist_head *from;
	tlist_head *to;
{
	if (from->ll_next == from) {
		to->ll_next = to;
		to->ll_prior = to;
	} else {
		to->ll_next = from->ll_next;
		to->ll_next->ll_prior = to;
		to->ll_prior = from->ll_prior;
		to->ll_prior->ll_next = to;
		CLEAR_HEAD((*from));
	}
}
