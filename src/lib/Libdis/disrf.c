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
 * Synopsis:
 * 	float disrf(int stream, int *retval)
 *
 *	Gets a Data-is-Strings floating point number from <stream> and converts
 *	it into a float and returns it.  The number from <stream> consists of
 *	two consecutive signed integers.  The first is the coefficient, with its
 *	implied decimal point at the low-order end.  The second is the exponent
 *	as a power of 10.
 *
 *	*<retval> gets DIS_SUCCESS if everything works well.  It gets an error
 *	code otherwise.  In case of an error, the <stream> character pointer is
 *	reset, making it possible to retry with some other conversion strategy.
 *
 *	By fiat of the author, neither loss of significance nor underflow are
 *	errors.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "dis.h"
#include "dis_.h"
#undef disrf

static unsigned ndigs;
static unsigned nskips;
static double dval;

static int disrd_(

  int          stream,
  unsigned int count)

  {
  int		c;
  int		negate;
  unsigned int  unum;
  char		*cp;

  if (dis_umaxd == 0)
    disiui_();

  c = (*dis_getc)(stream);

  switch (c) 
    {
    case '-':
    case '+':

      negate = (c == '-');

      nskips = (count > FLT_DIG) ? 
        count - FLT_DIG : 
        0;

      count -= nskips;

      ndigs = count;

      dval = 0.0;

      do 
        {
        c = (*dis_getc)(stream);

        if ((c < '0') || (c > '9')) 
          {
          if (c < 0)
            {
            return (DIS_EOD);
            }

          return(DIS_NONDIGIT);
          }

        dval = dval * 10.0 + (double)(c - '0');
        } while (--count);

      count = nskips;

      if (count > 0) 
        {
        count--;

        switch ((*dis_getc)(stream)) 
          {
          case '5':

            if (count == 0)
              break;

            /* fall through */

          case '6':
          case '7':
          case '8':
          case '9':

            dval += 1.0;

            /* fall through */

          case '0':
          case '1':
          case '2':
          case '3':
          case '4':

            if ((count > 0) &&
               ((*disr_skip)(stream,(size_t)count) < 0))
              {
              return(DIS_EOD);
              }

            break;

          default:

            return(DIS_NONDIGIT);

            /*NOTREACHED*/

            break;
          }
        }

      dval = negate ? -dval : dval;

      return(DIS_SUCCESS);

      /*NOTREACHED*/

      break;

    case '0':

      return(DIS_LEADZRO);

      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':

      unum = c - '0';

      if (count > 1) 
        {
        if ((*dis_gets)(stream,dis_buffer + 1,count - 1) != count - 1)
          {
          return(DIS_EOD);
          }

        cp = dis_buffer;

        if (count >= dis_umaxd) 
          {
          if (count > dis_umaxd)
            break;

          *cp = c;

          if (memcmp(dis_buffer,dis_umax,dis_umaxd) > 0)
            break;
          }

        while (--count) 
          {
          c = *++cp;

          if ((c < '0') || (c > '9'))
            {
            return(DIS_NONDIGIT);
            }

          unum = unum * 10 + (unsigned)(c - '0');
          }
        }

      return(disrd_(stream,unum));

      /*NOTREACHED*/

      break;

    case -1:

      return(DIS_EOD);

      /*NOTREACHED*/

      break;

    case -2:

      return(DIS_EOF);

      /*NOTREACHED*/

      break;

    default:

      return(DIS_NONDIGIT);

      /*NOTREACHED*/

      break;
    }  /* END switch(c) */

  dval = HUGE_VAL;

  return(DIS_OVERFLOW);
  }  /* END disrd_() */




float disrf(

  int  stream,
  int *retval)

  {
	int		expon;
	unsigned	uexpon;
	int		locret;
	int		negate;

	assert(retval != NULL);
	assert(stream >= 0);
	assert(dis_getc != NULL);
	assert(dis_gets != NULL);
	assert(disr_skip != NULL);
	assert(disr_commit != NULL);

	dval = 0.0;
	if ((locret = disrd_(stream, 1)) == DIS_SUCCESS) {
		locret = disrsi_(stream, &negate, &uexpon, 1);
		if (locret == DIS_SUCCESS) {
			expon = negate ? nskips - uexpon : nskips + uexpon;
			if (expon + (int)ndigs > FLT_MAX_10_EXP) {
				if (expon + (int)ndigs > FLT_MAX_10_EXP + 1) {
					dval = dval < 0.0 ?
					    -HUGE_VAL : HUGE_VAL;
					locret = DIS_OVERFLOW;
				} else {
				        dval *= disp10d_(expon - 1);
					if (dval > FLT_MAX / 10.0) {
						dval = dval < 0.0 ?
						    -HUGE_VAL : HUGE_VAL;
						locret = DIS_OVERFLOW;
					} else
					        dval *= 10.0;
				}
			} else {
				if (expon < DBL_MIN_10_EXP) {
					dval *= disp10d_(expon + (int)ndigs);
					dval /= disp10d_((int)ndigs);
				} else
				        dval *= disp10d_(expon);
			}
		}
	}
	if ((*disr_commit)(stream, locret == DIS_SUCCESS) < 0)
	        locret = DIS_NOCOMMIT;
	*retval = locret;
	return (dval);
}
