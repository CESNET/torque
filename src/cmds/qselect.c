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
 *
 * qselect - (PBS) select batch job
 *
 * Authors:
 *      Terry Heidelberg
 *      Livermore Computing
 *
 *      Bruce Kelly
 *      National Energy Research Supercomputer Center
 *
 *      Lawrence Livermore National Laboratory
 *      University of California
 */

#include "cmds.h"
#include <pbs_config.h>   /* the master config generated by configure */

static char ident[] = "@(#) $RCSfile$ $Revision$";

void 
set_attrop(list, a_name, r_name, v_name, op)
struct attropl **list;
char *a_name;
char *r_name;
char *v_name;
enum batch_op op;
{
    struct attropl *attr;

    attr = (struct attropl *) malloc(sizeof(struct attropl));
    if ( attr == NULL ) {
        fprintf(stderr, "qselect: out of memory\n");
        exit(2);
    }
    if ( a_name == NULL )
        attr->name = NULL;
    else {
        attr->name = (char *) malloc(strlen(a_name)+1);
        if ( attr->name == NULL ) {
            fprintf(stderr, "qselect: out of memory\n");
            exit(2);
        }
        strcpy(attr->name, a_name);
    }
    if ( r_name == NULL )
        attr->resource = NULL;
    else {
        attr->resource = (char *) malloc(strlen(r_name)+1);
        if ( attr->resource == NULL ) {
            fprintf(stderr, "qselect: out of memory\n");
            exit(2);
        }
        strcpy(attr->resource, r_name);
    }
    if ( v_name == NULL )
        attr->value = NULL;
    else {
        attr->value = (char *) malloc(strlen(v_name)+1);
        if ( attr->value == NULL ) {
            fprintf(stderr, "qselect: out of memory\n");
            exit(2);
        }
        strcpy(attr->value, v_name);
    }
    attr->op = op;
    attr->next = *list;
    *list = attr;
    return;
}



#define OPSTRING_LEN 4
#define OP_LEN 2
#define OP_ENUM_LEN 6
static char *opstring_vals[] = { "eq", "ne", "ge", "gt", "le", "lt" };
static enum batch_op opstring_enums[] = { EQ, NE, GE, GT, LE, LT };



void 
check_op(optarg, op, optargout)
char *optarg;
enum batch_op *op;
char *optargout;
{
    char opstring[OP_LEN+1];
    int i;
    int cp_pos;

    *op = EQ;   /* default */
    cp_pos = 0;

    if ( optarg[0] == '.' ) {
        strncpy(opstring, &optarg[1], OP_LEN);          
        opstring[OP_LEN] = '\0';
        cp_pos = OPSTRING_LEN;
        for ( i=0; i<OP_ENUM_LEN; i++) {
            if ( strncmp(opstring, opstring_vals[i], OP_LEN) == 0 ) {
                *op = opstring_enums[i];                        
                break;
            }
        }
    }
    strcpy(optargout, &optarg[cp_pos]);
    return;
}



int
check_res_op(optarg, resource_name, op, resource_value, res_pos)
char *optarg;
char *resource_name;
enum batch_op *op;
char *resource_value;
char **res_pos;
{
    char opstring[OPSTRING_LEN];
    int i;
    int hit;
    char *p;

    p = strchr(optarg, '.');
    if ( p == NULL || *p == '\0' ) {
        fprintf(stderr, "qselect: illegal -l value\n");
        fprintf(stderr, "resource_list: %s\n", optarg);
        return (1);
    }
    else {
        strncpy(resource_name, optarg, p-optarg);
        resource_name[p-optarg] = '\0';
        *res_pos = p + OPSTRING_LEN;
    }
    if ( p[0] == '.' ) {
        strncpy(opstring, &p[1] , OP_LEN);              
        opstring[OP_LEN] = '\0';
        hit = 0;
        for ( i=0; i<OP_ENUM_LEN; i++) {
            if ( strncmp(opstring, opstring_vals[i], OP_LEN) == 0 ) {
                *op = opstring_enums[i];                        
                hit = 1;
                break;
            }
        }
        if ( ! hit ) {
            fprintf(stderr, "qselect: illegal -l value\n");
            fprintf(stderr, "resource_list: %s\n", optarg);
            return (1);
        }
    }
    p = strchr(*res_pos, ',');
    if ( p == NULL ) {
        p = strchr(*res_pos, '\0');
    }
    strncpy(resource_value, *res_pos, p-(*res_pos));
    resource_value[p-(*res_pos)] = '\0';
    if ( strlen(resource_value) == 0 ) {
        fprintf(stderr, "qselect: illegal -l value\n");
        fprintf(stderr, "resource_list: %s\n", optarg);
        return (1);
    }
    *res_pos =  (*p == '\0') ? p : ( p += 1 ) ;
    if ( **res_pos == '\0' && *(p-1) == ',' ) {
        fprintf(stderr, "qselect: illegal -l value\n");
        fprintf(stderr, "resource_list: %s\n", optarg);
        return (1);
    }

    return(0);  /* ok */
}


/* qselect */

int main(

  int    argc,
  char **argv,
  char **envp)

  {
  int c;
  int errflg=0;
  char *errmsg;

#define MAX_OPTARG_LEN 256
#define MAX_RESOURCE_NAME_LEN 256
    char optargout[MAX_OPTARG_LEN+1];
    char resource_name[MAX_RESOURCE_NAME_LEN+1];

    enum batch_op op;
    enum batch_op *pop = &op;

    struct attropl *select_list = 0;

    static char destination[PBS_MAXQUEUENAME+1] = "";
    char server_out[MAXSERVERNAME] = "";

    char *queue_name_out;
    char *server_name_out;

    int connect;
    char **selectjob_list;
    char *res_pos;
    char *pc;
    int u_cnt, o_cnt, s_cnt, n_cnt;
    time_t after;
    char a_value[80];
    int exec_only = 0;

    if (getenv("PBS_QSTAT_EXECONLY") != NULL)
      exec_only = 1;

#define GETOPT_ARGS "a:A:ec:h:l:N:p:q:r:s:u:"
         
    while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    switch (c) {
    case 'a':
        check_op(optarg, pop, optargout); 
        if ((after = cvtdate(optargout)) < 0) 
          {
          fprintf(stderr, "qselect: illegal -a value\n");
          errflg++;
	  break;
          }

        sprintf(a_value, "%ld", (long)after);
        set_attrop(&select_list, ATTR_a, NULL, a_value, op);
        break;
    case 'e':
        exec_only = 1;
        break;
    case 'c':
        check_op(optarg, pop, optargout); 
	pc = optargout;
        while ( isspace((int)*pc) ) pc++;
	if ( strlen(pc) == 0 ) {
	    fprintf(stderr, "qselect: illegal -c value\n");
	    errflg++;
	    break;
	}
	if ( strcmp(pc, "u") == 0 ) {
	    if ( (op != EQ) && (op != NE) ) {
		fprintf(stderr, "qselect: illegal -c value\n");
		errflg++;
		break;
	    }
	} else if ( (strcmp(pc, "n") != 0) &&
		    (strcmp(pc, "s") != 0) &&
		    (strcmp(pc, "c") != 0) ) {
	    if ( strncmp(pc, "c=", 2) != 0 ) {
		fprintf(stderr, "qselect: illegal -c value\n");
		errflg++;
		break;
	    }
	    pc += 2;
	    if ( strlen(pc) == 0 ) {
                fprintf(stderr, "qselect: illegal -c value\n");
                errflg++;
		break;
            }
	    while ( *pc != '\0' ) {
		if ( !isdigit((int)*pc) ) {
                    fprintf(stderr, "qselect: illegal -c value\n");
                    errflg++;
		    break;
                }
		pc++;
            }
	}
        set_attrop(&select_list, ATTR_c, NULL, optargout, op);
        break;
    case 'h':
        check_op(optarg, pop, optargout); 
        pc = optargout;
        while ( isspace((int)*pc) ) pc++;
        if ( strlen(pc) == 0 ) {
            fprintf(stderr, "qselect: illegal -h value\n");
            errflg++;
	    break;
        }
        u_cnt = o_cnt = s_cnt = n_cnt = 0;
        while ( *pc) {
            if ( *pc == 'u' )
                u_cnt++;
            else if ( *pc == 'o' )
                o_cnt++;
            else if ( *pc == 's' )
                s_cnt++;
            else if ( *pc == 'n' )
                n_cnt++;
            else {
                fprintf(stderr, "qselect: illegal -h value\n");
                errflg++;
		break;
            }
            pc++;
        }
        if ( n_cnt && (u_cnt + o_cnt + s_cnt) ) {
            fprintf(stderr, "qselect: illegal -h value\n");
            errflg++;
	    break;
        }
        set_attrop(&select_list, ATTR_h, NULL, optargout, op);
        break;
    case 'l':
        res_pos = optarg;
        while ( *res_pos != '\0' ) {
            if (check_res_op(res_pos, resource_name, pop, optargout, &res_pos) != 0) {
		errflg++;
		break;
	    }
            set_attrop(&select_list, ATTR_l, resource_name, optargout, op);
        }
        break;
    case 'p':
        check_op(optarg, pop, optargout); 
        set_attrop(&select_list, ATTR_p, NULL, optargout, op);
        break;
    case 'q':
        strcpy(destination, optarg);
        check_op(optarg, pop, optargout); 
        set_attrop(&select_list, ATTR_q, NULL, optargout, op);
        break;
    case 'r':
        op = EQ;
	pc = optarg;
	while ( isspace((int)(*pc)) ) pc++;
        if ( strlen(pc) != 1 ) {
            fprintf(stderr, "qsub: illegal -r value\n");
            errflg++;
	    break;
        }
        if ( *pc != 'y' && *pc != 'n' ) {
            fprintf(stderr, "qsub: illegal -r value\n");
            errflg++;
	    break;
        }
        set_attrop(&select_list, ATTR_r, NULL, pc, op);
        break;
    case 's':
        check_op(optarg, pop, optargout); 
	pc = optargout;
	while ( isspace((int)(*pc)) ) pc++;
	if ( strlen(optarg) == 0 ) {
	    fprintf(stderr, "qselect: illegal -s value\n");
	    errflg++;
	    break;
	}
	while ( *pc ) {
	    if ( *pc != 'E' && *pc != 'H' && *pc != 'Q' &&
		 *pc != 'R' && *pc != 'T' && *pc != 'W' ) {
		fprintf(stderr, "qselect: illegal -s value\n");
		errflg++;
		break;
	    }
	    pc++;
	}
        set_attrop(&select_list, ATTR_state, NULL, optargout, op);
        break;
    case 'u':
	op = EQ;
        if ( parse_at_list(optarg, FALSE, FALSE) ) {
	    fprintf(stderr, "qselect: illegal -u value\n");
	    errflg++;
	    break;
        }
        set_attrop(&select_list, ATTR_u, NULL, optarg, op);
        break;
    case 'A':
        op = EQ;
        set_attrop(&select_list, ATTR_A, NULL, optarg, op);
        break;
    case 'N':
        op = EQ;
        set_attrop(&select_list, ATTR_N, NULL, optarg, op);
        break;
    default :
        errflg++;
    }

    if (errflg || (optind < argc)) {
        static char usage[]="usage: qselect \
[-a [op]date_time] [-A account_string] [-e] [-c [op]interval] \n\
[-h hold_list] [-l resource_list] [-N name] [-p [op]priority] \n\
[-q destination] [-r y|n] [-s states] [-u user_name]\n";
        fprintf(stderr, usage);
        exit (2);
    }

    if ( notNULL(destination) ) {
        if (parse_destination_id(destination,&queue_name_out,&server_name_out)) {
	    fprintf(stderr, "qselect: illegally formed destination: %s\n", destination);
            exit(2);
        } else {
            if ( notNULL(server_name_out) ) {
                strcpy(server_out, server_name_out);
            }
        }
    }

    connect = cnt2server(server_out);
    if ( connect <= 0 ) {
        fprintf(stderr, "qselect: cannot connect to server %s (errno=%d)\n",
                pbs_server, pbs_errno);
        exit(pbs_errno);
    }
    selectjob_list = pbs_selectjob(connect, select_list, exec_only ? EXECQUEONLY : NULL);
    if ( selectjob_list == NULL ) {
        if ( pbs_errno != PBSE_NONE ) {
            errmsg = pbs_geterrmsg(connect);
            if ( errmsg != NULL ) {
                fprintf(stderr, "qselect: %s\n", errmsg);
            } else {
                fprintf(stderr, "qselect: Error (%d) selecting jobs\n", pbs_errno);
            }
            exit(pbs_errno);
	}
    } else {   /* got some jobs ids */
        int i = 0;
        while ( selectjob_list[i] != NULL ) {
            printf("%s\n", selectjob_list[i++]);
        }
        free(selectjob_list);
    }
    pbs_disconnect(connect);

    exit(0);
}
