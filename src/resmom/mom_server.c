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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/time.h>

#if defined(__TLINUX)
#include <sys/vfs.h>
#elif defined(__TDARWIN)
#include <sys/param.h> 
#include <sys/mount.h>
#endif /* __TLINUX */

#include "pbs_ifl.h"
#include "pbs_error.h"
#include "log.h"
#include "net_connect.h"
#include "rpp.h"
#include "dis.h"
#include "dis_init.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_nodes.h"
#include "resmon.h"


/* Global Data Items */

extern	unsigned int	default_server_port;
extern	char		mom_host[];
extern	char		*path_jobs;
extern	char		*path_home;
extern  char            *path_spool;
extern	int		pbs_errno;
extern	unsigned int	pbs_mom_port;
extern	unsigned int	pbs_rm_port;
extern	unsigned int	pbs_tm_port;
extern	time_t		time_now;
extern	int		internal_state;
extern  int             LOGLEVEL;
extern  char            PBSNodeCheckPath[1024];
extern  int             PBSNodeCheckInterval;
extern  char            PBSNodeMsgBuf[1024];
extern  int             MOMRecvHelloCount;
extern  int             MOMRecvClusterAddrsCount;
extern  time_t          LastServerUpdateTime;
extern  int             ServerStatUpdateInterval;

int			server_stream = -1;	/* XXX */

void state_to_server A_((int));

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */
/*LINTLIBRARY*/

/*
**	Modified by Tom Proett <proett@nas.nasa.gov> for PBS.
*/

typedef struct node_t {
  u_long         key;
  struct node_t	*left, *right;
  } node;

node *okclients = NULL;	/* tree of ip addrs */





/* list keys in tree */

int tlist(

  node *rootp,   /* I */
  char *Buf,     /* O (modified) */
  int   BufSize) /* I */

  {
  char tmpLine[32];

  int  BSize;

  /* NOTE:  recursive.  Buf not initialized */

  if ((rootp == NULL) || (Buf == NULL))
    {
    /* empty tree - failure */

    return(1);
    }

  if (BufSize <= 16)
    {
    /* inadequate space to append data */

    return(-1);
    }

  BSize = BufSize;

  if (rootp->left != NULL)
    {
    tlist(rootp->left,Buf,BSize);

    BSize -= strlen(Buf);
    }

  if (rootp->right != NULL)
    {
    tlist(rootp->right,Buf,BSize);

    BSize -= strlen(Buf);
    }

  if (BSize <= 16)
    {
    /* inadequate space to append data */

    return(-1);
    }

  sprintf(tmpLine,"%ld.%ld.%ld.%ld",
    (rootp->key & 0xff000000) >> 24,
    (rootp->key & 0x00ff0000) >> 16,
    (rootp->key & 0x0000ff00) >> 8,
    (rootp->key & 0x000000ff));

  if ((Buf[0] != '\0') && (BSize > 1))
    {
    strcat(Buf,",");

    BSize--;
    }

  if (BSize > strlen(tmpLine))
    {
    strcat(Buf,tmpLine);
    }

  return(-1);
  }  /* END tlist() */




/* find value in tree, return 1 if found, 0 if not */

int tfind(

  const u_long   key,	/* key to be located */
  node         **rootp)	/* address of tree root */

  {

  if (rootp == NULL)
    {
    /* empty tree - failure */

    return(0);
    }

  while (*rootp != NULL) 
    {	
    /* Knuth's T1: */

    if (key == (*rootp)->key)	
      {
      /* T2: */

      /* we found it! */

      return(1);
      }

    rootp = (key < (*rootp)->key) ?
      &(*rootp)->left :	/* T3: follow left branch */
      &(*rootp)->right;	/* T4: follow right branch */
    }  /* END while (*rootp != NULL) */

  /* cannot locate value in tree - failure */

  return(0);
  }  /* END tfind() */





/* NOTE:  tinsert cannot report failure */

void tinsert(

  const u_long   key,	/* key to be located */
  node         **rootp)	/* address of tree root */

  {
  register node *q;

  if (rootp == NULL)
    {
    /* invalid tree address - failure */

    return;
    }

  while (*rootp != NULL) 
    {	
    /* Knuth's T1: */

    if (key == (*rootp)->key)	/* T2: */
      {
      /* key already exists */

      return;			/* we found it! */
      }

    rootp = (key < (*rootp)->key) ?
      &(*rootp)->left :	/* T3: follow left branch */
      &(*rootp)->right;	/* T4: follow right branch */
    }
  
  /* create new node */

  q = (node *)malloc(sizeof(node));	/* T5: key not found */

  if (q == NULL) 
    {
    /* cannot allocate memory - failure */

    return;
    }

  /* make new node */

  *rootp = q;			/* link new node to old */

  q->key = key;			/* initialize new node */

  q->left = NULL;
  q->right = NULL;

  /* success */

  return;
  }  /* END tinsert() */





void tfree(

  node **rootp)

  {
  if (rootp == NULL || *rootp == NULL)
    {
    return;
    }

  tfree(&(*rootp)->left);
  tfree(&(*rootp)->right);

  free(*rootp);

  *rootp = NULL;

  return;
  }  /* END tfree() */





/*
**	Start a standard inter-server message.
*/

int is_compose(

  int stream,
  int command)

  {
  int ret;

  if (stream < 0)
    {
    return(DIS_EOF);
    }

  DIS_rpp_reset();

  ret = diswsi(stream, IS_PROTOCOL);

  if (ret != DIS_SUCCESS)
    goto done;

  ret = diswsi(stream, IS_PROTOCOL_VER);

  if (ret != DIS_SUCCESS)
    goto done;

  ret = diswsi(stream, command);

  if (ret != DIS_SUCCESS)
    goto done;

  return(DIS_SUCCESS);

done:

  DBPRT(("is_compose: send error %s\n", 
    dis_emsg[ret]))

  return(ret);
  }  /* END is_compose() */





/*
**	Input is coming from another server over a DIS rpp stream.
**	Read the stream to get a Inter-Server request.
*/

void is_request(

  int  stream,   /* I */
  int  version,  /* I */
  int *cmdp)     /* O (optional) */

  {
  static char id[] = "is_request";

  int		command = 0;
  int		ret = DIS_SUCCESS;
  u_long	ipaddr;
  short		port;
  struct	sockaddr_in *addr = NULL;
  void		init_addrs();

  time_t        time_now;

  if (cmdp != NULL)
    *cmdp = 0;

  if (LOGLEVEL >= 4)
    {
    sprintf(log_buffer,"stream %d version %d\n",
      stream,
      version);

    log_record(
      PBSEVENT_ERROR,
      PBS_EVENTCLASS_JOB,
      id,
      log_buffer);
    }

  if (version != IS_PROTOCOL_VER) 
    {
    sprintf(log_buffer,"protocol version %d unknown", 
      version);

    log_err(-1,id,log_buffer);

    rpp_close(stream);

    return;
    }

  /* check that machine is okay to be a server */
  /* If the stream is the server_stream we already opened, then it's fine  */

  addr = rpp_getaddr(stream);

  if (stream != server_stream)
    {
    port = ntohs((unsigned short)addr->sin_port);

    ipaddr = ntohl(addr->sin_addr.s_addr);

    if ((port >= IPPORT_RESERVED) || !tfind(ipaddr,&okclients)) 
      {
      char tmpLine[1024];

      tmpLine[0] = '\0';

      tlist(okclients,tmpLine,1024);

      sprintf(log_buffer,"bad connect from %s - unauthorized (okclients: %s)",
        netaddr(addr),
        tmpLine);

      log_err(-1,id,log_buffer);

      rpp_close(stream);

      return;
      }
    }    /* END if (stream != server_stream) */

  command = disrsi(stream,&ret);

  if (cmdp != NULL)
    *cmdp = command;

  if (ret != DIS_SUCCESS)
    goto err;

  if (LOGLEVEL >= 4)
    {
    sprintf(log_buffer,"command %d received",
      command);

    log_record(
      PBSEVENT_ERROR,
      PBS_EVENTCLASS_JOB,
      id,
      log_buffer);
    }

  switch (command) 
    {
    case IS_NULL: /* a ping from the server */

      if (LOGLEVEL >= 4)
        {
        DBPRT(("%s: IS_NULL\n",
          id))
        }

      if (internal_state & INUSE_DOWN) 
        {
        state_to_server(1);
        }

      break;

    case IS_HELLO:		/* server wants a return ping */

      DBPRT(("%s: IS_HELLO, state=0x%x\n", 
        id, 
        internal_state))

      server_stream = stream;		/* save stream to server XXX */

      is_compose(stream,IS_HELLO);

      rpp_flush(stream);

      MOMRecvHelloCount++;

      /* FORCE immediate update server */

      time_now = time((time_t *)0);

      LastServerUpdateTime = time_now - ServerStatUpdateInterval;

      if (internal_state != 0)
        state_to_server(1);

      break;

    case IS_CLUSTER_ADDRS:

      if (LOGLEVEL >= 3)
        {
        sprintf(log_buffer,"IS_CLUSTER_ADDRS received");

        log_record(
          PBSEVENT_ERROR,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }

      for (;;) 
        {
        ipaddr = disrul(stream,&ret);

        if (ret != DIS_SUCCESS)
          break;

        tinsert(ipaddr,&okclients);

        if (LOGLEVEL >= 4)
          {
          char tmpLine[1024];

          sprintf(tmpLine,"%s:\t%ld.%ld.%ld.%ld added to okclients\n", 
            id,
            (ipaddr & 0xff000000) >> 24,
            (ipaddr & 0x00ff0000) >> 16,
            (ipaddr & 0x0000ff00) >> 8,
            (ipaddr & 0x000000ff));

          log_record(
            PBSEVENT_ERROR,
            PBS_EVENTCLASS_JOB,
            id,
            tmpLine);
          }
        }  /* END for (;;) */

      MOMRecvClusterAddrsCount++;

      /* FORCE immediate update server */

      time_now = time((time_t *)0);

      LastServerUpdateTime = time_now - ServerStatUpdateInterval;

      if (ret != DIS_EOD)
        goto err;

      break;

    default:

      sprintf(log_buffer,"unknown command %d sent", 
        command);

      log_err(-1,id,log_buffer);

      goto err;
    }  /* END switch(command) */

  rpp_eom(stream);

  return;

err:

  /* We come here if we got a DIS read error or a protocol
  ** element is missing.  */

  sprintf(log_buffer,"%s from %s", 
    dis_emsg[ret], 
    (addr != NULL) ? netaddr(addr) : "???");

  log_err(-1,id,log_buffer);

  rpp_close(stream);

  return;
  }  /* END is_request() */




/*
 * check_busy() - 
 *	If current load average ge max_load_val and busy not already set
 *		set it
 *	If current load average lt ideal_load_val and busy currently set
 *		unset it
 */

void check_busy(

  double mla)

  {
  extern int   internal_state;
  extern float ideal_load_val;
  extern float max_load_val;

  if ((mla >= max_load_val) && 
     ((internal_state & INUSE_BUSY) == 0))
    {
    internal_state |= (INUSE_BUSY | UPDATE_MOM_STATE);
    }
  else if ((mla < ideal_load_val) && 
          ((internal_state & INUSE_BUSY) != 0))
    {
    internal_state = (internal_state & ~INUSE_BUSY) | UPDATE_MOM_STATE;
    }

  return;
  }  /* END check_busy() */





int MUReadPipe(

  char *Command,  /* I */
  char *Buffer,   /* O */
  int   BufSize)  /* I */

  {
  FILE *fp;
  int   rc;

  int   rcount;
  int   ccount;

  if ((Command == NULL) || (Buffer == NULL))
    {
    return(1);
    }

  if ((fp = popen(Command,"r")) == NULL)
    {
    return(1);
    }

  ccount = 0;
  rcount = 0;

  do 
    {
    rc = fread(Buffer + ccount,1,BufSize - ccount,fp);

    ccount += rc;

    if (ferror(fp))
      {
      break;
      }

    if ((ccount >= BufSize) || (rcount++ > 10))
      {
      /* full buffer loaded or too many attempts */

      break;
      }
    } while (!feof(fp));

  if (ferror(fp))
    {
    /* FAILURE */

    pclose(fp);

    return(1);
    }

  /* SUCCESS - terminate buffer */

  Buffer[MIN(BufSize - 1,ccount)] = '\0';

  pclose(fp);

  return(0);
  }  /* END MUReadPipe() */




/*
 * check_state() -
 *   if down criteria satisfied and node is up, mark node down 
 *   if down criteria is not set and node is down, mark it up
 */

void check_state()

  {
  static int ICount = 0;

  static char tmpPBSNodeMsgBuf[1024];

  /* clear node messages */

  PBSNodeMsgBuf[0] = '\0';

  /* conditions:  external state should be down if
     - inadequate file handles available (for period X) 
     - external health check fails
  */

  /* verify adequate space in spool directory */

#define TMINSPOOLBLOCKS 100  /* blocks available in spool directory required for proper operation */


#if defined(__TLINUX) || defined(__TDARWIN)
  {
  struct statfs F;

  if (statfs(path_spool,&F) == -1)
    {
    /* cannot check filesystem */
    }
  else if (F.f_bfree < TMINSPOOLBLOCKS)
    {
    /* inadequate disk space in spool directory */

    strcpy(PBSNodeMsgBuf,"no disk space");
    }
  }    /* END BLOCK */
#endif /* __TLINUX || __TDARWIN */

  if (PBSNodeCheckPath[0] != '\0')
    {
    if (ICount == 0)
      {
      if (MUReadPipe(
           PBSNodeCheckPath,
           tmpPBSNodeMsgBuf,
           sizeof(tmpPBSNodeMsgBuf)) == 0)
        {
        if (strncmp(tmpPBSNodeMsgBuf,"ERROR",strlen("ERROR")))
          {
          /* ignore non-error messages */

          tmpPBSNodeMsgBuf[0] = '\0';
          }
        }
      }    /* END if (ICount == 0) */

    if (tmpPBSNodeMsgBuf[0] != '\0')
      {
      /* update node msg buffer */

      strncpy(
        PBSNodeMsgBuf,
        tmpPBSNodeMsgBuf,
        sizeof(PBSNodeMsgBuf));

      PBSNodeMsgBuf[sizeof(PBSNodeMsgBuf) - 1] = '\0';
      }
    }      /* END if (PBSNodeCheckPath[0] != '\0') */

  ICount ++;

  ICount %= MAX(1,PBSNodeCheckInterval);

  return;
  }  /* END check_state() */





/*
 * state_to_server() - if UPDATE_MOM_STATE is set, send state message to
 *	the server.
 */

void state_to_server(

  int force)  /* I (boolean) */

  {
  char *id = "state_to_server";

  if (!force && !(internal_state & UPDATE_MOM_STATE)) 
    {
    return;
    }

  if (server_stream < 0)
    {
    return;
    }
 
  if (is_compose(server_stream,IS_UPDATE) != DIS_SUCCESS) 
    {
    return;		
    } 

  if (diswui(server_stream,internal_state & ~UPDATE_MOM_STATE) != DIS_SUCCESS) 
    {
    return;
    }

  if (rpp_flush(server_stream) == 0)
    {
    /* send successful, unset UPDATE_MOM_STATE */

    internal_state &= ~UPDATE_MOM_STATE;

    if (LOGLEVEL >= 4)
      {
      sprintf(log_buffer,"sent updated state 0x%x to server\n",
        internal_state);

      log_record(
        PBSEVENT_ERROR,
        PBS_EVENTCLASS_JOB,
        id,
        log_buffer);
      }
    }
  else
    {
    if (LOGLEVEL >= 2)
      {
      log_record(
        PBSEVENT_ERROR,
        PBS_EVENTCLASS_JOB,
        id,
        "server state update failed");
      }
    }

  return;
  }  /* END state_to_server() */

/* END mom_server.c */

