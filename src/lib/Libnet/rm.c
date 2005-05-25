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

#if !defined(_BSD) && defined(_AIX)   /* this is needed by AIX */
#define	_BSD	1
#endif

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<string.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/param.h>
#include	<sys/time.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>

#include	"pbs_ifl.h"
#include	"net_connect.h"
#include	"resmon.h"
#include	"log.h"
#include	"dis.h"
#include	"dis_init.h"
#include	"rm.h"
#if	RPP
#include	"rpp.h"
#endif


static	int	full = 1;

/*
**	This is the structure used to keep track of the resource
**	monitor connections.  Each entry is linked into as list
**	pointed to by "outs".  If len is -1, no
**	request is active.  If len is -2, a request has been
**	sent and is waiting to be read.  If len is > 0, the number
**	indicates how much data is waiting to be sent.
*/
struct	out {
	int	stream;
	int	len;
	struct	out	*next;
};

#define	HASHOUT	32
static	struct	out	*outs[HASHOUT];

/*
**	Create an "out" structure and put it in the hash table.
*/

static int addrm(

  int stream)  /* I */

  {
  struct out *op, **head;

	if ((op = (struct out *)malloc(sizeof(struct out))) == NULL) {
		pbs_errno = errno;
		return -1;
	}

	head = &outs[stream % HASHOUT];
	op->stream = stream;
	op->len = -1;
	op->next = *head;
	*head = op;
	return 0;
}




#if	RPP
static
void
funcs_dis()		/* The equivalent of DIS_tcp_funcs() */
{
	if (dis_getc != rpp_getc) {
		dis_getc = (int (*)(int))rpp_getc;
		dis_puts = (int (*)(int, const char *, size_t))rpp_write;
		dis_gets = (int (*)(int, char *, size_t))rpp_read;
		disr_commit = (int (*)(int, int))rpp_rcommit;
		disw_commit = (int (*)(int, int))rpp_wcommit;
	}
}

#define	setup_dis(x)	funcs_dis()	/* RPP doesn't need reset */
#define	close_dis(x)	rpp_close(x)
#define	flush_dis(x)	rpp_flush(x)

#else

#define	funcs_dis()	DIS_tcp_funcs()
#define	setup_dis(x)	DIS_tcp_setup(x)
#define	close_dis(x)	close(x)
#define	flush_dis(x)	DIS_tcp_wflush(x)

#endif




/*
**	Connects to a resource monitor and returns a file descriptor to
**	talk to it.  If port is zero, use default port.
*/

int openrm(

  char         *host,  /* I */
  unsigned int  port)  /* I (optional,0=DEFAULT) */

  {
  int    stream;
  static int		first = 1;
  static unsigned int	gotport = 0;

  pbs_errno = 0;

  if (port == 0) 
    {
    if (gotport == 0) 
      {

      gotport = get_svrport(PBS_MANAGER_SERVICE_NAME,"tcp",
	   	PBS_MANAGER_SERVICE_PORT);

      }  /* END if (gotport == 0) */

    port = gotport;
    }

#if RPP

  if (first) 
    {
    int tryport = IPPORT_RESERVED;

    first = 0;

    while (--tryport > 0) 
      {
      if (rpp_bind(tryport) != -1)
        break;

      if ((errno != EADDRINUSE) && (errno != EADDRNOTAVAIL))
        break;
      }
    }

  stream = rpp_open(host,port,NULL);

#else /* RPP */

  if ((stream = socket(AF_INET,SOCK_STREAM,0)) != -1) 
    {
    int	tryport = IPPORT_RESERVED;
    struct	sockaddr_in	addr;
    struct	hostent		*hp;

    if ((hp = gethostbyname(host)) == NULL) 
      {
      DBPRT(("host %s not found\n", 
        host))

      pbs_errno = ENOENT;

      return(-1);
      }

    memset(&addr,'\0',sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    while (--tryport > 0) 
      {
      addr.sin_port = htons((u_short)tryport);

      if (bind(stream,(struct sockaddr *)&addr,sizeof(addr)) != -1)
        break;

      if ((errno != EADDRINUSE) && (errno != EADDRNOTAVAIL))
        break;
      }

    memset(&addr,'\0',sizeof(addr));

    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons((unsigned short)port);

    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

    if (connect(stream, (struct sockaddr *)&addr,sizeof(addr)) == -1) 
      {
      pbs_errno = errno;

      close(stream);

      return(-1);
      }
    }    /* END if ((stream = socket(AF_INET,SOCK_STREAM,0)) != -1) */

#endif /* RPP */

  pbs_errno = errno;

  if (stream < 0)
    {
    return(-1);
    }

  if (addrm(stream) == -1) 
    {
    pbs_errno = errno;

    close_dis(stream);

    return(-1);
    }

  return(stream);
  }  /* END openrm() */





/*
**	Routine to close a connection to a resource monitor
**	and free the "out" structure.
**	Return 0 if all is well, -1 on error.
*/
static
int
delrm(stream)
     int	stream;
{
	struct	out	*op, *prev = NULL;

	for (op=outs[stream % HASHOUT]; op; op=op->next) {
		if (op->stream == stream)
			break;
		prev = op;
	}
	if (op) {
		close_dis(stream);

		if (prev)
			prev->next = op->next;
		else
			outs[stream % HASHOUT] = op->next;
		free(op);
		return 0;
	}
	return -1;
}




/*
**	Internal routine to find the out structure for a stream number.
**	Return non NULL if all is well, NULL on error.
*/

static struct out *findout(

  int stream)

  {
  struct out *op;

  for (op = outs[stream % HASHOUT];op;op=op->next) 
    {
    if (op->stream == stream)
      break;
    }

  if (op == NULL)
    pbs_errno = ENOTTY;

  return(op);
  }




static int startcom(

  int stream,  /* I */
  int com)     /* I */

  {
  int ret;

  setup_dis(stream);

  ret = diswsi(stream,RM_PROTOCOL);

  if (ret == DIS_SUCCESS) 
    {
    ret = diswsi(stream,RM_PROTOCOL_VER);

    if (ret == DIS_SUCCESS)
      ret = diswsi(stream,com);
    }

  if (ret != DIS_SUCCESS) 
    {
    /* NOTE:  cannot resolve log_err */

    /* log_err(ret,"startcom - diswsi error",(char *)dis_emsg[ret]); */

    pbs_errno = errno;
    }

  return(ret);
  }  /* END startcom() */





/*
**	Internal routine to compose and send a "simple" command.
**	This means anything with a zero length body.
**	Return 0 if all is well, -1 on error.
*/

static int simplecom(

  int stream,
  int com)

  {
  struct out *op;

  if ((op = findout(stream)) == NULL)
    {
    return(-1);
    }

  op->len = -1;

  if (startcom(stream,com) != DIS_SUCCESS) 
    {
    close_dis(stream);

    return(-1);
    }

  if (flush_dis(stream) == -1) 
    {
    pbs_errno = errno;

    DBPRT(("simplecom: flush error %d\n", 
      pbs_errno))

    close_dis(stream);

    return(-1);
    }

#if	RPP
  rpp_eom(stream);
#endif

  return(0);
  }  /* END simplecom() */




/*
**	Internal routine to read the return value from a command.
**	Return 0 if all is well, -1 on error.
*/

static int simpleget(

  int stream)

  {
  int ret, num;

  num = disrsi(stream,&ret);

  if (ret != DIS_SUCCESS) 
    {
    /* NOTE:  cannot resolve log_err */

    /* log_err(ret,"simpleget",(char *)dis_emsg[ret]); */

    pbs_errno = errno ? errno : EIO;

    close_dis(stream);

    return(-1);
    }

  if (num != RM_RSP_OK) 
    {
#ifdef	ENOMSG
    pbs_errno = ENOMSG;
#else
    pbs_errno = EINVAL;
#endif

    return(-1);
    }

  return(0);
  }  /* END simpleget() */





/*
**	Close connection to resource monitor.  Return result 0 if
**	all is ok or -1 if not (set pbs_errno).
*/

int closerm(

  int stream)

  {
  pbs_errno = 0;

  simplecom(stream,RM_CMD_CLOSE);

  if (delrm(stream) == -1) 
    {
    pbs_errno = ENOTTY;

    return(-1);
    }

  return(0);
  }  /* END closerm() */





/*
**	Shutdown the resource monitor.  Return result 0 if
**	all is ok or -1 if not (set pbs_errno).
*/

int downrm(

  int stream)  /* I */

  {
  pbs_errno = 0;

  if (simplecom(stream,RM_CMD_SHUTDOWN))
    {
    return(-1);
    }

  if (simpleget(stream))
    {
    return(-1);
    }

  delrm(stream);

  return(0);
  }  /* END downrm() */




/*
**	Cause the resource monitor to read the file named.
**	Return the result 0 if all is ok or -1 if not (set pbs_errno).
*/

int configrm(

  int   stream,  /* I */
  char *file)    /* I */

  {
  int         ret, len;
  struct out *op;

  pbs_errno = 0;

  if ((op = findout(stream)) == NULL)
    {
    return(-1);
    }

  op->len = -1;

  /* NOTE:  remove absolute job path check to allow config file staging (CRI) */

  /* NOTE:  remove filename size check (was 'MAXPATHLEN') */

  if ((len = strlen(file)) > (size_t)65536)
    {
    pbs_errno = EINVAL;

    return(-1);
    }

  if (startcom(stream,RM_CMD_CONFIG) != DIS_SUCCESS)
    {
    return(-1);
    }

  ret = diswcs(stream,file,len);

  if (ret != DIS_SUCCESS) 
    {
#if defined(ECOMM)
    pbs_errno = ECOMM;
#elif defined(ENOCONNECT)
    pbs_errno = ENOCONNECT;
#else
    pbs_errno = ETXTBSY;
#endif

    DBPRT(("configrm: diswcs %s\n", 
      dis_emsg[ret]))

    return(-1);
    }

  if (flush_dis(stream) == -1) 
    {
    pbs_errno = errno;

    DBPRT(("configrm: flush error %d\n", 
      pbs_errno))

    return(-1);
    }

  if (simpleget(stream))
    {
    return(-1);
    }

  return(0);
  }  /* END configrm() */





/*
**	Begin a new message to the resource monitor if necessary.
**	Add a line to the body of an outstanding command to the resource
**	monitor.
**	Return the result 0 if all is ok or -1 if not (set pbs_errno).
*/

static int doreq(

  struct out *op,
  char       *line)

  {
  int ret;

  if (op->len == -1) 
    {
    /* start new message */

    if (startcom(op->stream,RM_CMD_REQUEST) != DIS_SUCCESS)
      {
      return(-1);
      }

    op->len = 1;
    }

  ret = diswcs(op->stream,line,strlen(line));

  if (ret != DIS_SUCCESS) 
    {
#if defined(ECOMM)
    pbs_errno = ECOMM;
#elif defined(ENOCONNECT)
    pbs_errno = ENOCONNECT;
#else
    pbs_errno = ETXTBSY;
#endif

    DBPRT(("doreq: diswcs %s\n", 
      dis_emsg[ret]))

    return(-1);
    }

  return(0);
  }  /* END doreq() */





/*
**	Add a request to a single stream.
*/

int addreq(

  int   stream,
  char *line)

  {
  struct out *op;

  pbs_errno = 0;

  if ((op = findout(stream)) == NULL)
    {
    return(-1);
    }

  funcs_dis();

  if (doreq(op,line) == -1) 
    {
    delrm(stream);

    return(-1);
    }

  return(0);
  }  /* END addreq() */





/*
**	Add a request to every stream.
**	Return the number of streams acted upon.
*/
int
allreq(line)
     char	*line;
{
	struct	out	*op, *prev;
	int		i, num;

	funcs_dis();
	pbs_errno = 0;
	num = 0;
	for (i=0; i<HASHOUT; i++) {
		prev=NULL;
		op=outs[i];
		while (op) {
			if (doreq(op, line) == -1) {
				struct	out	*hold = op;

				close_dis(op->stream);
				if (prev)
					prev->next = op->next;
				else
					outs[i] = op->next;
				op = op->next;
				free(hold);
			}
			else {
				prev = op;
				op = op->next;
				num++;
			}
		}
	}
	return num;
}





/*
**	Finish (and send) any outstanding message to the resource monitor.
**	Return a pointer to the next response line or a NULL if
**	there are no more or an error occured.  Set pbs_errno on error.
*/

char *getreq(

  int stream)  /* I */

  {
  char	*startline;
  struct	out	*op;
  int	ret;

  pbs_errno = 0;

  if ((op = findout(stream)) == NULL)
    {
    return(NULL);
    }

  if (op->len >= 0) 
    {	
    /* there is a message to send */

    if (flush_dis(stream) == -1) 
      {
      pbs_errno = errno;

      DBPRT(("getreq: flush error %d\n", 
        pbs_errno))

      delrm(stream);

      return(NULL);
      }

    op->len = -2;

#if RPP
    rpp_eom(stream);
#endif
    }

  funcs_dis();

  if (op->len == -2) 
    {
    if (simpleget(stream) == -1)
      {
      return(NULL);
      }

    op->len = -1;
    }

  startline = disrst(stream,&ret);

  if (ret == DIS_EOF) 
    {
    return(NULL);
    }

  if (ret != DIS_SUCCESS) 
    {
    pbs_errno = errno ? errno : EIO;

    DBPRT(("getreq: cannot read string %s\n", 
      dis_emsg[ret]))

    return(NULL);
    }

  if (!full) 
    {
    char *cc, *hold;
    int   indent = 0;

    for (cc = startline;*cc;cc++) 
      {
      if (*cc == '[')
        indent++;
      else if (*cc == ']')
        indent--;
      else if ((*cc == '=') && (indent == 0)) 
        {
        hold = strdup(cc + 1);
  
        free(startline);

        startline = hold;

        break;
        }
      }
    }    /* END if (!full) */

  return(startline);
  }  /* END getreq() */





/*
**	Finish and send any outstanding messages to all resource monitors.
**	Return the number of messages flushed.
*/

int flushreq()

  {
  struct	out	*op, *prev;
  int	did, i;

  pbs_errno = 0;

  did = 0;

  for (i = 0;i < HASHOUT;i++) 
    {
    for (op = outs[i];op != NULL;op = op->next) 
      {
      if (op->len <= 0)	/* no message to send */
        continue;

      if (flush_dis(op->stream) == -1) 
        {
        pbs_errno = errno;

        DBPRT(("flushreq: flush error %d\n", 
          pbs_errno))

        close_dis(op->stream);

        op->stream = -1;

        continue;
        }

      op->len = -2;

#if RPP

      rpp_eom(op->stream);

#endif /* RPP */

      did++;
      }  /* END for (op) */

    prev = NULL;

    op = outs[i];

    while (op != NULL) 
      {		/* get rid of bad streams */
      if (op->stream != -1) 
        {
        prev = op;

        op = op->next;

        continue;
        }

      if (prev == NULL) 
        {
        outs[i] = op->next;

        free(op);

        op = outs[i];
        }
      else 
        {
        prev->next = op->next;

        free(op);

        op = prev->next;
        }
      }
    }

  return(did);
  }  /* END flushreq() */





/*
**	Return the stream number of the next stream with something
**	to read or a negative number (the return from rpp_poll)
**	if there is no stream to read.
*/

int activereq(void)

  {
	static	char	id[] = "activereq";
	struct	out	*op;
	int		try, i, num;
	int		bucket;
	struct	timeval	tv;
	fd_set		fdset;

	pbs_errno = 0;
	flushreq();
	FD_ZERO(&fdset);

#if	RPP
	for (try=0; try<3;) {
		if ((i = rpp_poll()) >= 0) {
			if ((op = findout(i)) != NULL)
				return i;

			op = (struct out *)malloc(sizeof(struct out));
			if (op == NULL) {
				pbs_errno = errno;
				return -1;
			}

			bucket = i % HASHOUT;
			op->stream = i;
			op->len = -2;
			op->next = outs[bucket];
			outs[bucket] = op;
		}
		else if (i == -1) {
			pbs_errno = errno;
			return -1;
		}
		else {
			extern	int	rpp_fd;

			FD_SET(rpp_fd, &fdset);
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			num = select(FD_SETSIZE, &fdset, NULL, NULL, &tv);
			if (num == -1) {
				pbs_errno = errno;
				DBPRT(("%s: select %d\n", id, pbs_errno))
				return -1;
			}
			if (num == 0) {
				try++;
				DBPRT(("%s: timeout %d\n", id, try))
			}
		}
	}
	return i;
#else
	pbs_errno = 0;
	for (i=0; i<HASHOUT; i++) {
		struct	out	*op;

		op=outs[i];
		while (op) {
			FD_SET(op->stream, &fdset);
			op = op->next;
		}
	}
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	num = select(FD_SETSIZE, &fdset, NULL, NULL, &tv);
	if (num == -1) {
		pbs_errno = errno;
		DBPRT(("%s: select %d\n", id, pbs_errno))
		return -1;
	}
	else if (num == 0)
		return -2;

	for (i=0; i<HASHOUT; i++) {
		struct	out	*op;

		op=outs[i];
		while (op) {
			if (FD_ISSET(op->stream, &fdset))
				return op->stream;
			op = op->next;
		}
	}
	return -2;
#endif
}

/*
**	If flag is true, turn on "full response" mode where getreq
**	returns a pointer to the beginning of a line of response.
**	This makes it possible to examine the entire line rather
**	than just the answer following the equal sign.
*/
void
fullresp(flag)
     int	flag;
{
#if	RPP
	extern	int	rpp_dbprt;

	if (flag)
		rpp_dbprt = 1 - rpp_dbprt;	/* toggle RPP debug */
#endif
	pbs_errno = 0;
	full = flag;
	return;
}
