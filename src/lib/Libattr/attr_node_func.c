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
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "net_connect.h"
#include "pbs_job.h"
#include "pbs_nodes.h"
#include "pbs_error.h"
#include "log.h"
#if SYSLOG
#include <syslog.h>
#endif

extern int LOGLEVEL;


/*
 * This file contains functions for deriving attribute values from a pbsnode
 * and for updating the "state" (inuse), "node type" (ntype) or "properties"
 * list using the "value" carried in an attribute.
 *
 * Included are:
 *
 * global:
 * decode_state()  "functions for at_decode func pointer"
 * decode_ntype()
 * decode_props()
 *
 * encode_state()  "functions for at_encode func pointer"
 * encode_ntype()
 * encode_props()
 * encode_jobs()
 *
 * set_node_state()  "functions for at_set func pointer"
 * set_node_ntype()
 *
 * node_state()   "functions for at_action func pointer"
 * node_ntype()
 * node_prop_list()
 *
 * free_prop_list()
 * free_prop_attr()  "function for at_free func pointer"
 *
 * local:
 * load_prop()
 * set_nodeflag()
 *
 * The prototypes are declared in "attr_func.h"
 */


/*
 * Set of forward declarations for functions used before defined
 * keeps the compiler happy
*/
static int set_nodeflag A_((char*, short*));
/* static int load_prop A_(( char*, struct prop* )); */


static struct node_state
  {
  short  bit;
  char  *name;
  } ns[] =

  {
    {INUSE_UNKNOWN, ND_state_unknown},
  {INUSE_DOWN,    ND_down},
  {INUSE_OFFLINE, ND_offline},
  {INUSE_RESERVE, ND_reserve},
  {INUSE_JOB,     ND_job_exclusive},
  {INUSE_JOBSHARE, ND_job_sharing},
  {INUSE_BUSY,    ND_busy},
  {0,             NULL}
  };




int PNodeStateToString(

  int   SBM,     /* I (state bitmap) */
  char *Buf,     /* O */
  int   BufSize) /* I */

  {
  if (Buf == NULL)
    {
    return(-1);
    }

  BufSize--;

  Buf[0] = '\0';

  if (SBM & (INUSE_DOWN))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_down, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_OFFLINE))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_offline, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_JOB))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_job_exclusive, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_JOBSHARE))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_job_sharing, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_BUSY))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_busy, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_RESERVE))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_reserve, BufSize - strlen(Buf));
    }

  if (SBM & (INUSE_UNKNOWN))
    {
    if (Buf[0] != '\0')
      strncat(Buf, ",", BufSize - strlen(Buf));

    strncat(Buf, ND_state_unknown, BufSize - strlen(Buf));
    }

  if (Buf[0] == '\0')
    {
    strncpy(Buf, ND_free, BufSize);
    }

  return(0);
  }




/*
 * encode_state
 * Once the node's "inuse" field is converted to an attribute,
 * the attribute can be passed to this function for encoding into
 * an svrattrl structure
 * Returns  <0       an error encountered; value is negative of an error code
 *           0       ok, encode happened and svrattrl created and linked in,
 *       or nothing to encode
 */

int encode_state(

  attribute *pattr, /*struct attribute being encoded  */
  tlist_head *ph, /*head of a list of  "svrattrl"   */
  /*structs which are to be returned*/

  char      *aname, /*attribute's name    */
  char      *rname, /*resource's name (null if none)  */
  int        mode) /*mode code, unused here   */

  {
  int   i;
  svrattrl *pal;
  short   state;

  static char state_str[MAX_ENCODE_BFR];

  if (!pattr)
    {
    return -(PBSE_INTERNAL);
    }

  if (!(pattr->at_flags & ATR_VFLAG_SET))
    {
    /* SUCCESS - attribute not set */

    return(0);
    }

  state = pattr->at_val.at_short & (INUSE_SUBNODE_MASK | INUSE_UNKNOWN);

  if (!state)
    {
    strcpy(state_str, ND_free);
    }
  else
    {
    state_str[0] = '\0';

    for (i = 0;ns[i].name;i++)
      {
      if (state & ns[i].bit)
        {
        if (state_str[0] != '\0')
          strcat(state_str, ",");

        strcat(state_str, ns[i].name);
        }
      }
    }

  pal = attrlist_create(aname, rname, (int)strlen(state_str) + 1);

  if (pal == NULL)
    {
    return -(PBSE_SYSTEM);
    }

  strcpy(pal->al_value, state_str);

  pal->al_flags = ATR_VFLAG_SET;

  append_link(ph, &pal->al_link, pal);

  /* SUCCESS */

  return(0);
  }  /* END encode_state */




/*
 * encode_ntype
 * Once the node's "ntype" field is converted to an attribute,
 * the attribute can be passed to this function for encoding into
 * an svrattrl structure
 * Returns  <0       an error encountered; value is negative of an error code
 *           0       ok, encode happened and svrattrl created and linked in,
 *       or nothing to encode
 */

int
encode_ntype(
  attribute *pattr, /*struct attribute being encoded  */
  tlist_head *ph,  /*head of a list of  "svrattrl"   */
  char *aname, /*attribute's name    */
  char *rname, /*resource's name (null if none)  */
  int mode  /*mode code, unused here   */
)

  {
  svrattrl *pal;
  short  ntype;

  static   char ntype_str[ MAX_ENCODE_BFR ];

  if (!pattr)
    return -(PBSE_INTERNAL);

  if (!(pattr->at_flags & ATR_VFLAG_SET))
    return (0);      /*nothing to report back*/

  ntype = pattr->at_val.at_short & PBSNODE_NTYPE_MASK;

  switch (ntype)
    {
    case NTYPE_CLUSTER:     strcpy(ntype_str, ND_cluster);    break;
    case NTYPE_TIMESHARED:  strcpy(ntype_str, ND_timeshared); break;
    case NTYPE_CLOUD:       strcpy(ntype_str, ND_cloud);      break;
    case NTYPE_VIRTUAL:     strcpy(ntype_str, ND_virtual);    break;
    }

  pal = attrlist_create(aname, rname, (int)strlen(ntype_str) + 1);

  if (pal == (svrattrl *)0)
    return -(PBSE_SYSTEM);

  (void)strcpy(pal->al_value, ntype_str);

  pal->al_flags = ATR_VFLAG_SET;

  append_link(ph, &pal->al_link, pal);

  return (0);             /*success*/
  }




/*
 * encode_jobs
 * Once the node's struct jobinfo pointer is put in the data area of
 * temporary attribute containing a pointer to the parent node, this
 * function will walk the list of jobs and generate the comma separated
 * list to send back via an svrattrl structure.
 * Returns  <0       an error encountered; value is negative of an error code
 *           0       ok, encode happened and svrattrl created and linked in,
 *       or nothing to encode
 */

/* FORMAT for PCONST_ENCOVERHEAD -> '<TID>/<HOST>, ' ... */
/*  overhead supports ',', ' ', '/', and TID <= 99999 */

#define PCONST_ENCOVERHEAD  8

int encode_jobs(

  attribute *pattr, /*struct attribute being encoded  */
  tlist_head *ph, /*head of a  list of "svrattrl"   */
  /*structs which are to be returned*/
  char      *aname, /*attribute's name    */
  char      *rname, /*resource's name (null if none)  */
  int      mode) /*mode code, unused here   */

  {
  svrattrl  *pal;

  struct jobinfo *jip;

  struct pbsnode *pnode;

  struct pbssubn *psubn;
  int  i;
  int  jobcnt;  /*number of jobs using the node     */
  int  strsize; /*computed string size      */
  char  *job_str; /*holds comma separated list of jobs*/

  if (pattr == NULL)
    {
    return (-1);
    }

  if (!(pattr->at_flags & ATR_VFLAG_SET) || !pattr->at_val.at_jinfo)
    {
    return(0);                  /* nothing to report back */
    }

  /* cnt number of jobs and estimate size of string buffer required */

  jobcnt = 0;

  strsize = 1;        /*allow for terminating null char*/

  pnode = pattr->at_val.at_jinfo;

  for (psubn = pnode->nd_psn;psubn != NULL;psubn = psubn->next)
    {
    for (jip = psubn->jobs;jip != NULL;jip = jip->next)
      {
      jobcnt++;

      strsize += strlen(jip->job->ji_qs.ji_jobid) + PCONST_ENCOVERHEAD;
      }
    }    /* END for (psubn) */

  if (jobcnt == 0)
    {
    /* no jobs currently on this node */

    return(0);
    }

  if (!(job_str = (char *)malloc(strsize)))
    {
    return -(PBSE_SYSTEM);
    }

  job_str[0] = '\0';

  i = 0;

  for (psubn = pnode->nd_psn;psubn != NULL;psubn = psubn->next)
    {
    for (jip = psubn->jobs;jip != NULL;jip = jip->next)
      {
      if (i != 0)
        strcat(job_str, ", ");
      else
        i++;

      sprintf(job_str + strlen(job_str), "%d/%s",
              psubn->index,
              jip->job->ji_qs.ji_jobid);
      }
    }    /* END for (psubn) */

  pal = attrlist_create(aname, rname, (int)strlen(job_str) + 1);

  if (pal == NULL)
    {
    free(job_str);

    return -(PBSE_SYSTEM);
    }

  strcpy(pal->al_value, job_str);

  pal->al_flags = ATR_VFLAG_SET;

  free(job_str);

  append_link(ph, &pal->al_link, pal);

  return(0);                  /* success */
  }  /* END encode_jobs() */




/*
 * decode_state
 * In this case, the two arguments that get  used are
 * pattr-- it points to an attribute whose value is a short,
 * and the argument "val".
 * Once the "value" argument, val, is decoded from its form
 * as a string of comma separated substrings, the component
 * values are used to set the appropriate bits in the attribute's
 * value field.
 */

int decode_state(

  attribute *pattr,   /* I (modified) */
  char      *name,    /* attribute name */
  char      *rescn,   /* resource name, unused here */
  char      *val)     /* attribute value */

  {
  int   rc = 0;  /*return code; 0==success*/
  short flag, currflag;
  char  *str;

  char  strbuf[512]; /*should handle most vals*/
  char *sbufp;
  int   slen;

  if (val == NULL)
    {
    return(PBSE_BADNDATVAL);
    }

  /*
   * determine string storage requirement and copy the string "val"
   * to a work buffer area
  */

  slen = strlen(val); /*bufr either on stack or heap*/

  if (slen - 512 < 0)
    {
    sbufp = strbuf;
    }
  else
    {
    if (!(sbufp = (char *)malloc(slen + 1)))
      {
      return(PBSE_SYSTEM);
      }
    }

  strcpy(sbufp, val);

  if ((str = parse_comma_string(sbufp)) == NULL)
    {
    if (slen >= 512)
      free(sbufp);

    return(rc);
    }

  flag = 0;

  if ((rc = set_nodeflag(str, &flag)) != 0)
    {
    if (slen >= 512)
      free(sbufp);

    return(rc);
    }

  currflag = flag;

  /*calling parse_comma_string with a null ptr continues where*/
  /*last call left off.  The initial comma separated string   */
  /*copy pointed to by sbufp is modified with each func call  */

  while ((str = parse_comma_string(NULL)) != NULL)
    {
    if ((rc = set_nodeflag(str, &flag)) != 0)
      break;

    if (((currflag == 0) && flag) || (currflag && (flag == 0)))
      {
      rc = PBSE_MUTUALEX; /*free is mutually exclusive*/

      break;
      }

    currflag = flag;
    }

  if (!rc)
    {
    pattr->at_val.at_short = flag;
    pattr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    }

  if (slen >= 512) /* buffer on heap, not stack */
    free(sbufp);

  return(rc);
  }  /* END decode_state() */





/*
 * decode_ntype
 * In this case, the two arguments that get  used are
 * pattr-- it points to an attribute whose value is a short,
 * and the argument "val".
 * Although we have only "time-shared" and "cluster" at this
 * point in PBS's evolution, there may come a time when other
 * ntype values are needed.  The one thing that is assumed is
 * that the types are going to be mutually exclusive
 */

int
decode_ntype(
  attribute *pattr,
  char *name,             /* attribute name */
  char *rescn,            /* resource name, unused here */
  char *val              /* attribute value */
)

  {
  int rc = 0;  /*return code; 0==success*/
  short tmp = 0;


  if (val == (char *)0)
    rc = (PBSE_BADNDATVAL);
  else if (!strcmp(val, ND_timeshared))
    tmp = NTYPE_TIMESHARED;
  else if (!strcmp(val, ND_cluster))
    tmp = NTYPE_CLUSTER;
  else
    rc = (PBSE_BADNDATVAL);


  if (!rc)
    {
    pattr->at_val.at_short = tmp;
    pattr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    }

  return rc;
  }


/*
 * free_prop_list
 * For each element of a null terminated prop list call free
 * to clean up any string buffer that hangs from the element.
 * After this, call free to remove the struct prop.
 */

void
free_prop_list(struct prop *prop)
  {

  struct prop *pp;

  while (prop)
    {
    pp = prop->next;
    free(prop);
    prop = pp;
    }
  }




/*
 * set_node_state - the information in the "short" attribute, *new, is used to
 *      update the information in the "short" attribute, *pattr.
 *      the mode of the update is goverened by the argument "op"
 *      (SET,INCR,DECR)
 */

int set_node_state(

  attribute *pattr,  /*attribute gets modified    */
  attribute *new,  /*carries new, modifying info*/
  /*that got decoded into 'new"*/
  enum batch_op op)

  {
  int rc = 0;

  assert((pattr != NULL) && (new != NULL) && (new->at_flags & ATR_VFLAG_SET));

  switch (op)
    {

    case SET:

      pattr->at_val.at_short = new->at_val.at_short;

      break;

    case INCR:

      if (pattr->at_val.at_short && new->at_val.at_short == 0)
        {

        rc = PBSE_BADNDATVAL;  /*"free" mutually exclusive*/
        break;
        }

      pattr->at_val.at_short |= new->at_val.at_short;

      break;

    case DECR:

      if (pattr->at_val.at_short && new->at_val.at_short == 0)
        {

        rc = PBSE_BADNDATVAL;  /*"free" mutually exclusive*/
        break;
        }

      pattr->at_val.at_short &= ~new->at_val.at_short;

      break;

    default:
      rc = PBSE_INTERNAL;
    }

  if (!rc)
    pattr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return rc;
  }


/*
 * set_node_ntype - the value entry in attribute "new" is a short.  It was
 *      generated by the decode routine is used to update the
 *      value portion of the attribute *pattr
 *      the mode of the update is goverened by the argument "op"
 *      (SET,INCR,DECR)
 */

int
set_node_ntype(
  attribute *pattr,  /*attribute gets modified    */
  attribute *new,  /*carries new, modifying info*/
  enum batch_op op
)
  {
  int rc = 0;

  assert(pattr && new && (new->at_flags & ATR_VFLAG_SET));

  switch (op)
    {

    case SET:
      pattr->at_val.at_short = new->at_val.at_short;
      break;

    case INCR:

      if (pattr->at_val.at_short != new->at_val.at_short)
        {

        rc = PBSE_MUTUALEX;  /*types are mutually exclusive*/
        }

      break;

    case DECR:

      if (pattr->at_val.at_short != new->at_val.at_short)
        rc = PBSE_MUTUALEX;  /*types are mutually exclusive*/

      else if (pattr->at_val.at_short == NTYPE_TIMESHARED)
        pattr->at_val.at_short = NTYPE_CLUSTER;

      else
        pattr->at_val.at_short = NTYPE_TIMESHARED;

      break;

    default:
      rc = PBSE_INTERNAL;
    }

  if (!rc)
    pattr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return rc;
  }


/*
 * set_nodeflag - Use the input string's value to set a bit in
 *    the "flags" variable pointed to by pflags
 *    Each call sets one more bit in the flags
 *    variable or it clears the flags variable
 *    in the case where *str is the value "free"
 */

static int set_nodeflag(

  char  *str,
  short *pflag)

  {
  int rc = 0;

  if (*str == '\0')
    {
    return(PBSE_BADNDATVAL);
    }

  if (!strcmp(str, ND_free))
    *pflag = 0;
  else if (!strcmp(str, ND_offline))
    *pflag = *pflag | INUSE_OFFLINE;
  else if (!strcmp(str, ND_down))
    *pflag = *pflag | INUSE_DOWN;
  else if (!strcmp(str, ND_reserve))
    *pflag = *pflag | INUSE_RESERVE;
  else
    {
    rc = PBSE_BADNDATVAL;
    }

  return(rc);
  }





/*
 * node_state - Either derive a "state" attribute from the node
 *  or update node's "inuse" field using the "state"
 *  attribute.
 */

int node_state(

  attribute *new, /*derive state into this attribute*/
  void     *pnode, /*pointer to a pbsnode struct    */
  int      actmode) /*action mode; "NEW" or "ALTER"   */

  {
  int rc = 0;

  struct pbsnode* np;

  np = (struct pbsnode*)pnode; /*because of def of at_action  args*/

  switch (actmode)
    {

    case ATR_ACTION_NEW:  /*derive attribute*/

      new->at_val.at_short = np->nd_state;

      break;

    case ATR_ACTION_ALTER:

      np->nd_state = new->at_val.at_short;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }

  return(rc);
  }





/*
 * node_ntype - Either derive an "ntype" attribute from the node
 *  or update node's "ntype" field using the
 *  attribute's data
 */

int
node_ntype(
  attribute *new,  /*derive ntype into this attribute*/
  void *pnode,  /*pointer to a pbsnode struct     */
  int actmode /*action mode; "NEW" or "ALTER"   */
)
  {
  int rc = 0;

  struct pbsnode* np;


  np = (struct pbsnode*)pnode; /*because of def of at_action  args*/

  switch (actmode)
    {

    case ATR_ACTION_NEW:  /*derive attribute*/
      new->at_val.at_short = np->nd_ntype;
      break;

    case ATR_ACTION_ALTER:
      np->nd_ntype = new->at_val.at_short;
      break;

    default:
      rc = PBSE_INTERNAL;
    }

  return rc;
  }




/*
 * node_prop_list - Either derive a "prop list" attribute from the node
 *      or update node's prop list from attribute's prop list.
 */

int node_prop_list(

  attribute *new, /*derive props into this attribute*/
  void     *pnode, /*pointer to a pbsnode struct     */
  int      actmode) /*action mode; "NEW" or "ALTER"   */

  {
  int   rc = 0;

  struct pbsnode  *np;
  attribute  temp;

  np = (struct pbsnode*)pnode; /*because of at_action arg type*/

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a property list, then copy array_strings    */
      /* into temp to use to setup a copy, otherwise setup empty */

      if (np->nd_prop != NULL)
        {
        /* setup temporary attribute with the array_strings */
        /* from the node        */

        temp.at_val.at_arst = np->nd_prop;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_ARST;

        rc = set_arst(new, &temp, SET);
        }
      else
        {
        /* Node has no properties, setup empty attribute */

        new->at_val.at_arst = 0;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_ARST;
        }

      break;

    case ATR_ACTION_ALTER:

      /* update node with new attr_strings */

      np->nd_prop = new->at_val.at_arst;

      /* update number of properties listed in node */
      /* does not include name and subnode property */

      if (np->nd_prop)
        np->nd_nprops = np->nd_prop->as_usedptr;
      else
        np->nd_nprops = 0;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }  /* END node_prop_list() */

/*
 * node_status_list - Either derive a "status list" attribute from the node
 *                 or update node's status list from attribute's status list.
 */

int node_status_list(

  attribute *new,           /*derive status into this attribute*/
  void      *pnode,         /*pointer to a pbsnode struct     */
  int        actmode)       /*action mode; "NEW" or "ALTER"   */

  {
  int              rc = 0;

  struct pbsnode  *np;
  attribute        temp;

  np = (struct pbsnode *)pnode;    /* because of at_action arg type */

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a status list, then copy array_strings    */
      /* into temp to use to setup a copy, otherwise setup empty */

      if (np->nd_status != NULL)
        {
        /* setup temporary attribute with the array_strings */
        /* from the node                                    */

        temp.at_val.at_arst = np->nd_status;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_ARST;

        rc = set_arst(new, &temp, SET);
        }
      else
        {
        /* node has no properties, setup empty attribute */

        new->at_val.at_arst = NULL;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_ARST;
        }

      break;

    case ATR_ACTION_ALTER:

      if (np->nd_status != NULL)
        {
        free(np->nd_status->as_buf);
        free(np->nd_status);

        np->nd_status = NULL;
        }

      /* update node with new attr_strings */

      np->nd_status = new->at_val.at_arst;

      new->at_val.at_arst = NULL;

      /* update number of status items listed in node */
      /* does not include name and subnode property */

      if (np->nd_status != NULL)
        np->nd_nstatus = np->nd_status->as_usedptr;
      else
        np->nd_nstatus = 0;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }  /* END node_status_list() */


/** Node additional properties list
 *
 * Either derive a "prop list" attribute from the node or update nodes prop list
 * from attributes prop list.
 */
int node_adprop_list(

  attribute *new, /*derive props into this attribute*/
  void     *pnode, /*pointer to a pbsnode struct     */
  int      actmode) /*action mode; "NEW" or "ALTER"   */

  {
  int   rc = 0;

  struct pbsnode  *np;
  attribute  temp;

  np = (struct pbsnode*)pnode; /*because of at_action arg type*/

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a property list, then copy array_strings    */
      /* into temp to use to setup a copy, otherwise setup empty */

      if (np->x_ad_properties != NULL)
        {
        /* setup temporary attribute with the array_strings */
        /* from the node        */

        temp.at_val.at_arst = np->x_ad_properties;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_ARST;

        rc = set_arst(new, &temp, SET);
        }
      else
        {
        /* Node has no properties, setup empty attribute */

        new->at_val.at_arst = 0;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_ARST;
        }

      break;

    case ATR_ACTION_ALTER:

      /* update node with new attr_strings */

      np->x_ad_properties = new->at_val.at_arst;

      /* update number of properties listed in node */
      /* does not include name and subnode property */

      if (np->x_ad_properties)
        np->xn_ad_prop = np->x_ad_properties->as_usedptr;
      else
        np->x_ad_properties = 0;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }  /* END node_adprop_list() */


/** Either derive a no_multinode attribute from the node or update node
 *
 * @param new
 * @param pnode
 * @param actmode
 * @return
 */
int node_no_multinode(attribute *new, void *pnode, int actmode)
  {
  int rc = 0;
  struct pbsnode  *np;

  np = (struct pbsnode *)pnode;    /* because of at_action arg type */

  switch (actmode)
    {

    case ATR_ACTION_NEW:
      new->at_val.at_long  = np->nd_no_multinode;
      new->at_flags       = ATR_VFLAG_SET;
      new->at_type        = ATR_TYPE_LONG;
      break;

    case ATR_ACTION_ALTER:
      np->nd_no_multinode = new->at_val.at_long;
      break;

    default:
      rc = PBSE_INTERNAL;
      break;
    }  /* END switch(actmode) */

  return(rc);
  }


/** Either derive a cloud attribute from the node or update node's cloud
 *  from attribute's list
 *
 *  @param new derive queue into this attribute
 *  @param pnode pointer to a pbsnode struct
 *  @param actmode NEW or ALTER
 */
int node_cloud(attribute *new, void *pnode, int actmode)
  {
  int              rc = 0;

  struct pbsnode  *np;
  attribute        temp;

  np = (struct pbsnode *)pnode;    /* because of at_action arg type */

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a queue, then copy string into temp  */
      /* to use to setup a copy, otherwise setup empty   */

      if (np->cloud != NULL)
        {
        /* setup temporary attribute with the string from the node */

        temp.at_val.at_str = np->cloud;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_STR;

        rc = set_note_str(new, &temp, SET); /* TODO change */
        }
      else
        {
        /* node has no properties, setup empty attribute */

        new->at_val.at_str  = NULL;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_STR;
        }

      break;

    case ATR_ACTION_ALTER:

      if (np->cloud != NULL)
        {
        free(np->cloud);

        np->cloud = NULL;
        }

      /* update node with new string */

      np->cloud = new->at_val.at_str;

      new->at_val.at_str = NULL;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }


/** Either derive a queue attribute from the node or update node's queue
 *  from attribute's list
 *
 *  @param new derive queue into this attribute
 *  @param pnode pointer to a pbsnode struct
 *  @param actmode NEW or ALTER
 */
int node_queue(attribute *new, void *pnode, int actmode)
  {
  int              rc = 0;

  struct pbsnode  *np;
  attribute        temp;

  np = (struct pbsnode *)pnode;    /* because of at_action arg type */

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a queue, then copy string into temp  */
      /* to use to setup a copy, otherwise setup empty   */

      if (np->queue != NULL)
        {
        /* setup temporary attribute with the string from the node */

        temp.at_val.at_str = np->queue;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_STR;

        rc = set_note_str(new, &temp, SET); /* TODO change */
        }
      else
        {
        /* node has no properties, setup empty attribute */

        new->at_val.at_str  = NULL;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_STR;
        }

      break;

    case ATR_ACTION_ALTER:

      if (np->queue != NULL)
        {
        free(np->queue);

        np->queue = NULL;
        }

      /* update node with new string */

      np->queue = new->at_val.at_str;

      new->at_val.at_str = NULL;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }

/*
 * node_note - Either derive a note attribute from the node
 *             or update node's note from attribute's list.
 */

int node_note(

  attribute *new,           /*derive status into this attribute*/
  void      *pnode,         /*pointer to a pbsnode struct     */
  int        actmode)       /*action mode; "NEW" or "ALTER"   */

  {
  int              rc = 0;

  struct pbsnode  *np;
  attribute        temp;

  np = (struct pbsnode *)pnode;    /* because of at_action arg type */

  switch (actmode)
    {

    case ATR_ACTION_NEW:

      /* if node has a note, then copy string into temp  */
      /* to use to setup a copy, otherwise setup empty   */

      if (np->nd_note != NULL)
        {
        /* setup temporary attribute with the string from the node */

        temp.at_val.at_str = np->nd_note;
        temp.at_flags = ATR_VFLAG_SET;
        temp.at_type  = ATR_TYPE_STR;

        rc = set_note_str(new, &temp, SET);
        }
      else
        {
        /* node has no properties, setup empty attribute */

        new->at_val.at_str  = NULL;
        new->at_flags       = 0;
        new->at_type        = ATR_TYPE_STR;
        }

      break;

    case ATR_ACTION_ALTER:

      if (np->nd_note != NULL)
        {
        free(np->nd_note);

        np->nd_note = NULL;
        }

      /* update node with new string */

      np->nd_note = new->at_val.at_str;

      new->at_val.at_str = NULL;

      break;

    default:

      rc = PBSE_INTERNAL;

      break;
    }  /* END switch(actmode) */

  return(rc);
  }  /* END node_note() */


int set_queue_str(
    struct attribute *attr,
    struct attribute *new,
    enum batch_op     op)
  {
  static char id[] = "set_queue_str";
  size_t nsize;
  int rc = 0;

  assert(attr && new && new->at_val.at_str && (new->at_flags & ATR_VFLAG_SET));
  nsize = strlen(new->at_val.at_str);    /* length of new note */

  if (nsize > PBS_MAXQUEUENAME)
    {
    sprintf(log_buffer, "Warning: Client attempted to set queue with len (%d) > PBS_MAXQUEUENAME (%d)",
            (int)nsize,
            MAX_NOTE);

    log_record(
      PBSEVENT_SECURITY,
      PBS_EVENTCLASS_REQUEST,
      id,
      log_buffer);

    rc = PBSE_BADNDATVAL;
    }

  if (strchr(new->at_val.at_str, '\n') != NULL)
    {
    sprintf(log_buffer, "Warning: Client attempted to set queue with a newline char");

    log_record(
      PBSEVENT_SECURITY,
      PBS_EVENTCLASS_REQUEST,
      id,
      log_buffer);

    rc = PBSE_BADNDATVAL;
    }

  if (rc != 0)
    return(rc);

  rc = set_str(attr, new, op);

  return(rc);

  }


/*
 * a set_str() wrapper with sanity checks for notes
 */

int set_note_str(

  struct attribute *attr,
  struct attribute *new,
  enum batch_op     op)

  {
  static char id[] = "set_note_str";
  size_t nsize;
  int rc = 0;

  assert(attr && new && new->at_val.at_str && (new->at_flags & ATR_VFLAG_SET));
  nsize = strlen(new->at_val.at_str);    /* length of new note */

  if (nsize > MAX_NOTE)
    {
    sprintf(log_buffer, "Warning: Client attempted to set note with len (%d) > MAX_NOTE (%d)",
            (int)nsize,
            MAX_NOTE);

    log_record(
      PBSEVENT_SECURITY,
      PBS_EVENTCLASS_REQUEST,
      id,
      log_buffer);

    rc = PBSE_BADNDATVAL;
    }

  if (strchr(new->at_val.at_str, '\n') != NULL)
    {
    sprintf(log_buffer, "Warning: Client attempted to set note with a newline char");

    log_record(
      PBSEVENT_SECURITY,
      PBS_EVENTCLASS_REQUEST,
      id,
      log_buffer);

    rc = PBSE_BADNDATVAL;
    }

  if (rc != 0)
    return(rc);

  rc = set_str(attr, new, op);

  return(rc);
  }  /* END set_note_str() */


/* END attr_node_func.c */

