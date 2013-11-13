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
 * node_attr_def is the array of "attribute" definitions for a node.
 * Each legal node "attribute" is defined here
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"

/* External Functions Referenced */

/*
* extern int decode_state A_((struct attribute *patr, char *nam, char *rn, char *val));
* extern int decode_props A_((struct attribute *patr, char *nam, char *rn, char *val));
* extern int encode_state      A_((struct attribute *patr, tlist_head *ph, char *anm, char *rmn, int mode));
* extern int encode_props      A_((struct attribute *patr, tlist_head *ph, char *anm, char *rmn, int mode));
* extern int set_node_state    A_((attribute *patr, attribute *new, enum batch_op));
* extern int set_node_props    A_((attribute *patr, attribute *new, enum batch_op));
* extern int node_state        A_((attribute *patr, void *pobj, int actmode));
* extern int node_np_action    A_((attribute *patr, void *pobj, int actmode));
* extern int node_prop_list    A_((attribute *patr, void *pobj, int actmode));
* extern void free_null      A_((attribute *patr));
* extern void free_prop_attr   A_((attribute *patr));
* extern int  comp_null      A_((attribute *patr, attribute *with));
*/


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

attribute_def node_attr_def[] =
  {

  /* ND_ATR_state */
    { ATTR_NODE_state, /* "state" */
    decode_state,
    encode_state,
    set_node_state,
    comp_null,
    free_null,
    node_state,
    NO_USER_SET,
    ATR_TYPE_SHORT,
    PARENT_TYPE_NODE,
    },

  /* ND_ATR_np */
  { ATTR_NODE_np,  /* "np" */
    decode_l,
    encode_l,
    set_l,
    comp_null,
    free_null,
    node_np_action,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },
  { ATTR_NODE_npfree, /* "npfree" */
    decode_null,
    encode_l,
    set_null,
    comp_null,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },
  { ATTR_NODE_npshared, /* "npshared" */
    decode_null,
    encode_l,
    set_null,
    comp_null,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },
  /* ND_ATR_properties */
  { ATTR_NODE_properties, /* "properties" */
    decode_arst,
    encode_arst,
    set_arst,
    comp_null,
    free_arst,
    node_prop_list,
    MGR_ONLY_SET,
    ATR_TYPE_ARST,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_adproperties */
  { ATTR_NODE_adproperties,
    decode_arst,
    encode_arst,
    set_arst,
    comp_null,
    free_arst,
    node_adprop_list,
    MGR_ONLY_SET,
    ATR_TYPE_ARST,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_ntype */
  { ATTR_NODE_ntype, /* "ntype" */
    decode_ntype,
    encode_ntype,
    set_node_ntype,
    comp_null,
    free_null,
    node_ntype,
    NO_USER_SET,
    ATR_TYPE_SHORT,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_jobs */
  {   ATTR_NODE_jobs,         /* "jobs" */
      decode_null,
      encode_jobs,
      set_null,
      comp_null,
      free_null,
      NULL_FUNC,
      ATR_DFLAG_RDACC,
      ATR_TYPE_JINFOP,
      PARENT_TYPE_NODE,
  },

  /* ND_ATR_status */
  {  ATTR_NODE_status,
     decode_arst,
     encode_arst,
     set_arst,
     comp_null,
     free_arst,
     node_status_list,
     MGR_ONLY_SET,
     ATR_TYPE_ARST,
     PARENT_TYPE_NODE,
  },

  /* ND_ATR_note */
  { ATTR_NODE_note, /* "note" */
    decode_str,
    encode_str,
    set_note_str,
    comp_str,
    free_str,
    node_note,
    NO_USER_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_no_multinode_jobs */
  { ATTR_NODE_no_multinode_jobs,
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    node_no_multinode,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_exclusively_assigned */
  { ATTR_NODE_exclusively_assigned,
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    node_exclusively_assigned,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,  	
  },

  /* ND_ATR_queue */
  { ATTR_NODE_queue,
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    node_queue,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_cloud */
  { ATTR_NODE_cloud,
    decode_str,
    encode_str,
    set_str,
    comp_null,
    free_str,
    node_cloud,
    MGR_ONLY_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_NODE
  },

  /* ND_ATR_noautoresv */
  { ATTR_NODE_noautoresv,
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    node_noautoresv,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_priority */
  { ATTR_NODE_priority, /* set node priority */
    decode_l,
    encode_l,
    set_l,
    comp_null,
    free_null,
    node_priority,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_machine_spec */
  { ATTR_NODE_machine_spec, /* machine_spec */
    decode_dbl,
    encode_dbl,
    set_dbl,
    comp_dbl,
    free_null,
    node_machine_spec,
    MGR_ONLY_SET,
    ATR_TYPE_DOUBLE,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_NodeCost */
  { ATTR_NODE_fairshare_coef, /* fairshare_coef */
    decode_dbl,
    encode_dbl,
    set_dbl,
    comp_dbl,
    free_null,
    node_fairshare_coef,
    MGR_ONLY_SET,
    ATR_TYPE_DOUBLE,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_admin_slot_enabled */
  { ATTR_NODE_admin_slot_enabled, /* determines whether the admin slot is enabled */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    node_admin_slot_enable,
    MGR_ONLY_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_admin_slot_available */
  { ATTR_NODE_admin_slot_available, /* follows the admin slot occupied state */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    node_admin_slot_avail,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_resources_total */
  { ATTR_NODE_resources_total,
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    MGR_ONLY_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_NODE,
  },

  /* ND_ATR_resources_used */
  { ATTR_NODE_resources_used,
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_RESC,
    PARENT_TYPE_NODE,
  },

  };
