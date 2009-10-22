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
 * cnt2server
 *
 *	Connect to the server, and if there is an error, print a more
 * descriptive message.
 *
 * Synopsis:
 *
 *	int cnt2server(char *server)
 *
 *	server	The name of the server to connect to. A NULL or null string
 *		for the default server.
 *
 * Returns:
 *
 *	The connection returned by pbs_connect().
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "pbs_ifl.h"
#include "pbs_error.h"

#define CNTRETRYDELAY 5

/* >0 is number of seconds to retry, -1 is infinity */

static long cnt2server_retry = 0;

int cnt2server_conf(

  long retry)  /* I */

  {
  cnt2server_retry = retry;

  return(0);
  }  /* END cnt2server_conf() */





int cnt2server( char *SpecServer )  /* I (optional) */
  {
  int connect;
  time_t firsttime = 0, thistime = 0;

  char Server[1024];


  if (cnt2server_retry > 0)
    {
    firsttime = time(NULL);
    }

  memset(Server, 0, sizeof(Server));
  if (SpecServer && SpecServer[0])
    {
    strncpy(Server, SpecServer, sizeof(Server));
    Server[sizeof(Server) - 1] = '\0';
    }
  
start:

  connect = pbs_connect(Server);

  if (connect <= 0) 
    {
    if (pbs_errno > PBSE_) 
      {
      switch (pbs_errno) 
        {
        case PBSE_BADHOST:

          if ((Server == NULL) || (Server[0] == '\0'))
            {
            fprintf(stderr,"Cannot resolve default server host '%s' - check server_name file.\n",
              pbs_default());
            }
          else
            {
            fprintf(stderr,"Cannot resolve specified server host '%s'.\n",
              Server);
            }

          break;

        case PBSE_NOCONNECTS:

          if (thistime == 0)
            fprintf(stderr,"Too many open connections.\n");

          if (cnt2server_retry != 0)
            goto retry;

          break;
		
        case PBSE_NOSERVER:

          fprintf(stderr,"No default server name - check server_name file.\n");

          break;
		
        case PBSE_SYSTEM:

          if (thistime == 0)
            fprintf(stderr,"System call failure.\n");

          if (cnt2server_retry != 0)
            goto retry;

          break;
		
        case PBSE_PERM:

          if (thistime == 0)
            fprintf(stderr,"No Permission.\n");

          if (cnt2server_retry != 0)
            goto retry;

          break;
		
        case PBSE_PROTOCOL:
        default:

          if (thistime == 0)
            fprintf(stderr,"Communication failure.\n");

          if (cnt2server_retry != 0)
            goto retry;

          break;
        }
      }    /* END if (pbs_errno > PBSE_) */ 
    else 
      {
      if (thistime == 0)
        {
        if (errno == ECONNREFUSED)
          {
          if ((Server == NULL) || (Server[0] == '\0'))
            {
            char *fbserver;

            fbserver = pbs_fbserver();

            if ((fbserver != NULL) && (fbserver[0] != '\0'))
              {
              strncpy(Server,fbserver,sizeof(Server));
 
              Server[sizeof(Server) - 1] = '\0';

              if (getenv("PBSDEBUG") != NULL)
                {
                fprintf(stderr,"attempting fallback server %s\n",
                  fbserver);
                }

              goto start;
              }

            fprintf(stderr,"Cannot connect to default server host '%s' - check pbs_server daemon.\n",
              pbs_default());
            }
          else
            {
            fprintf(stderr,"Cannot connect to specified server host '%s'.\n",
              Server);
            }
          }
        else
          {
          perror(NULL);
          }
        }

      if (cnt2server_retry != 0)
        goto retry;
      }
    }    /* END if (connect <= 0) */

  return(connect);

retry:

  if (thistime == 0)
    {
    fprintf(stderr,"Retrying for %d seconds\n",
      (int)cnt2server_retry);
    }

  thistime = time(NULL);

  if (cnt2server_retry > 0) /* negative is infinite */
    {
    if ((thistime - firsttime) > cnt2server_retry)
      {
      return(connect);
      }

    if (getenv("PBSDEBUG") != NULL)
      {
      fprintf(stderr,"seconds remaining: %d\n",
        (int)(cnt2server_retry - (thistime - firsttime)));
      }
    }
  else
    {
    if (getenv("PBSDEBUG") != NULL)
      fprintf(stderr,"retrying...\n");
    }

  sleep(CNTRETRYDELAY);

  goto start;
  }  /* END cnt2server() */


/* END cnt2server.c */

