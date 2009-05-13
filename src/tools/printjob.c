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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "portability.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "pbs_job.h"
#include "tm.h"

void prt_job_struct(

  job *pjob)

  {
  printf("---------------------------------------------------\n");
  printf("jobid:\t%s\n",
         pjob->ji_qs.ji_jobid);

  printf("---------------------------------------------------\n");
  printf("ji_qs version:\t%#010x\n", pjob->ji_qs.qs_version);
  printf("state:\t\t0x%x\n",
         pjob->ji_qs.ji_state);

  printf("substate:\t0x%x (%d)\n",
         pjob->ji_qs.ji_substate,
         pjob->ji_qs.ji_substate);

  printf("svrflgs:\t0x%x (%d)\n",
         pjob->ji_qs.ji_svrflags,
         pjob->ji_qs.ji_svrflags);

  printf("ordering:\t%d\n",
         pjob->ji_qs.ji_ordering);
  printf("inter prior:\t%d\n",
         pjob->ji_qs.ji_priority);
  printf("stime:\t\t%d\n",
         (int)pjob->ji_qs.ji_stime);
  printf("file base:\t%s\n",
         pjob->ji_qs.ji_fileprefix);
  printf("queue:\t\t%s\n",
         pjob->ji_qs.ji_queue);
  printf("destin:\t\t%s\n",
         pjob->ji_qs.ji_destin);

  switch (pjob->ji_qs.ji_un_type)
    {

    case JOB_UNION_TYPE_NEW:

      printf("union type new:\n");
      printf("\tsocket\t%d\n",
             pjob->ji_qs.ji_un.ji_newt.ji_fromsock);
      printf("\taddr\t%lu\n",
             pjob->ji_qs.ji_un.ji_newt.ji_fromaddr);
      printf("\tscript\t%d\n",
             pjob->ji_qs.ji_un.ji_newt.ji_scriptsz);

      break;

    case JOB_UNION_TYPE_EXEC:

      printf("union type exec:\n");
      printf("\tmomaddr\t%lu\n",
             pjob->ji_qs.ji_un.ji_exect.ji_momaddr);
      printf("\texits\t%d\n",
             pjob->ji_qs.ji_un.ji_exect.ji_exitstat);

      break;

    case JOB_UNION_TYPE_ROUTE:

      printf("union type route:\n");
      printf("\tquetime\t%d\n",
             (int)pjob->ji_qs.ji_un.ji_routet.ji_quetime);
      printf("\tretry\t%d\n",
             (int)pjob->ji_qs.ji_un.ji_routet.ji_rteretry);

      break;

    case JOB_UNION_TYPE_MOM:

      printf("union type mom:\n");
      printf("\tsvraddr\t%lu\n",
             pjob->ji_qs.ji_un.ji_momt.ji_svraddr);
      printf("\texitst\t%d\n",
             pjob->ji_qs.ji_un.ji_momt.ji_exitstat);
      printf("\tuid\t%ld\n",
             (long)pjob->ji_qs.ji_un.ji_momt.ji_exuid);
      printf("\tgid\t%ld\n",
             (long)pjob->ji_qs.ji_un.ji_momt.ji_exgid);

      break;

    default:

      printf("--bad union type %d\n",
             pjob->ji_qs.ji_un_type);

      break;
    }  /* END switch (pjob->ji_qs.ji_un_type) */

  return;
  }  /* END prt_job_struct() */




#define ENDATTRIBUTES -711

int read_attr(

  int fd)

  {
  int       amt;
  int       i;
  svrattrl *pal;
  svrattrl  tempal;

  i = read(fd, (char *) & tempal, sizeof(tempal));

  if (i != sizeof(tempal))
    {
    fprintf(stderr, "bad read of attribute\n");

    /* FAILURE */

    return(0);
    }

  if (tempal.al_tsize == ENDATTRIBUTES)
    {
    /* FAILURE */

    return(0);
    }

  pal = (svrattrl *)malloc(tempal.al_tsize);

  if (pal == NULL)
    {
    fprintf(stderr, "malloc failed\n");

    exit(1);
    }

  *pal = tempal;

  /* read in the actual attribute data */

  amt = pal->al_tsize - sizeof(svrattrl);

  i = read(fd, (char *)pal + sizeof(svrattrl), amt);

  if (i != amt)
    {
    fprintf(stderr, "short read of attribute\n");

    exit(2);
    }

  pal->al_name = (char *)pal + sizeof(svrattrl);

  if (pal->al_rescln != 0)
    pal->al_resc = pal->al_name + pal->al_nameln;
  else
    pal->al_resc = NULL;

  if (pal->al_valln != 0)
    pal->al_value = pal->al_name + pal->al_nameln + pal->al_rescln;
  else
    pal->al_value = NULL;

  printf("%s",
         pal->al_name);

  if (pal->al_resc != NULL)
    {
    printf(".%s",
           pal->al_resc);
    }

  printf(" = ");

  if (pal->al_value != NULL)
    {
    printf("%s",
           pal->al_value);
    }

  printf("\n");

  free(pal);

  return(1);
  }





int read_tm_info(

  int fds)

  {
  int outport, errport;
  tm_task_id taskid;
  tm_task_id nodeid;

  if (read(fds, (char *)&outport, sizeof(int)) != sizeof(int))
    {
    fprintf(stderr, "short read of TM output info\n");
    exit(2);
    }

  if (read(fds, (char *)&errport, sizeof(int)) != sizeof(int))
    {
    fprintf(stderr, "short read of TM error info\n");
    exit(2);
    }

  if (read(fds, (char *)&taskid, sizeof(tm_task_id)) != sizeof(tm_task_id))
    {
    fprintf(stderr, "short read of TM task info\n");
    exit(2);
    }

  if (read(fds, (char *)&nodeid, sizeof(tm_node_id)) != sizeof(tm_node_id))
    {
    fprintf(stderr, "short read of TM nodeid info\n");
    exit(2);
    }

  printf("stdout port = %d\nstderr port = %d\ntaskid = %d\nnodeid = %d\n",

         outport,
         errport,
         taskid,
         nodeid);

  return(0);
  }




int main(

  int argc,
  char *argv[])

  {
  int amt;
  int err = 0;
  int f;
  int fp;
  int no_attributes = 0;
  job xjob;

  extern int optind;

  while ((f = getopt(argc, argv, "a")) != EOF)
    {
    switch (f)
      {

      case 'a':

        no_attributes = 1;

        break;

      default:

        err = 1;

        break;
      }
    }

  if (err || (argc - optind < 1))
    {
    fprintf(stderr, "usage: %s [-a] file[ file]...}\n",
            argv[0]);

    return(1);
    }

  for (f = optind;f < argc;++f)
    {
    fp = open(argv[f], O_RDONLY, 0);

    if (fp < 0)
      {
      perror("open failed");

      fprintf(stderr, "unable to open file %s\n",
              argv[f]);

      exit(1);
      }

    amt = read(fp, &xjob.ji_qs, sizeof(xjob.ji_qs));

    if (amt != sizeof(xjob.ji_qs))
      {
      fprintf(stderr, "Short read of %d bytes, file %s\n",
              amt,
              argv[f]);
      }

    if (xjob.ji_qs.qs_version != PBS_QS_VERSION)
      {
      printf("%s contains an old version of the ji_qs structure.\n"
             "  expecting version %#010x, read %#010x\n"
             "  Skipping prt_job_struct()\n"
             "  pbs_server may be able to upgrade job automatically\n",
             argv[f], PBS_QS_VERSION, xjob.ji_qs.qs_version);
      close(fp);
      continue;
      }

    /* print out job structure */

    prt_job_struct(&xjob);

    /* now do attributes, one at a time */

    if (no_attributes == 0)
      {
      printf("--attributes--\n");

      while (read_attr(fp));
      }

    if (xjob.ji_qs.ji_un_type == JOB_UNION_TYPE_MOM)
      {
      printf("--TM info--\n");

      read_tm_info(fp);
      }

    close(fp);

    printf("\n");
    }  /* END for (f) */

  return(0);
  }    /* END main() */

/* END printjob.c */

