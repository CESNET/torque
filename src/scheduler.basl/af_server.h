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
#ifndef _AF_SERVER_H
#define _AF_SERVER_H

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif  /* _POSIX_SOURCE */

/* System headers */

/* Local Headers */
#include "pbs_ifl.h"
#include "af_que.h"
#include "af_cnode.h"

/* Macros */
/* Server states */
#define SERVER_ACTIVE    0 /* server running and job scheduler will be */
/* invoked */
#define SERVER_IDLE  1 /* server running but will not invoke */
/* job scheduler */
#define SERVER_SCHED     2 /* server is running and there's an */
/* outstanding request  to the job scheduler */
#define SERVER_TERM  3 /* server is terminating. */
#define SERVER_TERMDELAY 4 /* server is terminating in delayed mode */

/* The following are types of queries sent to server */
#define STATSERV 0
#define STATQUE  1
#define STATJOB  2
#define STATNODE        3

/* File Scope Variables */
/* The following are the query messages to the server. */

static  struct attrl node_alist[] =
  {
    {&node_alist[1], ATTR_NODE_state, "", "", DFLT},
  {&node_alist[2], ATTR_NODE_properties, "", "", DFLT},
  {NULL,    ATTR_NODE_ntype, "", "", DFLT}
  };

static  struct attrl serv_alist[] =
  {
    {&serv_alist[1], ATTR_status, "", "", DFLT},
  {&serv_alist[2], ATTR_dfltque, "", "", DFLT},
  {&serv_alist[3], ATTR_maxrun, "", "", DFLT},
  {&serv_alist[4], ATTR_maxuserrun, "", "", DFLT},
  {&serv_alist[5], ATTR_rescavail, "", "", DFLT},
  {&serv_alist[6], ATTR_rescassn, "", "", DFLT},
  {NULL,    ATTR_maxgrprun, "", "", DFLT}
  };

static  struct attrl que_alist[] =
  {
    {&que_alist[1], ATTR_qtype, "", "", DFLT},
  {&que_alist[2], ATTR_p, "", "", DFLT},
  {&que_alist[3], ATTR_maxrun, "", "", DFLT},
  {&que_alist[4], ATTR_maxuserrun, "", "", DFLT},
  {&que_alist[5], ATTR_maxgrprun, "", "", DFLT},
  {&que_alist[6], ATTR_rescavail, "", "", DFLT},
  {&que_alist[7], ATTR_rescassn, "", "", DFLT},
  {NULL,   ATTR_start, "", "", DFLT}
  };

static  struct attrl job_alist[] =
  {
    {&job_alist[1], ATTR_N, "", "", DFLT},
  {&job_alist[2], ATTR_owner, "", "", DFLT},
  {&job_alist[3], ATTR_euser, "", "", DFLT},
  {&job_alist[4], ATTR_egroup, "", "", DFLT},
  {&job_alist[5], ATTR_state, "", "", DFLT},
  {&job_alist[6], ATTR_p, "", "", DFLT},
  {&job_alist[7], ATTR_r, "", "", DFLT},
  {&job_alist[8], ATTR_inter, "", "", DFLT},
  {&job_alist[9], ATTR_M, "", "", DFLT},
  {&job_alist[10], ATTR_l, "", "", DFLT},
  {&job_alist[11], ATTR_used, "", "", DFLT},
  {&job_alist[12], ATTR_queue, "", "", DFLT},
  {&job_alist[13], ATTR_stagein, "", "", DFLT},
  {&job_alist[14], ATTR_stageout, "", "", DFLT},
  {NULL, ATTR_ctime, "", "", DFLT}
  };

/* MACRO */
#define SERV_INT        0
#define SERV_FLT        1
#define SERV_STR        2
#define SERV_SIZE       3
#define SERV_JSTATE     4
#define SERV_BOOL       5
#define SERV_DTIME      6

struct ServerAttrInfo
  {
  char    *name; /* server attribute name */
  char    *res;  /* server resource name (if ATTRL_l or ATTR_used) */
  int     type;     /* type the server attribute */
  void (*putfunc)();      /* the ServerPut function for attribute */
  };

/* This maps attributes whose values usually do not change over time */

static struct ServerAttrInfo jattrinfo_map[] =
  {
    {ATTR_N,        NULLSTR,        SERV_STR,       JobNamePut                },
  {ATTR_owner,    NULLSTR,        SERV_STR,       JobOwnerNamePut           },
  {ATTR_euser,    NULLSTR,        SERV_STR,       JobEffectiveUserNamePut   },
  {ATTR_egroup,   NULLSTR,        SERV_STR,       JobEffectiveGroupNamePut  },
  {ATTR_p,        NULLSTR,        SERV_INT,       JobPriorityPut            },
  {ATTR_M,        NULLSTR,        SERV_STR,       JobEmailAddrPut           },
  {ATTR_stagein,  NULLSTR,        SERV_STR,       JobStageinFilesPut        },
  {ATTR_stageout, NULLSTR,        SERV_STR,       JobStageoutFilesPut       },
  {ATTR_state,    NULLSTR,        SERV_JSTATE,    JobStatePut               },
  {ATTR_r,        NULLSTR,        SERV_BOOL,      JobRerunFlagPut    },
  {ATTR_inter,    NULLSTR,        SERV_BOOL,      JobInteractiveFlagPut   },
  {ATTR_ctime,    ",",  SERV_DTIME,     JobDateTimeCreatedPut     },
  {ATTR_used,     "nodes,arch,neednodes", \
   SERV_STR,       JobStringResUsePut        },
  {ATTR_used,     "file,mem,pmem,workingset,pf,ppf,srfs_tmp,srfs_wrk,srfs_big,srfs_fast,sds,psds",                SERV_SIZE,     JobSizeResUsePut          },
  {ATTR_used,     "cput,pcput,walltime,mppt,pmppt,nice,procs,mppe,ncpus,pncpus,nodect,srfs_assist,mta,mtb,mtc,mtd,mte,mtf,mtg,mth",
   SERV_INT,      JobIntResUsePut           },
  {ATTR_l,        "nodes,arch,neednodes", \
   SERV_STR,       JobStringResReqPut        },
  {ATTR_l,        "file,mem,pmem,workingset,pf,ppf,srfs_tmp,srfs_wrk,srfs_big,srfs_fast,sds,psds",                SERV_SIZE,     JobSizeResReqPut          },
  {ATTR_l,        "cput,pcput,walltime,mppt,pmppt,nice,procs,mppe,ncpus,pncpus,nodect,srfs_assist,mta,mtb,mtc,mtd,mte,mtf,mtg,mth",
   SERV_INT,      JobIntResReqPut           },
  {NULLSTR,       NULLSTR,        -1,             NULL    }
  };

/* put in here the list of resources_assigned.res that will be accumulated */
/* for server and queues based on the Resource_List.res value for a job */
static char *accumTable[] =
  {
  "nodect",
  "workingset",
  "mem",
  "ncpus",
  "sds",
  "procs",
  "mppe",
  NULLSTR
  };

/* External Variables */

struct server_struct
  {

  struct  server_struct *nextptr;
  char    *inetAddr;
  int     portNumberOneWay; /* scheduler <-- server */
  /* if set to 0, use PBS_SCHEDULER_SERVICE_PORT */
  int portNumberTwoWay; /* scheduler <-> server */
  /* if set to 0, use PBS_BATCH_SERVICE_PORT_DIS */
  int     socket;        /* socket file descriptor */
  int     fdOneWay;      /* fd to use when only receiving messages from */
  /* the Server */
  /* -1 if not connected */
  int     fdTwoWay;      /* fd to use when sending messages to and */
  /* receiving messages from the Server */
  /* -1 if not connected */
  int     state;
  int maxRunJobs; /* on this server */
  int maxRunJobsPerUser;
  int maxRunJobsPerGroup;
  char *defQue; /* server's default que */

  struct  IntRes *intResAvail;

  struct  IntRes *intResAssign;

  struct  SizeRes *sizeResAvail;

  struct  SizeRes *sizeResAssign;

  struct  StringRes *stringResAvail;

  struct  StringRes *stringResAssign;
  SetQue  queues;       /* queues managed by the server */
  SetJob  jobs;       /* jobs associated with server */
  SetCNode nodes;       /* nodes managed by the server */
  };

typedef struct server_struct Server;

struct SetServer_type
  {
  Server     *head;
  Server    *localhost;
  Server     *tail;
  };

typedef struct SetServer_type SetServer;

/* External Functions */
extern char *
  ServerInetAddrGet A_((Server *server));

extern char *
  ServerDefQueGet A_((Server *server));

extern int
  ServerPortNumberOneWayGet A_((Server *server));

extern int
  ServerPortNumberTwoWayGet A_((Server *server));

extern int
  ServerSocketGet A_((Server *server));

extern int
  ServerFdOneWayGet A_((Server *server));

extern int
  ServerFdTwoWayGet A_((Server *server));

extern int
  ServerStateGet A_((Server *server));

extern int
  ServerMaxRunJobsGet A_((Server *server));

extern int
  ServerMaxRunJobsPerUserGet A_((Server *server));

extern int
  ServerMaxRunJobsPerGroupGet A_((Server *server));

extern SetQue *
  ServerQueuesGet A_((Server *server));

extern struct SetJobElement *
        ServerJobsGet A_((Server *server));

extern int
  ServerIntResAvailGet A_((Server *server, char *name));

extern int
  ServerIntResAssignGet A_((Server *server, char *name));

extern Size
  ServerSizeResAvailGet A_((Server *server, char *name));

extern Size
  ServerSizeResAssignGet A_((Server *server, char *name));

extern char *
  ServerStringResAvailGet A_((Server *server, char *name));

extern char *
  ServerStringResAssignGet A_((Server *server, char *name));

/* Put functions */
extern void
  ServerInetAddrPut A_((Server *server, char *server_name));

extern void
  ServerDefQuePut A_((Server *server, char *que_name));

extern void
  ServerPortNumberOneWayPut A_((Server *server, int port));

extern void
  ServerPortNumberTwoWayPut A_((Server *server, int port));

extern void
  ServerSocketPut A_((Server *server, int fd));

extern void
  ServerFdOneWayPut A_((Server *server, int fd));

extern void
  ServerFdTwoWayPut A_((Server *server, int fd));

extern void
  ServerStatePut A_((Server *server, int state));

extern void
  ServerMaxRunJobsPut A_((Server *server, int maxRun));

extern void
  ServerMaxRunJobsPerUserPut A_((Server *server, int maxRunPerUser));

extern void
  ServerMaxRunJobsPerGroupPut A_((Server *server, int maxRunPerGroup));

extern void
  ServerQueuesPut A_((Server *server, Que *queues));

extern void
  ServerIntResAvailPut A_((Server *server, char *name, int value));

extern void
  ServerIntResAssignPut A_((Server *server, char *name, int value));

extern void
  ServerSizeResAvailPut A_((Server *server, char *name, Size value));

extern void
  ServerSizeResAssignPut A_((Server *server, char *name, Size value));

extern void
  ServerStringResAvailPut A_((Server *server, char *name, char *value));

extern void
  ServerStringResAssignPut A_((Server *server, char *name, char *value));

extern void
  ServerPrint A_((Server *server));

extern void
  ServerInit A_((Server *server));

extern int
  ServerOpenInit A_((Server *server));

extern int
  ServerOpen A_((Server *server));

extern int
  ServerRead A_((Server *server));

extern void *
  ServerWriteRead A_((Server *server, int msg, void *param));

extern int
  ServerClose A_((Server *server));

extern void
  ServerCloseFinal A_((Server *server));

extern void
  ServerStateRead A_((Server *server));

extern int
  JobAction A_((Job *job, Action action, void *param));

extern void
  ServerFree A_((Server *server));

/* Set stuff */
extern void
  SetServerInit A_((SetServer *ss));

extern void
  SetServerAdd A_((SetServer *ss, Server *s));

extern void
  SetServerFree A_((SetServer *ss));

extern void
  SetServerPrint A_((SetServer *ss));

extern int
  inSetServer A_((Server *s, SetServer *ss));

/* AllServers stuff */
extern int
  AllServersAdd A_((char *name, int port));

extern void
  AllServersInit A_((void));

extern SetServer *
  AllServersGet A_((void));

extern Server  *
  AllServersHeadGet A_((void));

extern Server  *
  AllServersLocalHostGet A_((void));

extern void
  AllServersLocalHostPut A_((Server *s));

extern void
  AllServersFree A_((void));

extern int
  SetServerSortInt A_((SetServer *s, int (*key)(), int order));

extern int
  SetServerSortStr A_((SetServer *s, char *(*key)(), int order));

extern int
  SetServerSortDateTime A_((SetServer *s, DateTime(*key)(), int order));

extern int
  SetServerSortSize A_((SetServer *s, Size(*key)(), int order));

extern int
  SetServerSortFloat A_((SetServer *s, double(*key)(), int order));

extern int
  inAccumTable A_((char *resName));

/* ServerNodes stuff */
extern SetCNode *
  ServerNodesGet A_((Server *server));

extern CNode *
  ServerNodesAdd A_((Server *s, char *name, int port, int queryMom));

extern CNode  *
  ServerNodesHeadGet A_((Server *s));

extern CNode  *
  ServerNodesTailGet A_((Server *s));

extern int
  ServerNodesQuery A_((Server *server, char *spec));

extern int
  ServerNodesReserve A_((Server *server, char *spec, int resId));

extern int
  ServerNodesRelease A_((Server *server, int resId));

extern int
  ServerNodesNumAvailGet A_((Server *server));

extern int
  ServerNodesNumAllocGet A_((Server *server));

extern int
  ServerNodesNumRsvdGet A_((Server *server));

extern int
  ServerNodesNumDownGet A_((Server *server));

extern void
  ServerNodesNumAvailPut A_((Server *server, int numAvail));

extern void
  ServerNodesNumAllocPut A_((Server *server, int numAlloc));

extern void
  ServerNodesNumRsvdPut A_((Server *server, int numRsvd));

extern void
  ServerNodesNumDownPut A_((Server *server, int numDown));

/* AllNodes stuff */
extern SetCNode *
  AllNodesGet A_((void));

extern CNode *
  AllNodesLocalHostGet A_((void));
#endif  /* _AF_SERVER_H */
