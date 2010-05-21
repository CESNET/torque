#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include "libpbs.h"
#include "dis.h"
#include "server_limits.h"
#include "net_connect.h"
#include "credential.h"

/*
 * pbs_iff - authenticates the user to the PBS server.
 *
 * Usage: call via pbs_connect() with
 *  pbs_iff [-t] hostname port [parent_connection_port]
 *
 * The parent_connection_port is required unless -t (for test) is given.
 */
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


extern int pbs_errno;

extern char pbs_current_user[PBS_MAXUSER]; /* for libpbs.a */



int main(

  int     argc,   /* I */
  char   *argv[], /* I */
  char   *envp[]) /* I */

  {
  int   auth_type = PBS_credentialtype_none;
  int   err = 0;
  struct sockaddr_storage hostaddr, sockname;
  int   i;
  uid_t   myrealuid;
  uid_t          myeuid;
  unsigned int  parentport;
  int   parentsock = -1;

  struct passwd *pwent;
  int            servport = -1;
  int    sock;

  torque_socklen_t socknamelen;

  int   testmode = 0;
  int   rc;

  struct batch_reply   *reply;
  char *parse_servername(char *, short *);
  extern int   optind;
  extern char *optarg;

  char  EMsg[1024];

  char *ptr;

  int PBSLOGLEVEL = 0;

  strcpy(pbs_current_user, "PBS_Server");

  if ((ptr = getenv("PBSLOGLEVEL")) != NULL)
    {
    PBSLOGLEVEL = (int)strtol(ptr, NULL, 10);
    }

  /* Need to unset LOCALDOMAIN if set, want local host name */

  for (i = 0;envp[i];++i)
    {
    if (!strncmp(envp[i], "LOCALDOMAIN=", 12))
      {
      envp[i] = "";

      break;
      }
    }    /* END for (i) */

  while ((i = getopt(argc, argv, "t")) != EOF)
    {
    switch (i)
      {

      case 't':

        testmode = 1;

        break;

      default:

        err = 1;

        break;
      }
    }    /* END while (i = getopt()) */

  if ((err == 1) ||
      (testmode && (argc - optind) != 2) ||
      (!testmode && (argc - optind) != 3))
    {
    fprintf(stderr, "Usage: %s [-t] host port [parent_port]\n",
            argv[0]);

    fprintf(stderr, "NOTE:  one of '-t' or 'parent_port' must be specified\n");

    return(1);
    }

  if (!testmode && isatty(fileno(stdout)))
    {
    fprintf(stderr, "pbs_iff: output is a tty & not test mode\n");

    return(1);
    }

  myeuid = geteuid();

#ifndef __CYGWIN__

  if (!testmode && (myeuid != 0))
    {
    fprintf(stderr, "pbs_iff: file not setuid root, likely misconfigured\n");

#if SYSLOG
    syslog(LOG_ERR | LOG_DAEMON, "not setuid 0, likely misconfigured");
#endif  /* SYSLOG */

    }   /* END if (!testmode && (myeuid != 0)) */

#endif  /* __CYGWIN__ */

  /* first, make sure we have a valid server (host), and ports */

  if (get_hostaddr(argv[optind], &hostaddr) != 0)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: unknown host %s\n",
            argv[optind]);

    return(1);
    }

  if ((servport = atoi(argv[++optind])) <= 0)
    {
    /* FAILURE */

    return(1);
    }

  errno = 0;

  for (i = 0;i < 10;i++)
    {
    sock = client_to_svr(&hostaddr, (unsigned int)servport, 1, EMsg);

    if (sock != PBS_NET_RC_RETRY)
      break;

    sleep(1);
    }  /* END for (i) */

  if (sock < 0)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot connect to %s:%d - %s, errno=%d (%s) %s\n",
            argv[optind - 1],
            servport,
            (sock == PBS_NET_RC_FATAL) ? "fatal error" : "timeout",
            errno,
            strerror(errno),
            EMsg);

    return(4);
    }

  connection[1].ch_inuse = 1;

  connection[1].ch_errno = 0;
  connection[1].ch_socket = sock;
  connection[1].ch_errtxt = NULL;

  DIS_tcp_setup(sock);

  if (testmode == 0)
    {
    optind++;

    if ((parentsock = atoi(argv[optind])) < 0)
      {
      /* FAILURE */

      fprintf(stderr, "pbs_iff: invalid parent socket '%s' specified\n",
              argv[optind]);

      return(1);
      }
    }
  else
    {
    /* for test mode, use my own port rather than the parents */

    parentsock = sock;
    }

  /* next, get the real user name */

  myrealuid = getuid();

  pwent = getpwuid(myrealuid);

  if (pwent == NULL)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot get account info for uid %d, errno=%d (%s)\n",
            (int)myrealuid,
            errno,
            strerror(errno));

    return(3);
    }

  /* now get the parent's client-side port */

  socknamelen = sizeof(sockname);

  if ((socknamelen = getsockname(
        parentsock,
        (struct sockaddr *)&sockname,
        &socknamelen)) == (torque_socklen_t)-1)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot get sockname for socket %d, errno=%d (%s)\n",
            parentsock,
            errno,
            strerror(errno));

    return(3);
    }

  parentport = GET_PORT(&sockname);

  /* send authentication information */

  if ((rc = encode_DIS_ReqHdr(sock, PBS_BATCH_AuthenUser, pwent->pw_name)) ||
      (rc = diswui(sock, parentport)) ||
      (rc = encode_DIS_ReqExtend(sock, NULL)))
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot send request to pbs_server, rc=%d\n",
            rc);

    return(2);
    }

  rc = DIS_tcp_wflush(sock);

  if (rc != 0)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot flush request to pbs_server, rc=%d\n",
            rc);

    return(2);
    }

  /* read back the response */

  reply = PBSD_rdrpy(1);

  if (reply == NULL)
    {
    /* FAILURE */

    fprintf(stderr, "pbs_iff: cannot read reply from pbs_server\n");

    return(1);
    }

  if (reply->brp_code != 0)
    {
    /* FAILURE */

    if (reply->brp_choice == BATCH_REPLY_CHOICE_Text)
      {
      fprintf(stderr, "pbs_iff: %s\n",
              reply->brp_un.brp_txt.brp_str);
      }
    else
      {
      fprintf(stderr, "pbs_iff: pbs_server returned failure code %d\n",
              reply->brp_code);
      }

    return(1);
    }

  /* SUCCESS */

  pbs_disconnect(1);

  /* send back "type none" credential */

  if (write(fileno(stdout), &auth_type, sizeof(int)) != sizeof(int))
    fprintf(stderr, "pbs_iff: error writing to stdout?!\n");

  fclose(stdout);

  return(0);
  }  /* END main() */


/* END iff2.c */


