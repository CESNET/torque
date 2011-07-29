#include <license_pbs.h> /* See here for the software license */
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* LONG_MIN, LONG_MAX */
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains functions for manipulating attributes of type
 * Long integer, where "Long" is defined as the largest integer
 * available.
 *
 * Each set has functions for:
 * Decoding the value string to the machine representation.
 * Encoding the internal attribute to external form
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * --------------------------------------------------
 * The Set of Attribute Functions for attributes with
 * value type "Long" (_ll)
 * --------------------------------------------------
 */

/*
 * decode_ll - decode Long integer into attribute structure
 * Unlike decode_long, this function will decode octal (leading zero) and
 * hex (leading 0x or 0X) data as well as decimal
 *
 * Returns: 0 if ok
 *  >0 error number if error
 *  *patr elements set
 */

int decode_ll(

  attribute *patr,
  char      *name,  /* attribute name */
  char      *rescn, /* resource name, unused here */
  char      *val,   /* attribute value */
  int        perm)  /* only used for resources */

  {
  if ((val != (char *)0) && (strlen(val) != 0))
    {

    patr->at_val.at_ll = atoL(val);

    if ((patr->at_val.at_ll == LONG_MIN) || (patr->at_val.at_ll == LONG_MAX))
      return (PBSE_BADATVAL);  /* invalid string */

    patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    }
  else
    {
    patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) |
                     ATR_VFLAG_MODIFY;
    patr->at_val.at_ll = 0;
    }

  return (0);
  }

/*
 * encode_ll - encode attribute of type Long into attr_extern
 *
 * Returns: >0 if ok
 *   =0 if no value, no attrlist link added
 *   <0 if error
 */
/*ARGSUSED*/


int encode_ll(

  attribute  *attr,   /* ptr to attribute */
  tlist_head *phead,   /* head of attrlist list */
  char       *atname,  /* attribute name */
  char       *rsname,  /* resource name or null */
  int         mode,   /* encode mode, unused here */
  int         perm)  /* only used for resources */

  {
  size_t   ct;
  char cvn[MAXLONGLEN];
  svrattrl *pal;

  if (!attr)
    return (-1);

  if (!(attr->at_flags & ATR_VFLAG_SET))
    return (0);

  snprintf(cvn, MAXLONGLEN, LLD, attr->at_val.at_ll);

  ct = strlen(cvn) + 1;

  pal = attrlist_create(atname, rsname, ct);

  if (pal == (svrattrl *)0)
    return (-1);

  (void)memcpy(pal->al_value, cvn, ct);

  pal->al_flags = attr->at_flags;

  append_link(phead, &pal->al_link, pal);

  return (1);
  }

/*
 * set_ll - set attribute A to attribute B,
 * either A=B, A += B, or A -= B
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int
set_ll(struct attribute *attr, struct attribute *new, enum batch_op op)
  {
  assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

  switch (op)
    {

    case SET:
      attr->at_val.at_ll = new->at_val.at_ll;
      break;

    case INCR:
      attr->at_val.at_ll += new->at_val.at_ll;
      break;

    case DECR:
      attr->at_val.at_ll -= new->at_val.at_ll;
      break;

    default:
      return (PBSE_INTERNAL);
    }

  attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return (0);
  }

/*
 * comp_ll - compare two attributes of type Long
 *
 * Returns: +1 if 1st > 2nd
 *    0 if 1st == 2nd
 *   -1 if 1st < 2nd
 */

int
comp_ll(struct attribute *attr, struct attribute *with)
  {
  if (!attr || !with)
    return (-1);

  if (attr->at_val.at_ll < with->at_val.at_ll)
    return (-1);
  else if (attr->at_val.at_ll >  with->at_val.at_ll)
    return (1);
  else
    return (0);
  }

/*
 * free_ll - use free_null to (not) free space
 */
