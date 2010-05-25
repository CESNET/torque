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
 * svr_resc_def is the array of resource definitions for the server.
 * Each legal server resource is defined here.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"
#include "server_limits.h"
#include "server.h"

extern struct server server;

/*
 * The entries for each attribute are (see attribute.h):
 * name,
 * decode function,
 * encode function,
 * set function,
 * compare function,
 * free value space function,
 * access permission flags,
 * value type
 */

/* sync w/job_attr_def.c */

static int decode_nodes A_((struct attribute *, char *, char *, char *));
static int set_node_ct A_((resource *, attribute *, int actmode));
static int set_tokens A_((struct attribute *attr, struct attribute *new, enum batch_op actmode));
static int set_mppnodect A_((resource *, attribute *, int actmode));

resource_def *svr_resc_def;

resource_def svr_resc_def_const[] =
  {

    { "arch",    /* system architecture type */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE,
    ATR_TYPE_STR
    },
  { "cpupercent",
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG
  },
  { "cput",
    decode_time,
    encode_time,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_LONG
  },
  { "file",
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_SIZE
  },
  { "mem",
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN | ATR_DFLAG_RASSN,
    ATR_TYPE_SIZE
  },
  { "pmem",
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN | ATR_DFLAG_RMOMIG,
    ATR_TYPE_SIZE
  },
  { "ncpus",                        /* number of processors for job */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG | ATR_DFLAG_RASSN,
    ATR_TYPE_LONG
  },
  { "vmem",
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN | ATR_DFLAG_RASSN,
    ATR_TYPE_SIZE
  },
  { "pvmem",
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_SIZE
  },
  { "nice",    /* job nice value */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "pcput",
    decode_time,
    encode_time,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "walltime",   /* wall clock time limit for a job */
    decode_time,
    encode_time,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_LONG
  },
  { "nodemask",
    decode_ll,
    encode_ll,
    set_ll,
    comp_ll,
    free_null,
    NULL_FUNC,
    NO_USER_SET | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_LL
  },
  { "host",    /* host to execute on */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE,
    ATR_TYPE_STR
  },
  { "nodes",   /* user specification of nodes */
    decode_nodes,
    encode_str,
    set_str,
    comp_str,
    free_str,
    set_node_ct,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_STR
  },
  { "neednodes",   /* scheduler modified specification */
    decode_str,   /* of nodes */
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    ATR_DFLAG_MGRD | ATR_DFLAG_MGWR | ATR_DFLAG_SvWR | ATR_DFLAG_RMOMIG | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_STR
  },
  { "nodect",   /* count of number of nodes requested */
    decode_l,   /* read-only, set by server whenever  */
    encode_l,   /* "nodes" is set, for use by sched   */
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_ONLY | ATR_DFLAG_MGWR | ATR_DFLAG_RASSN | ATR_DFLAG_RMOMIG,
    ATR_TYPE_LONG
  },
    {
    "mpplabel",   /* PE label required for execution */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE,
    ATR_TYPE_STR
    },
    {
    "other",
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_STR
    },
  { "software",   /* software required for execution */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE,
    ATR_TYPE_STR
  },
  { "taskspn",   /* number of tasks per node      */
    decode_l,   /* set by admin on queue or system */
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_LONG
  },

  /* the following are found only on Cray systems */

  { "mta",    /* mag tape, a class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mtb",    /* mag tape, b class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mtc",    /* mag tape, c class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mtd",    /* mag tape, d class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mte",    /* mag tape, e class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mtf",    /* mag tape, f class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mtg",    /* mag tape, g class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mth",    /* mag tape, h class */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "pf",    /* max file space for job */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_SIZE
  },
  { "ppf",    /* max file space per process */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_SIZE
  },
  { "sds",    /* max SDS for job */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_SIZE
  },
  { "psds",    /* max SDS per process */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_SIZE
  },
  { "procs",   /* number of processes per job */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_LONG
  },
  { "mppe",    /* number of mpp nodes */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },
  { "mppt",    /* total mpp time for job */
    decode_time,
    encode_time,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_LONG
  },
  { "pmppt",   /* max mpp time for any process */
    decode_time,
    encode_time,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_LONG
  },
  { "mppmem",   /* max mppmem memory for job */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
    ATR_TYPE_SIZE
  },
  { "srfs_tmp",   /* SRFS TMPDIR allocation */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_SIZE
  },
  { "srfs_big",   /* SRFS BIGDIR allocation */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_SIZE
  },
  { "srfs_fast",   /* SRFS FASTDIR allocation */
    decode_size,
    encode_size,
    set_size,
    comp_size,
    free_null,
    NULL_FUNC,
    READ_WRITE | ATR_DFLAG_MOM,
    ATR_TYPE_SIZE
  },
  { "srfs_assist",   /* SRFS assist flag */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    ATR_DFLAG_OPRD | ATR_DFLAG_MGRD | ATR_DFLAG_SvRD | ATR_DFLAG_SvWR | ATR_DFLAG_MOM,
    ATR_TYPE_LONG
  },

    {
    "pe_mask",
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    NO_USER_SET | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG,
    ATR_TYPE_STR
    },
  { ATTR_tokens,                               /* tokens required to run */
    decode_tokens,
    encode_str,
    set_tokens,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_WRITE,
    ATR_TYPE_STR
  },

  /* Cray XT */
  { "size", decode_l, encode_l, set_l, comp_l, free_null, NULL_FUNC, READ_WRITE | ATR_DFLAG_MOM, ATR_TYPE_LONG },
  { "cpapartitionid", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "cpaalloccookie", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "cpaadmincookie", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  {   "mppwidth",     /* Number mpp PEs (processing elements) */
      decode_l,    /* PE = ALPS launched binary invocation on compute node */
      encode_l,
      set_l,
      comp_l,
      free_null,
      set_mppnodect,
      READ_WRITE,
      ATR_TYPE_LONG
  },
  {   "mppdepth",    /* Number of threads per PE */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_LONG
  },
  {   "mppnppn",    /* number of PEs per Node */
      decode_l,     /* applies to multi-core systems only */
      encode_l,
      set_l,
      comp_l,
      free_null,
      set_mppnodect,
      READ_WRITE,
      ATR_TYPE_LONG
  },
  {   "mppnodes",    /* node list (can be of the form 1,3-7) */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_STR
  },
  {   "mpplabels",    /* compute node features */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_STR
  },
  {   "mpptime",   /* NYI */
      decode_time,
      encode_time,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_LONG
  },
  {   "mpphost",
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_STR
  },
  {   "mpparch",    /* node architecture (XT3, XT4, etc.) */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      READ_WRITE,
      ATR_TYPE_STR
  },
  { "mppnodect", /* node count estimate for queueing purposes */
      decode_l, /* derived from mppwidth,mppdepth,mppnppn */
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG
  },

  /* support external resource manager extensions */

  /* NOTE:  should enable expansion of this list dynamically (NYI) */

  { "advres", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "deadline", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "depend", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "ddisk", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "dmem", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "epilogue", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE | ATR_DFLAG_MOM, ATR_TYPE_STR },
  { "feature", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "flags", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "gattr", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "geometry", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "gmetric", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "gres", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG, ATR_TYPE_STR },
  { "hostlist", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "image", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "jgroup", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "jobflags", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },  /* same as flags */
  { "latency", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "loglevel", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "minprocspeed", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "minpreempttime", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "minwclimit", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "naccesspolicy", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "nallocpolicy", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "nodeset", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "opsys", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "os", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "partition", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "pref", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "procs", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "procs_bitmap", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_RMOMIG, ATR_TYPE_STR },
  { "prologue", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE | ATR_DFLAG_MOM, ATR_TYPE_STR },
  { "qos", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "queuejob", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "reqattr", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "retrycount", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "retrycc", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "rmtype", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "select", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "sid", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "signal", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "stagein", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "spriority", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "subnode", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "subnode_list", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "taskdistpolicy", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "template", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "termsig", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "termtime", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "tid", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "tpn", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "trig", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "trl", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "var", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "vcores", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },
  { "wcrequeue", decode_str, encode_str, set_str, comp_str, free_str, NULL_FUNC, READ_WRITE, ATR_TYPE_STR },

  /* the definition for the "unknown" resource MUST be last */

  { "|unknown|", decode_unkn, encode_unkn, set_unkn, comp_unkn, free_unkn, NULL_FUNC, READ_WRITE, ATR_TYPE_LIST }

  /* DO NOT ADD DEFINITIONS AFTER "unknown", ONLY BEFORE */
  };

int svr_resc_size = 0;


int init_resc_defs(void)

  {
  resource_def *tmpresc = NULL;
  int rindex = 0, dindex = 0, unkindex = 0;
#ifndef PBS_MOM

  struct array_strings *resc_arst = NULL;
  char                 *extra_resc;
  int   resc_num = 0;
#endif

  svr_resc_size = sizeof(svr_resc_def_const) / sizeof(resource_def);

#ifndef PBS_MOM
  /* build up a temporary list of string resources */

  if (server.sv_attr[(int)SRV_ATR_ExtraResc].at_flags & ATR_VFLAG_SET)
    {
    resc_arst = server.sv_attr[(int)SRV_ATR_ExtraResc].at_val.at_arst;

    tmpresc = calloc(resc_arst->as_usedptr + 1, sizeof(resource_def));

    if (tmpresc == NULL)
      {
      return(-1);
      }

    for (resc_num = 0;resc_num < resc_arst->as_usedptr;resc_num++)
      {
      extra_resc = resc_arst->as_string[resc_num];

      (tmpresc + resc_num)->rs_name = strdup(extra_resc);
      (tmpresc + resc_num)->rs_decode = decode_str;
      (tmpresc + resc_num)->rs_encode = encode_str;
      (tmpresc + resc_num)->rs_set = set_str;
      (tmpresc + resc_num)->rs_comp = comp_str;
      (tmpresc + resc_num)->rs_free = free_str;
      (tmpresc + resc_num)->rs_action = NULL_FUNC;
      (tmpresc + resc_num)->rs_flags = READ_WRITE;
      (tmpresc + resc_num)->rs_type = ATR_TYPE_STR;

      dindex++;

      }
    }

#endif

  svr_resc_def = calloc(svr_resc_size + dindex, sizeof(resource_def));

  if (svr_resc_def == NULL)
     {
     return(-1);
     }

  /* copy all const resources, except for the last "unknown" */
  for (rindex = 0; rindex < (svr_resc_size - 1); rindex++)
    {
    memcpy(svr_resc_def + rindex, svr_resc_def_const + rindex, sizeof(resource_def));
    }

  unkindex = rindex;

  /* copy our dynamic resources */

  if (tmpresc)
    {
    for (dindex = 0; (tmpresc + dindex)->rs_decode; dindex++)
      {
      if (find_resc_def(svr_resc_def, (tmpresc + dindex)->rs_name, rindex) == NULL)
        {
        memcpy(svr_resc_def + rindex, tmpresc + dindex, sizeof(resource_def));
        rindex++;
        }
      }

    free(tmpresc);
    }

  /* copy the last "unknown" resource */
  memcpy(svr_resc_def + rindex, svr_resc_def_const + unkindex, sizeof(resource_def));

  svr_resc_size = rindex + 1;

  /* uncomment if you feel like debugging this
    for (rindex=0; rindex<svr_resc_size; rindex++)
      {
      fprintf(stderr,"resource: %s (%d)\n",(svr_resc_def+rindex)->rs_name,rindex);
      }
  */
  return 0;
  }






/*
 * decode_nodes - decode a node requirement specification,
 * Check if node requirement specification is syntactically ok,
 * then call decode_str()
 *
 * val is of the form: node_spec[+node_spec...]
 * where node_spec is: number | property | number:property
 */

static int decode_nodes(

  struct attribute *patr,
  char             *name,   /* attribute name */
  char             *rescn,  /* resource name - unused here */
  char             *val)    /* attribute value */

  {
  char *pc;

  pc = val;

  while (1)
    {
    while (isspace(*pc))
      ++pc;

    if (!isalnum(*pc))
      {
      return(PBSE_BADATVAL);
      }

    if (isdigit(*pc))
      {
      pc++;

      while (isdigit(*pc))
        pc++;

      if (*pc == '\0')
        break;

      if ((*pc != '+') && (*pc != ':') && (*pc != '#'))
        {
        return(PBSE_BADATVAL);
        }
      }
    else if (isalpha(*pc))
      {
      pc++;

      while (*pc != '\0')
        {
        if (isalnum(*pc) || strchr("-.=_", *pc))
          pc++;
        else
          break;
        }

      if (*pc == '\0')
        break;

      if ((*pc != '+') && (*pc != ':') && (*pc != '#'))
        {
        return(PBSE_BADATVAL);
        }
      }

    ++pc;
    }  /* END while(1) */

  return(decode_str(patr, name, rescn, val));
  }  /* END decode_nodes() */





/*
 * ctnodes = count nodes, turn node spec (see above) into a
 * plain number of nodes.
 */

int ctnodes(

  char *spec)

  {
  int   ct = 0;
  char *pc;

  while (1)
    {
    while (isspace((int)*spec))
      ++spec;

    if (isdigit((int)*spec))
      ct += atoi(spec);
    else
      ++ct;

    if ((pc = strchr(spec, '+')) == NULL)
      break;

    spec = pc + 1;
    }  /* END while (1) */

  return(ct);
  }  /* END ctnodes() */






/*
 * set_node_ct = set node count
 *
 * This is the "at_action" routine for the resource "nodes".
 * When the resource_list attribute changes, then set/update
 * the value of the resource "nodect" for use by the scheduler.
 */

static int set_node_ct(

  resource  *pnodesp,  /* I */
  attribute *pattr,    /* I */
  int        actmode)  /* I */

  {
  resource *pnct;
  resource_def *pndef;

  if (actmode == ATR_ACTION_RECOV)
    {
    /* SUCCESS */

    return(0);
    }

  /* Set "nodect" to count of nodes in "nodes" */

  pndef = find_resc_def(svr_resc_def, "nodect", svr_resc_size);

  if (pndef == NULL)
    {
    return(PBSE_SYSTEM);
    }

  if ((pnct = find_resc_entry(pattr, pndef)) == NULL)
    {
    if ((pnct = add_resource_entry(pattr, pndef)) == 0)
      {
      return(PBSE_SYSTEM);
      }
    }

  pnct->rs_value.at_val.at_long =

    ctnodes(pnodesp->rs_value.at_val.at_str);

  pnct->rs_value.at_flags |= ATR_VFLAG_SET;

  /* Set "neednodes" to "nodes", may be altered by scheduler */

  pndef = find_resc_def(svr_resc_def, "neednodes", svr_resc_size);

  if (pndef == NULL)
    {
    return(PBSE_SYSTEM);
    }

  if ((pnct = find_resc_entry(pattr, pndef)) == NULL)
    {
    if ((pnct = add_resource_entry(pattr, pndef)) == NULL)
      {
      return(PBSE_SYSTEM);
      }
    }
  else
    {
    pndef->rs_free(&pnct->rs_value);
    }

  pndef->rs_decode(&pnct->rs_value, NULL, NULL, pnodesp->rs_value.at_val.at_str);

  pnct->rs_value.at_flags |= ATR_VFLAG_SET;

  /* SUCCESS */

  return(0);
  }  /* END set_node_ct() */





/*
 * set_tokens = set node count
 *
 */

static int set_tokens(

  struct attribute *attr,
  struct attribute *new,
  enum batch_op     op)

  {
  char  *colon = NULL;
  float  count = 0;

  int ret = 0;

  if (new != NULL)
    {
    colon = strchr(new->at_val.at_str, (int)':');

    if (colon == NULL)
      {
      ret = PBSE_BADATVAL;
      }
    else
      {
      colon++;
      count = atof(colon);

      if ((count <= 0) || (count > 1000))
        {
        ret = PBSE_BADATVAL;
        }
      }
    }

  if (ret == 0)
    {
    ret = set_str(attr, new, op);
    }

  return(ret);
  }  /* END set_tokens() */

/*
 * set_mppnodect
 *
 */

static int set_mppnodect(

  resource *res,
  attribute *attr,
  int op)

  {
  int width;
  int nppn;
  int nodect;
  int have_mppwidth = 0;
  int have_mppnppn = 0;
  resource_def *pdef;
  resource *pent = NULL;

  /* Go find the currently known width, nppn attributes */

  width = 0;
  nppn = 0;

  if (((pdef = find_resc_def(svr_resc_def,"mppwidth",svr_resc_size))) &&
    ((pent = find_resc_entry(attr,pdef))))
    {
    width = pent->rs_value.at_val.at_long;
    have_mppwidth = 1;
    }

  if (((pdef = find_resc_def(svr_resc_def,"mppnppn",svr_resc_size))) &&
    ((pent = find_resc_entry(attr,pdef))))
    {
    nppn = pent->rs_value.at_val.at_long;
    have_mppnppn = 1;
    }

  /* Check for width less than a node */

  if ((width) && (width < nppn))
    {
    nppn = width;
    pent->rs_value.at_val.at_long = nppn;
    pent->rs_value.at_flags |= ATR_VFLAG_SET;
    }

  /* Compute an estimate for the number of nodes needed */

  nodect = width;
  if (nppn>1)
    {
    nodect = (nodect + nppn - 1) / nppn;
    }

  /* Find or create the "mppnodect" attribute entry */

  if (((pdef = find_resc_def(svr_resc_def,"mppnodect",svr_resc_size))) &&
    (((pent = find_resc_entry(attr,pdef)) == NULL)) &&
    (((pent = add_resource_entry(attr,pdef)) == 0)))
    {
    return (PBSE_SYSTEM);
    }

  /* Update the value */

  if (!have_mppwidth || !have_mppnppn)
    {
    pent->rs_value.at_val.at_long = -1;
    }
  else
    {
    pent->rs_value.at_val.at_long = nodect;
    }
  pent->rs_value.at_flags |= ATR_VFLAG_SET;

  return (0);

  } /* END set_mppnodect() */
