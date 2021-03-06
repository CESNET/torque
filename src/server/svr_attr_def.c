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
 * svr_attr_def is the array of attribute definitions for the server.
 * Each legal server attribute is defined here
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"

/* External Functions Referenced */

extern int manager_oper_chk A_((attribute *pattr, void *pobject, int actmode));
extern int servername_chk A_((attribute *pattr, void *pobject, int actmode));
extern int schiter_chk A_((attribute *pattr, void *pobject, int actmode));

extern int nextjobnum_chk A_((attribute *pattr, void *pobject, int actmode));
extern int set_nextjobnum A_((attribute *attr, attribute *new, enum batch_op op));

extern int poke_scheduler A_((attribute *pattr, void *pobject, int actmode));

extern int encode_svrstate A_((attribute *pattr, tlist_head *phead, char *aname,
                                 char *rsname, int mode));

extern int decode_rcost A_((attribute *patr, char *name, char *rn, char *val));
extern int encode_rcost A_((attribute *attr, tlist_head *phead, char *atname,
                              char *rsname, int mode));
extern int set_rcost A_((attribute *attr, attribute *new, enum batch_op));
extern void free_rcost A_((attribute *attr));
extern int decode_null A_((attribute *patr, char *name, char *rn, char *val));
extern int set_null A_((attribute *patr, attribute *new, enum batch_op op));

extern int token_chk A_((attribute *pattr, void *pobject, int actmode));
extern int set_tokens A_((struct attribute *attr, struct attribute *new, enum batch_op op));

extern int extra_resc_chk A_((attribute *pattr, void *pobject, int actmode));
extern void free_extraresc A_((attribute *attr));
extern void restore_attr_default A_((struct attribute *));

/* DIAGTODO: write diag_attr_def.c */

/*
 * The entries for each attribute are (see attribute.h):
 * name,
 * decode function,
 * encode function,
 * set function,
 * compare function,
 * free value space function,
 * action function,
 * access permission flags,
 * value type
 */

/* define ATTR_XXX in pbs_ifl.h */
/* sync SRV_ATTR_XXX w/enum srv_atr in server.h */
/* define default in server_limits.h */
/* set default in pbsd_init() in pbsd_init.c */

attribute_def svr_attr_def[] =
  {

  /* SRV_ATR_State */
    { ATTR_status,  /* "server_state" */
    decode_null,
    encode_svrstate,
    set_null,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER,
    },

  /* SRV_ATR_scheduling */
  { ATTR_scheduling,
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    poke_scheduler,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER,
  },

  /* SRV_ATR_max_running */
  { ATTR_maxrun,  /* "max_running" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MaxUserRun */
  { ATTR_maxuserrun, /* "max_user_run" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MaxGrpRun */
  { ATTR_maxgrprun,  /* "max_group_run" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_TotalJobs */
  { ATTR_total,  /* "total_jobs" */
    decode_null,
    encode_l,
    set_null,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobsByState */
  { ATTR_count,  /* "state_count" */
    decode_null,  /* note-uses fixed buffer in server struct */
    encode_str,
    set_null,
    comp_str,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_acl_host_enable */
  { ATTR_aclhten,  /* "acl_host_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_acl_hosts */
  { ATTR_aclhost,  /* "acl_hosts" */
    decode_arst,
    encode_arst,
    set_hostacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AclUserEnabled */ /* User ACL to be used */
  { ATTR_acluren,  /* "acl_user_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AclUsers */  /* User Acess Control List */
  { ATTR_acluser,  /* "acl_users" */
    decode_arst,
    encode_arst,
    set_uacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AclRoot */  /* List of which roots may execute jobs */
  { ATTR_aclroot,  /* "acl_roots"    */
    decode_arst,
    encode_arst,
    set_uacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_managers */
  { ATTR_managers,  /* "managers" */
    decode_arst,
    encode_arst,
    set_uacl,
    comp_arst,
    free_arst,
    manager_oper_chk,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_operators */
  { ATTR_operators,  /* "operators" */
    decode_arst,
    encode_arst,
    set_uacl,
    comp_arst,
    free_arst,
    manager_oper_chk,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_dflt_que */
  { ATTR_dfltque,  /* "default_queue" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_log_events */
  { ATTR_logevents,  /* "log_events" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    restore_attr_default,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_mailfrom */
  { ATTR_mailfrom,  /* "mail_from" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_query_others */
  { ATTR_queryother, /* "query_other_jobs" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_resource_avail */
  { ATTR_rescavail,  /* "resources_available" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_resource_deflt */
  { ATTR_rescdflt,  /* "resources_default" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_ResourceMax */
  { ATTR_rescmax,  /* "resources_max" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_resource_assn */
  { ATTR_rescassn,  /* "resources_assigned" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_RESC,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_resource_cost */
  { ATTR_resccost,  /* "resources_cost" */
    decode_rcost, /* these are not right, haven't figured this out yet */
    encode_rcost,
    set_rcost,
    NULL_FUNC,
    free_rcost,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_sys_cost */
  { ATTR_syscost,  /* "system_cost" */
    decode_l,
    encode_l,
    set_l,
    NULL_FUNC,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_schedule_iteration */
  { ATTR_schedit,  /* "schedule_iteration" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_noop,  /* disable unset */
    schiter_chk,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_ping_rate */
  {   ATTR_pingrate,          /* "node_ping_rate" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_check_rate */
  {   ATTR_ndchkrate,         /* "node_check_rate" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_noop,  /* disable unset */
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_tcp_timeout */
  {   ATTR_tcptimeout,         /* "tcp_timeout" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      restore_attr_default,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_Comment */
  { ATTR_comment,  /* "comment"  - information */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_DefNode */
  { ATTR_defnode,  /* "default_node" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_NodePack */
  { ATTR_nodepack,  /* "node_pack" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_NodeSuffix */
  {   ATTR_nodesuffix,        /* "node_suffix" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobStatRate */
  { ATTR_jobstatrate, /* "job_stat_rate" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    restore_attr_default,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_PollJobs */
  { ATTR_polljobs,  /* "poll_jobs" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    restore_attr_default,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_LogLevel */
  {   ATTR_loglevel,  /* "loglevel" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      restore_attr_default,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_DownOnError */
  {   ATTR_downonerror, /* "down_on_error" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_DisableServerIdCheck */
  {   ATTR_disableserveridcheck,       /* "disable_server_id_check" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobNanny */
  {   ATTR_jobnanny,  /* "job_nanny" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_OwnerPurge */
  {   ATTR_ownerpurge,       /* "owner_purge" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_QCQLimits */
  {   ATTR_qcqlimits,       /* "queue_centric_limits" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MomJobSync */
  { ATTR_momjobsync, /* "mom_job_sync" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MailDomain */
  { ATTR_maildomain, /* "mail_domain" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_version */
  { ATTR_pbsversion, /* "pbs_version" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_KillDelay */
  {   ATTR_killdelay,         /* "kill_delay" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AclLogic */
  {   ATTR_acllogic,          /* "acl_logic_or" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AclGroupSloppy */
  {   ATTR_aclgrpslpy,          /* "acl_group_sloppy" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_KeepCompleted */
  {   ATTR_keepcompleted,     /* "keep_completed" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_SubmitHosts */
  {   ATTR_submithosts,         /* "submit_hosts" */
      decode_arst,
      encode_arst,
      set_arst,
      comp_arst,
      free_arst,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_ARST,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AllowNodeSubmit */
  {   ATTR_allownodesubmit,     /* "allow_node_submit" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AllowProxyUser */
  {   ATTR_allowproxyuser,     /* "allow_proxy_user" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AutoNodeNP */
  {   ATTR_autonodenp,          /* "auto_node_np" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_LogFileMaxSize */
  {   ATTR_logfilemaxsize,      /* "log_file_max_size" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_LogFileRollDepth */
  {   ATTR_logfilerolldepth,    /* "log_file_roll_depth" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SVR_ATR_Log_Keep_Days */
  {
      ATTR_logkeepdays,          /* "log_keep_days" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_NextJobNumber */
  { ATTR_nextjobnum,
    decode_l,
    encode_l,
    set_nextjobnum,
    comp_l,
    free_noop,
    nextjobnum_chk,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_tokens */
  {  ATTR_tokens,
     decode_arst,
     encode_arst,
     set_tokens,
     comp_arst,
     free_arst,
     token_chk,
     MGR_ONLY_SET,
     ATR_TYPE_ARST,
     PARENT_TYPE_SERVER
  },

  /* SRV_ATR_NetCounter */
  { ATTR_netcounter,  /* "net_counter" */
    decode_null,
    encode_str,
    set_null,
    comp_str,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_ExtraResc */
  {   ATTR_extraresc,  /* "extra_resc" */
      decode_arst,
      encode_arst,
      set_arst,
      comp_arst,
      free_extraresc,
      extra_resc_chk,
      NO_USER_SET,
      ATR_TYPE_ARST,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_ServerName */
  {   ATTR_servername,     /* "server_name" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      servername_chk,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_SchedVersion */
  {   ATTR_schedversion,     /* "sched_version" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_AcctKeepDays */
  {   ATTR_acctkeepdays,      /* "accounting_keep_days" */
    	decode_l,
    	encode_l,
    	set_l,
    	comp_l,
    	free_null,
    	NULL_FUNC,
    	NO_USER_SET,
    	ATR_TYPE_LONG,
    	PARENT_TYPE_SERVER
  },

  /* SRV_ATR_lockfile */
  {	  ATTR_lockfile,		/* "lock_file" */
    	decode_str,
    	encode_str,
    	set_str,
    	comp_str,
    	free_str,
    	NULL_FUNC,
    	MGR_ONLY_SET,
    	ATR_TYPE_STR,
    	PARENT_TYPE_SERVER
  },

  /* SRV_ATR_lockfile */
  {   ATTR_LockfileUpdateTime, /* lock_file_update_time */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_LockfileCheckTime */
  {   ATTR_LockfileCheckTime, /* lock_file_check_time */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_CredentialLifetime */
  {   ATTR_credentiallifetime,  /* "credential_lifetime" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobMustReport */
  { ATTR_jobmustreport,      /* "job_must_report" */
    	decode_b,
    	encode_b,
    	set_b,
    	comp_b,
    	free_null,
    	NULL_FUNC,
    	MGR_ONLY_SET,
    	ATR_TYPE_LONG,
    	PARENT_TYPE_SERVER
  },

  /* SRV_ATR_checkpoint_dir */
  {   ATTR_checkpoint_dir,   /* "checkpoint_dir" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
  },

  /*SRV_ATR_display_job_server_suffix */
  { ATTR_dispsvrsuffix, /* "display_job_server_suffix" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /*SRV_ATR_job_suffix_alias */
  { ATTR_jobsuffixalias, /* "job_suffix_alias" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MailSubjectFmt */
  { ATTR_mailsubjectfmt, /* "mail_subject_fmt" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_MailBodyFmt */
  { ATTR_mailbodyfmt, /* "mail_body_fmt" */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_SERVER
  },
    /* SRV_ATR_NPDefault */
  {   ATTR_npdefault,          /* "np_default" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobStartTimeout */
  {   ATTR_jobstarttimeout,         /* "job_start_timeout" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SRV_ATR_JobForceCancelTime */
  {   ATTR_jobforcecanceltime,     /* job_force_cancel_time */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_SERVER
  },

  /* SVR_ATR_MaxInstallingNodes */
  { ATTR_MaxInstallingNodes,  /* "max_installing_nodes" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_ResourcesToStore */
  {   ATTR_ResourcesToStore, /* "resources_to_store" */
      decode_arst,
      encode_arst,
      set_arst,
      comp_arst,
      free_arst,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_ARST,
      PARENT_TYPE_SERVER
  },
  /* SRV_ATR_ResourcesMappings */
  {   ATTR_ResourcesMappings, /* "resources_mappings" */
      decode_arst,
      encode_arst,
      set_arst,
      comp_arst,
      free_arst,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_ARST,
      PARENT_TYPE_SERVER
  },

  /* SVR_ATR_krb_realm_submit_acl */
  {   ATTR_krb_realm_submit_acl, /* "krb_realm_submit_acl" */
      decode_arst,
      encode_arst,
      set_hostacl,
      comp_arst,
      free_arst,
      NULL_FUNC,
      MGR_ONLY_SET,
      ATR_TYPE_ACL,
      PARENT_TYPE_SERVER,
  },


  /* SVR_ATR_acl_krb_realm_enable */
  { ATTR_acl_krb_realm_enable,  /* "acl_krb_realm_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_SERVER
  },

  /* SRV_ATR_acl_krb_realms */
  { ATTR_acl_krb_realms,  /* "acl_krb_realms" */
    decode_arst,
    encode_arst,
    set_hostacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_SERVER
  },

    { ATTR_lbserver,
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_SERVER
    },

  /* SRV_ATR_ignore_properties */
  { ATTR_ignore_properties,
    decode_arst,
    encode_arst,
    set_arst,
    comp_arst,
    free_arst,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_ARST,
    PARENT_TYPE_SERVER
  },

  /* site supplied server attribute definitions if any, see site_svr_attr_*.h  */
#include "site_svr_attr_def.h"

  };
