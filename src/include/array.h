#ifndef ARRAY_H
#define ARRAY_H



/* these are required if you include array.h */
#include "pbs_ifl.h"
#include "log.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "pbs_error.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif


#define ARRAY_FILE_SUFFIX ".AR"

typedef struct
  {
  list_link request_tokens_link;

  int start;
  int end;
  } array_request_node;



#define ARRAY_QS_STRUCT_VERSION 2

/* pbs_server will keep a list of these structs, with one struct per job array*/

struct job_array
  {
  list_link all_arrays;      /* node in server's linked list of all arrays */
  tlist_head request_tokens; /* head of linked list of request tokens, used 
                                during cloning */

  void **jobs; /* a pointer to the job pointers in this array */

  int jobs_recovered; /* on server restart we track the number of array tasks
                         that have been recovered */

  /* this info is saved in the array file */
  struct array_info
    {
    int  struct_version;
    int  array_size;
    int  num_jobs;
    int  jobs_done; /* number of jobs that have been deleted, etc. */
    int  num_cloned;
    /* max user name, server name, 1 for the @, and one for the NULL */
    char owner[PBS_MAXUSER + PBS_MAXSERVERNAME + 2];
    char parent_id[PBS_MAXSVRJOBID + 1];
    char fileprefix[PBS_JOBBASE + 1];
    char submit_host[PBS_MAXSERVERNAME +1];
    } ai_qs;
  };

typedef struct job_array job_array;

int  is_array(char *id);
int  array_delete(job_array *pa);
int  array_save(job_array *);
int  array_save(job_array *pa);
void array_get_parent_id(char *job_id, char *parent_id);

job_array *get_array(char *id);
job_array *array_recov(char *path);


#endif
