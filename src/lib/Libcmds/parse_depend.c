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

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pbs_ifl.h"
#include "cmds.h"

static char *deptypes[] = {
	"on",		/* "on" and "synccount" must be first two */
	"synccount",
	"after",
	"afterok",
	"afternotok",
	"afterany",
	"before",
	"beforeok",
	"beforenotok",
	"beforeany",
	"syncwith",
	(char *)0
};

/*
 *
 * parse_depend_item
 *
 * syntax:
 *
 *	jobid[:jobid...]
 *
 */

int parse_depend_item(depend_list, rtn_list, rtn_size)
	char *depend_list;
	char *rtn_list;		/* expanded jobids appended here */
	int   rtn_size;		/* size of above 		 */
{
    char *at;
    int i = 0;
    int first = 1;
    char *s = NULL, *c;
    char full_job_id[PBS_MAXCLTJOBID+1];
    char server_out[PBS_MAXSERVERNAME + PBS_MAXPORTNUM + 2];

    /* Begin the parse */
    c = depend_list;

    /* Loop on strings between colons */
    while ( *c != '\0' ) {
	s = c;
        while (((*c != ':') || (*(c-1) == '\\'))  && *c ) c++;
	if ( s == c ) return 1;

	if ( *c == ':' ) {
		*c++ = '\0';
	}

	if (first) {
		first = 0;
		for (i=0; deptypes[i]; ++i) {
			if (strcmp(s, deptypes[i]) == 0)
				break;
		}
		if (deptypes[i] == (char *)0)
			return 1;
		(void)strcat(rtn_list, deptypes[i]);

	} else {

		if (i < 2) {		/* for "on" and "synccount", number */
			(void)strcat(rtn_list, s);
		} else {		/* for others, job id */
			at = strchr(s, (int)'@');
			if (get_server(s, full_job_id, server_out) != 0)
				return 1;
			(void)strcat(rtn_list, full_job_id);
			if (at) {
				(void)strcat(rtn_list, "@");
				(void)strcat(rtn_list, server_out);
			}
		}
	}
	if (*c)
		(void)strcat(rtn_list, ":");
    }
    if ( s == c ) return 1;

    return 0;
}



/*
 *
 * parse_depend_list
 *
 * syntax:
 *
 *	depend_list[,depend_list...]
 *
 * Arguments:
 *
 *	list		List of colon delimited lists.
 *
 */

int parse_depend_list(list, rtn_list, rtn_size)
	char *list;
	char *rtn_list;		/* expanded list returned here */
	int   rtn_size;		/* size of above array */

{
    char *b, *c, *s, *lc;
    int comma = 0;

    if ( strlen(list) == 0 ) return (1);

    if ( (lc = (char *)malloc(strlen(list)+1)) == (char *)0 ) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    strcpy(lc, list);
    c = lc;
    *rtn_list = '\0';

    while ( *c != '\0' ) {
	/* Drop leading white space */
        while ( isspace(*c) ) c++;

	/* Find the next comma */
        s = c;
        while ( *c != ',' && *c ) c++;

	/* Drop any trailing blanks */
        comma = (*c == ',');
        *c = '\0';
	b = c - 1;
	while ( isspace((int)*b) ) *b-- = '\0';

	
	/* Parse the individual list item */

        if ( parse_depend_item(s, rtn_list, rtn_size) ) {
            return 1;
        }

        if ( comma ) {
            c++;
	    (void)strcat(rtn_list, ",");
        }
    }
    free(lc);

    if ( comma ) return 1;

    return 0;
}
