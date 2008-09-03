/* job_qs_upgrade.c - 
 *
 *	The following public functions are provided:
 *		job_qs_upgrade()   - 
 */

#include <pbs_config.h>   /* the master config generated by configure */
 
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"
#include "array.h"

extern char *path_jobs;
extern char *path_arrays;

int upgrade_2_1_X(job *pj,int *fds);
int upgrade_2_2_X(job *pj,int *fds);


typedef struct {
    int	    ji_state;		/* internal copy of state */
    int	    ji_substate;	/* job sub-state */
    int	    ji_svrflags;	/* server flags */
    int	    ji_numattr;		/* number of attributes in list */
    int	    ji_ordering;	/* special scheduling ordering */
    int	    ji_priority;	/* internal priority */
    time_t  ji_stime;		/* time job started execution */
    char    ji_jobid[80];   /* job identifier */
    char    ji_fileprefix[12];  /* job file prefix */
    char    ji_queue[16];  /* name of current queue */
    char    ji_destin[87]; /* dest from qmove/route */
    int	    ji_un_type;		/* type of ji_un union */
    union {	/* depends on type of queue currently in */
	struct {	/* if in execution queue .. */
     	    pbs_net_t ji_momaddr;  /* host addr of Server */
	    int	      ji_exitstat; /* job exit status from MOM */
	} ji_exect;
	struct {
	    time_t  ji_quetime;		      /* time entered queue */
	    time_t  ji_rteretry;	      /* route retry time */
	} ji_routet;
	struct {
            pbs_net_t  ji_fromaddr;     /* host job coming from   */
	    int	       ji_fromsock;	/* socket job coming over */
	    int	       ji_scriptsz;	/* script size */
	} ji_newt;
	struct {
     	    pbs_net_t ji_svraddr;  /* host addr of Server */
	    int	      ji_exitstat; /* job exit status from MOM */
	    uid_t     ji_exuid;	   /* execution uid */
	    gid_t     ji_exgid;	   /* execution gid */
	} ji_momt;
    } ji_un;
} ji_qs_2_1_X;
 
typedef struct {
    int     qs_version;
    int     ji_state;           /* internal copy of state */
    int     ji_substate;        /* job sub-state */
    int     ji_svrflags;        /* server flags */
    int     ji_numattr;         /* number of attributes in list */
    int     ji_ordering;        /* special scheduling ordering */
    int     ji_priority;        /* internal priority */
    time_t  ji_stime;           /* time job started execution */
    char    ji_jobid[86];   /* job identifier */
    char    ji_fileprefix[12];  /* job file prefix */
    char    ji_queue[16];  /* name of current queue */
    char    ji_destin[87]; /* dest from qmove/route */
    int     ji_un_type;         /* type of ji_un union */
    union {     /* depends on type of queue currently in */
        struct {        /* if in execution queue .. */
            pbs_net_t ji_momaddr;  /* host addr of Server */
            int       ji_exitstat; /* job exit status from MOM */
        } ji_exect;
        struct {
            time_t  ji_quetime;               /* time entered queue */
            time_t  ji_rteretry;              /* route retry time */
        } ji_routet;
        struct {
            pbs_net_t  ji_fromaddr;     /* host job coming from   */
            int        ji_fromsock;     /* socket job coming over */
            int        ji_scriptsz;     /* script size */
        } ji_newt;
        struct {
            pbs_net_t ji_svraddr;  /* host addr of Server */
            int       ji_exitstat; /* job exit status from MOM */
            uid_t     ji_exuid;    /* execution uid */
            gid_t     ji_exgid;    /* execution gid */
        } ji_momt;
    } ji_un;
} ji_qs_2_2_X;


/* this function will upgrade a ji_qs struct to the
   newest version.   
   
   this version upgrades from 2.1.x and 2.2.x to 2.3.0
   */

int job_qs_upgrade (

  job *pj,     /* I */
  int *fds,    /* I */
  int version) /* I */

  {


  /* reset the file descriptor */
  if (lseek(*fds, 0, SEEK_SET) != 0)
    {
    sprintf(log_buffer, "unable to reset fds\n");
    log_err(-1,"job_qs_upgrade",log_buffer);

    return (-1);
    }

  if (version == 0x00020200) 
    {
    return  upgrade_2_2_X(pj, fds);
    }
  else
    {
    return upgrade_2_1_X(pj, fds);
    }
    
    
  }

int upgrade_2_2_X (

  job *pj,   /* I */
  int *fds)  /* I */

{
  ji_qs_2_2_X qs_old;
  char basename[PBS_JOBBASE + 1];
/*
  char namebuf1[MAXPATHLEN];
  char namebuf2[MAXPATHLEN];
*/

#ifndef PBS_MOM
  char range[PBS_MAXJOBARRAYLEN*2+2];
  char parent_id[PBS_MAXSVRJOBID + 1];
  attribute tempattr;
  job_array *pa;
#endif

  basename[PBS_JOBBASE] = '\0';
 
  if (read(*fds, (char*)&qs_old, sizeof(qs_old)) != sizeof(qs_old))
    {
    return (-1);
    }

  pj->ji_qs.qs_version  = PBS_QS_VERSION;
  pj->ji_qs.ji_state    = qs_old.ji_state;
  pj->ji_qs.ji_substate = qs_old.ji_substate;
  pj->ji_qs.ji_svrflags = qs_old.ji_svrflags;
  pj->ji_qs.ji_numattr  = qs_old.ji_numattr;  
  pj->ji_qs.ji_ordering = qs_old.ji_ordering;
  pj->ji_qs.ji_priority = qs_old.ji_priority;
  pj->ji_qs.ji_stime    = qs_old.ji_stime;
  
  strcpy(pj->ji_qs.ji_jobid, qs_old.ji_jobid);
/*  removed renaming files for recovered jobs...
  strncpy(pj->ji_qs.ji_fileprefix, pj->ji_qs.ji_jobid, PBS_JOBBASE);  
*/
  strcpy(pj->ji_qs.ji_fileprefix, qs_old.ji_fileprefix);
  strcpy(pj->ji_qs.ji_queue, qs_old.ji_queue);
  strcpy(pj->ji_qs.ji_destin, qs_old.ji_destin);


  
  pj->ji_qs.ji_un_type  = qs_old.ji_un_type;

  memcpy(&pj->ji_qs.ji_un, &qs_old.ji_un, sizeof(qs_old.ji_un));


#ifndef PBS_MOM
  array_get_parent_id(pj->ji_qs.ji_jobid, parent_id);
  if (is_array(parent_id))
    {
  
    pa = get_array(parent_id);

    if (pa == NULL)
      {
      return(1);
      }

    sprintf(range,"0-%d",pa->ai_qs.array_size-1);
 
    clear_attr(&tempattr,&job_attr_def[JOB_ATR_job_array_request]);
    job_attr_def[JOB_ATR_job_array_request].at_decode(
       &tempattr,
       NULL,
       NULL,
       range);
    job_attr_def[JOB_ATR_job_array_request].at_set(
       &(pj->ji_wattr[JOB_ATR_job_array_request]),
       &tempattr,
       SET);
    }
#endif

  /* removed code to rename files using new larger PBS_JOBBASE, 
     TODO GB - if we decide to keep this code out clean up anything else 
     relaited to renaming files */
  
  return (0);
} 

int upgrade_2_1_X (

  job *pj,  /* I */
  int *fds) /* I */

  {
  ji_qs_2_1_X qs_old;


  if (read(*fds, (char*)&qs_old, sizeof(qs_old)) != sizeof(qs_old))
    {
    return (-1);
    }

  pj->ji_qs.qs_version  = PBS_QS_VERSION;
  pj->ji_qs.ji_state    = qs_old.ji_state;
  pj->ji_qs.ji_substate = qs_old.ji_substate;
  pj->ji_qs.ji_svrflags = qs_old.ji_svrflags;
  pj->ji_qs.ji_numattr  = qs_old.ji_numattr;
  pj->ji_qs.ji_ordering = qs_old.ji_ordering;
  pj->ji_qs.ji_priority = qs_old.ji_priority;
  pj->ji_qs.ji_stime    = qs_old.ji_stime;

  strcpy(pj->ji_qs.ji_jobid, qs_old.ji_jobid);
  strcpy(pj->ji_qs.ji_fileprefix, qs_old.ji_fileprefix);
  strcpy(pj->ji_qs.ji_queue, qs_old.ji_queue);
  strcpy(pj->ji_qs.ji_destin, qs_old.ji_destin);

  pj->ji_qs.ji_un_type  = qs_old.ji_un_type;

  memcpy(&pj->ji_qs.ji_un, &qs_old.ji_un, sizeof(qs_old.ji_un));

  return (0);
  }

