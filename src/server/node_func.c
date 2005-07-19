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
 * node_func.c - various functions dealing with nodes, properties and
 *		 the following global variables:
 *	pbsnlist     -	the server's global node list 
 *	svr_totnodes -	total number of pbshost entries
 *	svr_clnodes  -	number of cluster (space-shared) nodes 
 *	svr_tsnodes  -	number of time-shared nodes, one per host
 *
 * Included functions are:
 *	find_nodebyname() - find a node host with a given name
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include "pbs_ifl.h"
#include "libpbs.h"
#include "list_link.h"
#include "attribute.h"
#include "credential.h"
#include "batch_request.h"
#include "server_limits.h"
#include "server.h"
#include "job.h"
#include "pbs_nodes.h"
#include "pbs_error.h"
#include "log.h"
#include "pbs_proto.h"

#if !defined(H_ERRNO_DECLARED)
extern int h_errno;
#endif



/* Global Data */

extern int	 svr_totnodes;
extern int	 svr_tsnodes;
extern int	 svr_clnodes;
extern char	*path_nodes_new;
extern char	*path_nodes;
extern char	*path_nodestate;
extern int       LOGLEVEL;


/* Functions in this file
 * find_nodebyname()   -     given a node host name, search pbsndlist 
 * find_subnodebyname() -     given a subnode name
 * save_characteristic() - save the the characteristics of the node along with
 *		the address of the node
 * chk_characteristic() -  check for changes to the node's set of
 *		characteristics and set appropriate flag bits in the "need_todo"
 *		location depending on which characteristics changed
 * status_nodeattrib() -    add status of each requested (or all) node-attribute
 *		to the status reply
 * initialize_pbsnode() -   performs node initialization on a new node
 * effective_node_delete() -  effectively deletes a node from the server's node
 *		list by setting the node's "deleted" bit
 * setup_notification() -   sets mechanism for notifying other hosts about a new
 *		host
 * process_host_name_part() - processes hostname part of a batch request into a
 *		prop structure, host's IP addresses into an array, and node
 *		node type (cluster/time-shared) into an int variable
 * update_nodes_file() -    used to update the nodes file when certain changes
 *		occur to the server's internal nodes list
 * recompute_ntype_cnts -   Recomputes the current number of cluster nodes and
 *		current number of time-shared nodes
 * create_pbs_node -	create basic node structure for adding a node
 */


#include <work_task.h>

extern void ping_nodes(struct work_task *);



struct pbsnode *PGetNodeFromAddr(

  pbs_net_t addr)  /* I */

  {
  int nindex;
  int aindex;

  for (nindex = 0;nindex < svr_totnodes;nindex++)
    {
    if ((pbsndlist[nindex] == NULL) || 
        (pbsndlist[nindex]->nd_addrs == NULL))
      continue;

    for (aindex = 0;aindex < 10;aindex++)
      {
      if (pbsndlist[nindex]->nd_addrs[aindex] == 0)
        break;

      if (pbsndlist[nindex]->nd_addrs[aindex] == addr)
        {
        return(pbsndlist[nindex]);
        }
      }    /* END for (aindex) */
    }      /* END for (nindex) */

  return(NULL);
  }  /* END PGetNodeFromAddr() */




void bad_node_warning(
		
  pbs_net_t addr)  /* I */

  {
  int i;
  time_t now, last;

  for (i = 0;i < svr_totnodes;i++) 
    {
    if (pbsndlist[i] == NULL)
      {
      sprintf(log_buffer,"ALERT:  node table is corrupt at index %d",
        i);

      log_event(
        PBSEVENT_ADMIN,
        PBS_EVENTCLASS_SERVER,
        "WARNING",
        log_buffer);

      continue;
      }

    if (pbsndlist[i]->nd_addrs[0] == addr) 
      {
      now = time(0);

      last = pbsndlist[i]->nd_warnbad;
 
      if (last && (now - last < 3600))
        {
        return;
        }

      /*
      ** once per hour, log a warning that we can't reach the node, and
      ** ping_nodes to check and reset the node's state.
      */

      sprintf(log_buffer,"ALERT: unable to contact node %s",
        pbsndlist[i]->nd_name);

      log_event(
        PBSEVENT_ADMIN, 
        PBS_EVENTCLASS_SERVER, 
        "WARNING",
        log_buffer);

      /*
       *  (void)set_task(WORK_Timed,now + 5,ping_nodes,NULL);
       */

      pbsndlist[i]->nd_warnbad = now;

      break;
      }  /* END if (pbsndlist[i]->nd_addrs[0] == addr) */
    }    /* END for (i = 0) */

  return;
  }      /* END bad_node_warning() */




/*
 * return 0 if addr is a mom node and node is in bad state,
 * return 1 otherwise (it is not a MOM node, or it's state is OK)
 */

int addr_ok(

  pbs_net_t addr)  /* I */

  {
  int i;
  int status = 1;  /* assume destination host is healthy */

  if (pbsndlist != NULL)
    {
    for (i = 0;i < svr_totnodes;i++)
      {
      /* NOTE:  should walk thru all nd_addrs for multi-homed hosts */

      if (pbsndlist[i]->nd_addrs[0] != addr)
        continue;

      if (pbsndlist[i]->nd_state & (INUSE_DOWN|INUSE_DELETED|INUSE_UNKNOWN))
        {
        status = 0;
        }

      break;
      }
    }

  return(status);
  }  /* END addr_ok() */




typedef struct tree_t {
       u_long          key;
       struct pbsnode  *nodep;
       struct tree_t   *left, *right;
} tree;

extern void tinsert(const u_long key, struct pbsnode *nodep, tree **rootp);
extern void *tdelete(const u_long key, tree **rootp);




/*
 * find_nodebyname() - find a node host by its name
 */

struct pbsnode *find_nodebyname(

  char *nodename) /* I */

  {
  int		i;
  char		*pslash;
  struct pbsnode *pnode;

  if ((pslash = strchr(nodename,(int)'/')) != NULL)
    *pslash = '\0';

  pnode = NULL;

  for (i = 0;i < svr_totnodes;i++) 
    {
    if (pbsndlist[i]->nd_state & INUSE_DELETED)
      continue;

    if (strcasecmp(nodename,pbsndlist[i]->nd_name) == 0) 
      {
      pnode = pbsndlist[i];

      break;
      }
    }

  if (pslash != NULL)
    *pslash = '/';	/* restore the slash */

  return(pnode);
  }  /* END find_nodebyname() */





/*
 * find_subnodebyname() - find a subnode by its (parent's) name
 *
 *	returns first of N subnodes for a given node name
 */

struct pbssubn *find_subnodebyname(

  char *nodename)

  {
  struct pbsnode  *pnode;

  if ((pnode = find_nodebyname(nodename)) == NULL)
    {
    return(NULL);
    }

  return(pnode->nd_psn);
  }  /* END find_subnodebyname() */

	


static struct pbsnode	*old_address = 0;			/*node in question */
static struct prop	*old_first = (struct prop *)0xdead;	/*node's first prop*/
static struct prop      *old_f_st = (struct prop *)0xdead;      /*node's first status*/
static short		old_state = (short)0xdead;	/*node's   state   */
static short		old_ntype = (short)0xdead;	/*node's   ntype   */
static int		old_nprops = 0xdead;		/*node's   nprops  */
static int             old_nstatus = 0xdead;            /*node's   nstatus */




/*
 * save_characteristic() -  save the characteristic values of the node along
 *			    with the address of the node
 */

void save_characteristic( 

  struct pbsnode *pnode)

  {
  if (pnode == (struct pbsnode *)0)
    {
    return;
    }

  old_address = pnode;
  old_state =   pnode->nd_state;
  old_ntype =   pnode->nd_ntype;
  old_nprops =  pnode->nd_nprops;
  old_nstatus = pnode->nd_nstatus;
  old_first =   pnode->nd_first;
  old_f_st =    pnode->nd_f_st;

  return;
  }  /* END save_characteristic() */





/*
 * chk_characteristic() -  check the value of the characteristics against
 *   that which was saved earlier.
 *   Returns:
 *   -1  if parent address doesn't match saved parent address
 *    0  if successful check.  *pneed_todo gets appropriate
 *       bit(s) set depending on the results of the check.
 *       The "returned" bits get used by the caller.
 */


int chk_characteristic(

  struct pbsnode *pnode,      /* I */
  int            *pneed_todo) /* O */

  {
  short	tmp;
  char  tmpLine[1024];

  if ((pnode != old_address) || (pnode == NULL)) 
    {
    /* didn't do save_characteristic() before issuing chk_characteristic() */

    old_address = NULL;

    /* FAILURE */

    return(-1);
    }

  tmp = pnode->nd_state; 

  tmpLine[0] = '\0';

  if (tmp != old_state) 
    {
    if ((tmp & INUSE_OFFLINE) && !(old_state & INUSE_OFFLINE))
      {
      *pneed_todo |= WRITENODE_STATE;		/*marked offline */

      strcat(tmpLine,"offline set");
      }

    if (!(tmp & INUSE_OFFLINE) && old_state & INUSE_OFFLINE)
      {
      *pneed_todo |= WRITENODE_STATE;		/*removed offline*/

      strcat(tmpLine,"offline cleared");
      }

    if ((tmp & INUSE_DELETED) && (old_state & INUSE_OFFLINE))
      *pneed_todo |= WRITENODE_STATE;	       /*offline & now deleted*/

    if ((tmp & INUSE_DELETED) && !(old_state & INUSE_DELETED))
      *pneed_todo |= WRITE_NEW_NODESFILE;	/*node being deleted*/
    }

  if (tmpLine[0] != '\0')
    {
    if (LOGLEVEL >= 3)
      {
      sprintf(log_buffer,"node %s state modified (%s)\n",
        pnode->nd_name,
        tmpLine);

      log_event(
        PBSEVENT_ADMIN,
        PBS_EVENTCLASS_SERVER,
        "chk_characteristic",
        log_buffer);
      }
    }

  tmp = pnode->nd_ntype;

  if (tmp != old_ntype)
    *pneed_todo |= WRITE_NEW_NODESFILE;

  if ((old_nprops != pnode->nd_nprops) || (old_first != pnode->nd_first))
    *pneed_todo |= WRITE_NEW_NODESFILE;

  old_address = NULL;

  return(0);
  }  /* END chk_characteristic() */





/* status_nodeattrib() - add status of each requested (or all) node-attribute to
 *			 the status reply
 *
 *      Returns:     0	is success
 *                != 0	is error, if a node-attribute is incorrectly specified, *bad is
 *			set to the node-attribute's ordinal position 
 */

int status_nodeattrib(

  svrattrl        *pal,	/*an svrattrl from the request  */
  attribute_def   *padef,	/*the defined node attributes   */
  struct pbsnode  *pnode,	/*no longer an attribute ptr	*/
  int              limit,	/*number of array elts in padef */
  int              priv,	/*requester's privilege		*/

  list_head       *phead,	/*heads list of svrattrl structs that hang */
				/*off the brp_attr member of the status sub*/
				/*structure in the request's "reply area"  */

  int             *bad)	/*if node-attribute error record it's*/
				/*list position here                 */
  {
  int   i;
  int   rc = 0;		/*return code, 0 == success*/
  int   index;
  int   nth;		/*tracks list position (ordinal tacker)   */

  attribute	atemp[ND_ATR_LAST];	/*temporary array of attributes	  */

  priv &= ATR_DFLAG_RDACC;  		/* user-client privilege          */
	
  for (i = 0;i < ND_ATR_LAST;i++) 
    {   
    /*set up attributes using data from node*/

    if (!strcmp((padef + i)->at_name,ATTR_NODE_state))
      atemp[i].at_val.at_short = pnode->nd_state;
    else if (!strcmp ((padef + i)->at_name, ATTR_NODE_properties))
      atemp[i].at_val.at_arst = pnode->nd_prop;
    else if (!strcmp ((padef + i)->at_name, ATTR_NODE_status))
      atemp[i].at_val.at_arst = pnode->nd_status;
    else if (!strcmp ((padef + i)->at_name, ATTR_NODE_ntype))
      atemp[i].at_val.at_short = pnode->nd_ntype;
    else if (!strcmp ((padef + i)->at_name, ATTR_NODE_jobs))
      atemp[i].at_val.at_jinfo = pnode;
    else if (!strcmp ((padef + i)->at_name, ATTR_NODE_np))
      atemp[i].at_val.at_long = pnode->nd_nsn;
    else 
      {
      /*we don't ever expect this*/

      *bad = 0; 

      return(PBSE_UNKNODEATR);
      }

    atemp[i].at_flags = ATR_VFLAG_SET; /*artificially set the value's flags*/
    }

  if (pal != NULL) 
    {        /*caller has requested status on specific node-attributes*/
    nth = 0;

    while (pal != NULL) 
      {
      ++nth;

      index = find_attr(padef,pal->al_name,limit);

      if (index < 0) 
        {
        *bad = nth;		/*name in this position can't be found*/

        rc = PBSE_UNKNODEATR;

        break;
        }

      if ((padef + index)->at_flags & priv) 
        {
        rc = ((padef + index)->at_encode(
          &atemp[index],
          phead,
          (padef + index)->at_name,
          NULL,
          ATR_ENCODE_CLIENT));

        if (rc < 0)
          {
          rc = -rc;

          break;
          }
        else
          {
          /* encoding was successful */

          rc = 0;
          }
        }

      pal = (svrattrl *)GET_NEXT(pal->al_link);
      }  /* END while (pal != NULL) */
    }    /* END if (pal != NULL) */
  else 
    {        
    /* non-specific request, return all readable attributes */

    for (index = 0; index < limit; index++) 
      {
      if (((padef + index)->at_flags & priv) &&
         !((padef + index)->at_flags & ATR_DFLAG_NOSTAT)) 
        {
        rc = (padef + index)->at_encode(
              &atemp[index],
              phead,
              (padef + index)->at_name,
              NULL,
              ATR_ENCODE_CLIENT);

        if (rc < 0)
          {
          rc = -rc;

          break;
          }
        else
          {
          /* encoding was successful */

          rc = 0;
          }
        }
      }    /* END for (index) */
    }      /* END else (pal != NULL) */

  return(rc);
  }  /* END status_nodeattrib() */





/*
 * initialize_pbsnode - carries out initialization on a new
 *	pbs node.  The assumption is that all the parameters are valid.
*/

static void initialize_pbsnode(

  struct pbsnode *pnode,
  char           *pname, /* node name */
  u_long         *pul,	 /* host byte order array */
			 /* ipaddrs for this node */
  int             ntype) /* time-shared or cluster */

  {
  char *id = "initialize_pbsnode";

  int i;

  memset(pnode,0,sizeof(struct pbsnode));

  pnode->nd_name    = pname;
  pnode->nd_stream  = -1;
  pnode->nd_addrs   = pul;	    /*list of host byte order */
  pnode->nd_ntype   = ntype;
  pnode->nd_nsn     = 0;
  pnode->nd_nsnfree = 0;
  pnode->nd_needed  = 0;
  pnode->nd_order   = 0;
  pnode->nd_prop    = (struct array_strings *)0;
  pnode->nd_status  = (struct array_strings *)0;
  pnode->nd_psn     = NULL;
  pnode->nd_state   = INUSE_UNKNOWN | INUSE_NEEDS_HELLO_PING | INUSE_DOWN;
  pnode->nd_first   = init_prop(pnode->nd_name);
  pnode->nd_last    = pnode->nd_first;
  pnode->nd_f_st    = init_prop(pnode->nd_name);
  pnode->nd_l_st    = pnode->nd_f_st;
  pnode->nd_nprops  = 0;
  pnode->nd_nstatus = 0;
  pnode->nd_warnbad = 0;

  for (i = 0;pul[i];i++) 
    {
    if (LOGLEVEL >= 6)
      {
      sprintf(log_buffer,"node '%s' allows trust for ipaddr %ld.%ld.%ld.%ld\n",
        pnode->nd_name,
        (pul[i] & 0xff000000) >> 24,
        (pul[i] & 0x00ff0000) >> 16,
        (pul[i] & 0x0000ff00) >> 8,
        (pul[i] & 0x000000ff));
                                                                                               
      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);
      }

    tinsert(pul[i],pnode,&ipaddrs);
    }  /* END for (i) */

  return;
  }  /* END initialize_pbsnode() */




/*
 * subnode_delete - delete the specified subnode
 *	by marking it deleted
 */

static void subnode_delete(psubn)
	struct pbssubn *psubn;
{
	struct jobinfo	*jip, *jipt;

	for (jip = psubn->jobs; jip; jip = jipt) {
       		jipt = jip->next;
       		free (jip); 
	}
	psubn->host  = (struct pbsnode *)0;
	psubn->jobs  = (struct jobinfo *)0;
	psubn->next  = (struct pbssubn *)0;
	psubn->inuse = INUSE_DELETED; 
}




void effective_node_delete(

  struct pbsnode *pnode)

  {
  struct pbssubn  *psubn;
  struct pbssubn  *pnxt;
  u_long          *up;

  psubn = pnode->nd_psn;

  while (psubn) 
    {
    pnxt = psubn->next;

    subnode_delete(psubn);

    psubn = pnxt;
    }

  pnode->nd_last->next = (struct prop *)0; /*just in case*/
  pnode->nd_last = (struct prop *)0;

  free_prop_list (pnode->nd_first);

  pnode->nd_first = (struct prop *)0;

  for (up = pnode->nd_addrs;*up != 0;up++) 
    {
    /* del node's IP addresses from tree  */

    tdelete(*up,&ipaddrs);
    }

  if (pnode->nd_addrs != NULL) 
    {	/* remove array of IP addresses */
    free(pnode->nd_addrs);

    pnode->nd_addrs = NULL;
    }

  tdelete((u_long)pnode->nd_stream,&streams); /*take stream out of tree*/

  rpp_close(pnode->nd_stream);
  free(pnode->nd_name);

  pnode->nd_name = NULL;
  pnode->nd_stream = -1;
  pnode->nd_state  = INUSE_DELETED;
  pnode->nd_nsn    = 0;
  pnode->nd_nsnfree= 0;

  return;
  }



/*
 *	setup_notification -  Sets up the  mechanism for notifying
 *			      other members of the server's node
 *			      pool that a new node was added manually
 *			      via qmgr.  Actual notification occurs some
 *			      time later through the ping_nodes mechanism
*/

void setup_notification()

  {
  int i;

  for (i = 0;i < svr_totnodes;i++) 
    {
    if (pbsndlist[i]->nd_state & INUSE_DELETED)
      continue;

    pbsndlist[i]->nd_state |= INUSE_NEEDS_HELLO_PING;
    }

  return;
  }




static int process_host_name_part(

  char	  *objname,	/* node to be's name */ 
  u_long **pul,		/* 0 terminated host addrs array */
  char	 **pname,	/* node name w/o any :ts         */
  int	  *ntype)	/* node type; time-shared, not   */

  {
  struct hostent *hp;
  struct in_addr  addr;
  char		 *phostname;     /*caller supplied hostname   */
  int		  len, i;

  len = strlen(objname);

  if (len == 0)
    {
    return(PBSE_UNKNODE);
    }

  phostname = strdup(objname);

  if (phostname == NULL)
    {
    return(PBSE_SYSTEM);
    }

  *ntype = NTYPE_CLUSTER;

  if ((len >= 3) && !strcmp(&phostname[len - 3],":ts")) 
    {
    phostname[len - 3] = '\0';
    *ntype = NTYPE_TIMESHARED;
    }

  if ((hp = gethostbyname(phostname)) == NULL) 
    {
    sprintf(log_buffer,"host %s not found",
      objname);

    free(phostname);

    return(PBSE_UNKNODE);
    }

  memcpy((char *)&addr,hp->h_addr,hp->h_length);

  if (hp->h_addr_list[1] == NULL) 
    {
    /* weren't given canonical name */

    char *hname;

    if ((hp = gethostbyaddr(
          (void *)&addr,
          sizeof(struct in_addr),
          hp->h_addrtype)) == NULL) 
      {
      sprintf(log_buffer,"addr not found h_errno=%d errno=%d",
        h_errno, 
        errno);

      free(phostname);

      return(PBSE_UNKNODE);
      }
	
    hname = (char *)strdup(hp->h_name); /*canonical name in theory*/

    if (hname == (char *)0) 
      {
      free(phostname);

      return(PBSE_SYSTEM);
      }

    if ((hp = gethostbyname(hname)) == NULL) 
      {
      sprintf(log_buffer,"bad cname %s, h_errno=%d errno=%d",
        hname,h_errno,errno);

      free(hname);

      free(phostname);

      return(PBSE_UNKNODE);
      }

    free(hname);
    }  /* END if (hp->h_addr_list[1] == NULL) */

  for (i=0; hp->h_addr_list[i]; i++);	/*count how many ipadrs*/

  *pul = (u_long *)malloc(sizeof(u_long) * (i+1));  /*null end it*/

  if (*pul == (u_long *)0) 
    {
    free(phostname);
    }

  for (i = 0;hp->h_addr_list[i];i++) 
    {
    u_long	ipaddr;

    memcpy((char *)&addr,hp->h_addr_list[i],hp->h_length);

    ipaddr = ntohl(addr.s_addr);

    (*pul)[i] = ipaddr;
    }

  (*pul)[i] = 0;			/* null term array ip adrs*/

  *pname = phostname;			/* return node name	  */

  return(0);				/* function successful    */
  }  /* END process_host_name_part() */





/*
 *	update_nodes_file - When called, this function will update
 *			    the nodes file.  Specifically, it will
 *			    walk the server's array of pbsnodes
 *			    constructing for each entry a nodes file
 *			    line if that entry is not marked as deleted.
 *			    These are written to a temporary file.
 *			    Upon successful conclusion that file replaces
 *			    the nodes file.
*/			    

int update_nodes_file()

  {
  static char	id[] = "update_nodes_file";
  struct pbsnode  *np;
  int	i, j;
  FILE	*nin;

  if (LOGLEVEL >= 2)
    {
    DBPRT(("%s: entered\n",
      id))
    }

  if ((nin = fopen(path_nodes_new,"w")) == NULL) 
    {
    log_event(
      PBSEVENT_ADMIN, 
      PBS_EVENTCLASS_SERVER, 
      "nodes",
      "Node description file update failed");

    return(-1);
    }

  if ((svr_totnodes == 0) || (pbsndmast == NULL)) 
    {
    log_event(
      PBSEVENT_ADMIN, 
      PBS_EVENTCLASS_SERVER, 
      "nodes",
      "Server has empty nodes list");

    fclose(nin);

    return(-1);
    }

  /* for each node ... */

  for (i = 0;i < svr_totnodes;++i) 
    {
    np = pbsndmast[i];

    if (np->nd_state & INUSE_DELETED)
      continue;

    /* ... write its name, and if time-shared, append :ts */

    fprintf(nin,"%s", 
      np->nd_name);	/* write name */

    if (np->nd_ntype == NTYPE_TIMESHARED)
      fprintf(nin, ":ts"); 

    /* if number of subnodes is gt 1, write that; if only one,   */
    /* don't write to maintain compatability with old style file */

    if (np->nd_nsn > 1)
      fprintf(nin," %s=%d",
        ATTR_NODE_np,
        np->nd_nsn);

    /* write out properties */

    for (j = 0;j < np->nd_nprops - 1;++j) 
      {
      fprintf(nin," %s", 
        np->nd_prop->as_string[j]);
      }

    /* finish off line with new-line */

   fprintf(nin,"\n");

   fflush(nin);

   if (ferror(nin)) 
     {
     log_event(
       PBSEVENT_ADMIN, 
       PBS_EVENTCLASS_SERVER, 
       "nodes",
       "Node description file update failed");

     fclose(nin);

     return(-1);
     }
   }

  fclose(nin);

  unlink(path_nodes);
  link(path_nodes_new, path_nodes);
  unlink(path_nodes_new);

  return(0);
  }  /* END update_nodes_file() */





/*
 *	recompute_ntype_cnts - Recomputes the current number of cluster
 *			       nodes and current number of time-shared nodes
 */
void	recompute_ntype_cnts ()
{
	int		 svr_loc_clnodes = 0;
	int		 svr_loc_tsnodes = 0;
	int		 i;
	struct	pbsnode  *pnode;

	if (svr_totnodes) {
	    for (i=0; i<svr_totnodes; ++i) {

		 pnode = pbsndlist[i];
		 if (pnode->nd_state & INUSE_DELETED)
		     continue;

		 if (pnode->nd_ntype == NTYPE_CLUSTER)
		     svr_loc_clnodes += pnode->nd_nsn;
		 else if (pnode->nd_ntype == NTYPE_TIMESHARED)
		     svr_loc_tsnodes++;
	    }
	    svr_clnodes = svr_loc_clnodes;
	    svr_tsnodes = svr_loc_tsnodes;
	}
}





/*
 * init_prop - allocate and initialize a prop struct
 *
 *	pname points to the property string
 */

struct prop *init_prop(

  char *pname) /* I */

  {
  struct prop *pp;

  if ((pp = (struct prop *)malloc(sizeof(struct prop))) != NULL) 
    {
    pp->name    = pname;
    pp->mark    = 0;
    pp->next    = 0;
    }
	
  return(pp);
  }  /* END init_prop() */
	



/*
 * create_subnode - create a subnode entry and link to parent node
 *
 *	Note, pname arg must be a copy of prop list as it is linked directly in
 */

static struct pbssubn *create_subnode(

  struct pbsnode *pnode)

  {
  struct pbssubn  *psubn;
  struct pbssubn **nxtsn;
	
  psubn = (struct pbssubn *)malloc(sizeof(struct pbssubn));

  if (psubn == (struct pbssubn *)0) 
    {
    return(NULL);
    }

	/* initialize the subnode and link into the parent node */

	psubn->host  = pnode;
	psubn->next  = (struct pbssubn *)NULL;
	psubn->jobs  = (struct jobinfo *)NULL;
	psubn->flag  = okay;
	psubn->inuse = 0;
	psubn->index = pnode->nd_nsn++;
	pnode->nd_nsnfree++;
	if ((pnode->nd_state & (INUSE_JOB|INUSE_JOBSHARE)) != 0)
		pnode->nd_state = INUSE_FREE;
	psubn->allocto = (resource_t)0;

	nxtsn = &pnode->nd_psn;	   /* link subnode onto parent node's list */
	while (*nxtsn)
		nxtsn = &((*nxtsn)->next);
	*nxtsn = psubn;

	return (psubn);
  }  /* END create_subnode() */





/*
 * create_pbs_node - create pbs node structure, i.e. add a node
 */

int create_pbs_node(

  char     *objname,
  svrattrl *plist,
  int       perms,
  int      *bad)

  {
  struct pbsnode  *pnode = NULL;
  struct pbsnode **tmpndlist;
  int              ntype;	/*node type; time-shared, not */
  char            *pname;	/*node name w/o any :ts       */
  u_long          *pul;		/*0 terminated host adrs array*/
  int              rc;
  int              iht;

  if ((rc = process_host_name_part(objname,&pul,&pname,&ntype)) != 0) 
    {
    log_err(-1, "process_host_name_part", log_buffer);

    return(rc);
    }

  if (find_nodebyname(pname)) 
    {
    free(pname);
    free(pul);

    return(PBSE_NODEEXIST);
    }

  for (iht=0; iht<svr_totnodes; iht++) 
    {
    if (pbsndmast[iht]->nd_state & INUSE_DELETED) 
      { 
      /*available, use*/

      pnode = pbsndmast[iht];
 
      break;
      }
    }

  if (iht == svr_totnodes) 
    {
    /*no unused entry, make an entry*/

    pnode = (struct pbsnode *)calloc(1,sizeof(struct pbsnode));

    if (pnode == NULL) 
      {
      free(pul);
      free(pname);

      return(PBSE_SYSTEM);
      }

    pnode->nd_state = INUSE_DELETED;

    /* expand pbsndmast array exactly svr_totnodes long*/

    tmpndlist = (struct pbsnode **)realloc(
      pbsndmast,
      sizeof(struct pbsnode *) * (svr_totnodes + 1));

    if (tmpndlist == NULL) 
      {
      free(pnode);
      free(pul);
      free(pname);

      return(PBSE_SYSTEM);
      }

    /*add in the new entry etc*/

    pbsndmast = tmpndlist;
    pbsndmast[svr_totnodes++] = pnode;

    tmpndlist = (struct pbsnode **)realloc(
      pbsndlist, 
      sizeof(struct pbsnode *) * (svr_totnodes + 1));

    if (tmpndlist == NULL) 
      {
      free(pnode);
      free(pul);
      free(pname);

      return(PBSE_SYSTEM);
      }

    memcpy(
      tmpndlist, 
      pbsndmast, 
      svr_totnodes * sizeof(struct pbsnode *));

    pbsndlist = tmpndlist;
    }

  initialize_pbsnode(pnode,pname,pul,ntype);

  /* create and initialize the first subnode to go with the parent node */

  if (create_subnode(pnode) == NULL) 
    {
    pnode->nd_state = INUSE_DELETED;

    free(pul);
    free (pname);

    return (PBSE_SYSTEM);
    }

  rc = mgr_set_node_attr(
	  pnode,
	  node_attr_def,
	  ND_ATR_LAST,
          plist,
	  perms,
	  bad,
	  (void *)pnode,
          ATR_ACTION_ALTER);

  if (rc != 0) 
    {
    effective_node_delete(pnode);

    return(rc);
    }

  recompute_ntype_cnts();

  return(PBSE_NONE);	    /*create completely successful*/
  }




/*
 * parse_node_token - parse tokens in the nodes file
 *
 *	Token is returned, if null then there was none.
 *	If there is an error, then "err" is set non-zero.
 *	On following call, with argument "start" as null pointer, then 
 *	resume where left off.
 *
 *	If "cok" is true, then this is first token (node name) and ':' is
 *	allowed and '=' is not.   For following tokens, allow '=' as separator
 *	between "keyword" and "value".  Will get value as next token.
 */

static char *parse_node_token(

  char *start,	/* if null, restart where left off */
  int   cok,	/* flag - non-zero if colon ":" allowed in token */
  int  *err,	/* RETURN: non-zero if error */
  char *term)	/* RETURN: character terminating token */

  {
  static char *pt;
  char        *ts;

  *err = 0;

  if (start)
    pt = start;

  while (*pt && isspace((int)*pt))	/* skip leading whitespace */
    pt++;

  if (*pt == '\0')
    {
    return (NULL);		/* no token */
    }

  ts = pt;
	
  /* test for legal characters in token */

  for (;pt[0] != '\0';pt++) 
    {
    if (isalnum((int)*pt) || strchr("-._",*pt) || (*pt == '\0'))
      continue;

    if (isspace((int)*pt))
      break;

    if (cok && (*pt == ':'))
      continue;

    if (!cok && (*pt == '='))
      break;

    *err = 1;
    }  /* END for() */

  *term = *pt;
  *pt++ = '\0';

  return(ts);
  }  /* END parse_node_token() */
			




/*
**	Read the file, "nodes", containing the list of properties for each node.
**	The list of nodes is formed with pbsndmast (also pbsndlist) as the head.
**	Return -1 on error, 0 otherwise.
**
**	Read the node state file, "node_state", for any "offline"
**	conditions which should be set in the nodes. 
**
**	If routine returns -1, then "log_buffer" contains a message to
**	be logged.
*/

int setup_nodes(void)

  {
  static char id[] = "setup_nodes";

  FILE	 *nin;
  char	  line[256];
  char	 *nodename;
  char	  propstr[256];
  char	 *token;
  int	  bad, i, num, linenum;
  int	  err;
  struct pbsnode *np;
  char     *val;
  char      xchar;
  svrattrl *pal;
  int	  perm = ATR_DFLAG_MGRD|ATR_DFLAG_MGWR;
  list_head atrlist;

  extern char server_name[];
  extern resource_t next_resource_tag;

  sprintf(log_buffer,"%s()\n",
    id);

  log_record(
    PBSEVENT_SCHED,
    PBS_EVENTCLASS_REQUEST,
    id,
    log_buffer);

  CLEAR_HEAD(atrlist);

  if ((nin = fopen(path_nodes,"r")) == NULL) 
    {
    sprintf(log_buffer,"cannot open node description file '%s' in setup_nodes()\n",
      path_nodes);

    log_event(
      PBSEVENT_ADMIN, 
      PBS_EVENTCLASS_SERVER, 
      server_name,
      log_buffer);

    return(0);
    }

  next_resource_tag = time(0);	/* initialize next resource handle */

  tfree(&streams);
  tfree(&ipaddrs);

  svr_totnodes = 0;

  for (linenum = 1;fgets(line,sizeof(line),nin);linenum++) 
    {
    if (line[0] == '#')	/* comment */
      continue;

    /* first token is the node name, may have ":ts" appended */

    propstr[0] = '\0';

    token = parse_node_token(line,1,&err,&xchar);

    if (token == NULL)
      continue;	/* blank line */

    if (err != 0) 
      {
      sprintf(log_buffer,"invalid character in token \"%s\" on line %d",
        token, 
        linenum);

      goto errtoken2;
      }

    if (!isalpha((int)*token)) 
      {
      sprintf(log_buffer,"token \"%s\" doesn't start with alpha on line %d",
        token, 
        linenum);

      goto errtoken2;
      }

    nodename = token;

    /* now process remaining tokens (if any), they may be either */
    /* attributes (keyword=value) or old style properties        */

    while (1) 
      {
      token = parse_node_token(NULL,0,&err,&xchar);

      if (err != 0)
        goto errtoken1;

      if (token == NULL)
        break;

      if (xchar == '=') 
        {
        /* have new style attribute, keyword=value */

        val = parse_node_token(NULL,0,&err,&xchar);

        if ((val == NULL) || (err != 0) || (xchar == '='))
          goto errtoken1;

        pal = attrlist_create(token,0,strlen(val) + 1);

        if (pal == NULL) 
          {
          strcpy(log_buffer,"cannot create node attribute");

          log_record(
            PBSEVENT_SCHED,
            PBS_EVENTCLASS_REQUEST,
            id,
            log_buffer);

          goto errtoken2;
          }

        strcpy(pal->al_value,val);
 
        pal->al_flags = SET;

        append_link(&atrlist,&pal->al_link,pal);
        }
      else 
        {
        /* old style properity */

        if (propstr[0] != '\0')
          strcat(propstr,",");

        strcat(propstr,token);
        }
      }    /* END while(1) */

    /* if any properties, create property attr and add to list */

    if (propstr[0] != '\0') 
      {
      pal = attrlist_create(ATTR_NODE_properties,0,strlen(propstr) + 1);

      if (pal == NULL) 
        {
        strcpy(log_buffer,"cannot create node attribute");

        log_record(
          PBSEVENT_SCHED,
          PBS_EVENTCLASS_REQUEST,
          id,
          log_buffer);

        /* FAILURE */

        return(-1);
        }

      strcpy(pal->al_value, propstr);
  
      pal->al_flags = SET;

      append_link(&atrlist,&pal->al_link,pal);
      }

    /* now create node and subnodes */

    pal = GET_NEXT(atrlist);

    err = create_pbs_node(nodename,pal,perm,&bad);

    if (err == PBSE_NODEEXIST) 
      {
      sprintf(log_buffer,"duplicate node \"%s\"on line %d",
        nodename, 
        linenum);

      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);

      goto errtoken2;
      } 

    if (err != 0) 
      {
      sprintf(log_buffer,"could not create node \"%s\", error = %d", 
        nodename, 
        err);

      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);

      goto errtoken2;
      }

    if (LOGLEVEL >= 3)
      {
      sprintf(log_buffer,"node '%s' successfully loaded from nodes file",
        nodename);
                                                                                               
      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);
      }

    free_attrlist(&atrlist);
    }  /* END for (linenum) */

  fclose(nin);

  nin = fopen(path_nodestate,"r");

  if (nin != NULL) 
    {
    while (fscanf(nin,"%s %d", 
        line, 
        &num) == 2) 
      {
      for (i = 0;i < svr_totnodes;i++) 
        {
        np = pbsndmast[i];

        if (strcmp(np->nd_name,line) == 0) 
          {
          np->nd_state = num;

          break;
          }
        }
      }	

    fclose(nin);
    }

  /* SUCCESS */

  return(0);

errtoken1:

  sprintf(log_buffer,"token \"%s\" in error on line %d of file nodes",
    token, 
    linenum);

  log_record(
    PBSEVENT_SCHED,
    PBS_EVENTCLASS_REQUEST,
    id,
    log_buffer);

errtoken2:
  
  free_attrlist(&atrlist);

  fclose(nin);

  /* FAILURE */

  return(-1);
  }  /* END setup_nodes() */





/*
 * delete_a_subnode - mark a (last) single subnode entry as deleted
 */

static void delete_a_subnode(

  struct pbsnode *pnode)

  {
  struct pbssubn *psubn;
  struct pbssubn *pprior = NULL;

  psubn = pnode->nd_psn;

  while (psubn->next) 
    {
    pprior = psubn;
    psubn = psubn->next;
    }
	
  /*
   * found last subnode in list for given node, mark it deleted
   * note, have to update nd_nsnfree using pnode rather than psubn->host
   * because it point to the real node rather than the the copy (pnode)
   * and the real node is overwritten by the copy
   */
	
  if ((psubn->inuse & (INUSE_JOB|INUSE_JOBSHARE)) == 0)
    pnode->nd_nsnfree--;

  subnode_delete(psubn);

  if (pprior != NULL)
    pprior->next = NULL;

  return;
  }  /* END delete_a_subnode() */




  

/*
 * node_np_action - action routine for node's np attribute
 */

int	node_np_action( new, pobj, actmode)
	attribute	*new;		/*derive props into this attribute*/
	void		*pobj;		/*pointer to a pbsnode struct     */
	int		 actmode;	/*action mode; "NEW" or "ALTER"   */
{
	struct pbsnode *pnode = (struct pbsnode *)pobj;
	short		old_np;
	short		new_np;

	switch( actmode ) {
 
	    case ATR_ACTION_NEW:        
		new->at_val.at_long = pnode->nd_nsn;
		break;

	    case ATR_ACTION_ALTER:
		old_np = pnode->nd_nsn;
		new_np = (short)new->at_val.at_long;
		if (new_np <= 0)
			return PBSE_BADATVAL;

		while (new_np != old_np) {

		    if (new_np < old_np) {
			delete_a_subnode(pnode); 
			old_np--;
		    } else {
			create_subnode(pnode);
			old_np++;
		    }
		}
		pnode->nd_nsn = old_np;
		break;
	}

	return 0;
}
