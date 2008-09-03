/* this file contains functions for manipulating job arrays

  included functions:

  is_array() determine if jobnum is actually an array identifyer
  get_array() return array struct for given "parent id"
  array_save() save array struct to disk
  array_get_parent_id() return id of parent job if job belongs to a job array
  recover_array_struct() recover the array struct for a job array at restart
  delete_array_struct() free memory used by struct and delete sved struct on disk
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* this macro is for systems like BSD4 that do not have O_SYNC in fcntl.h,
 * but do have O_FSYNC! */

#ifndef O_SYNC
#define O_SYNC O_FSYNC
#endif /* !O_SYNC */

#include <unistd.h>


#include "pbs_ifl.h"
#include "log.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"
#include "pbs_error.h"
#include "svrfunc.h"

#include "work_task.h"

#include "array.h"




extern void  job_clone_wt A_((struct work_task *));
extern int array_upgrade(job_array *pa, int *fds, int version, int *old_version);

/* global data items used */

/* list of job arrays */
extern tlist_head svr_jobarrays;
extern char *path_arrays;
extern char *path_jobs;
extern time_t time_now;
extern int    LOGLEVEL;
extern char *pbs_o_host;


static int is_num(char *);
static int array_request_token_count(char *);
static int array_request_parse_token(char *, int *, int *);
static int parse_array_request(char *request, job_array *pa);



/* search job array list to determine if id is a job array */
int is_array(char *id)
  {

  job_array *pa;
  
  
    
  pa = (job_array*)GET_NEXT(svr_jobarrays);  
  while (pa != NULL)
    {
    if (strcmp(pa->ai_qs.parent_id, id) == 0)
      {
      return TRUE;
      }
    pa = (job_array*)GET_NEXT(pa->all_arrays);
    }
  
  return FALSE;
  }
  
/* return a server's array info struct corresponding to an array id */
job_array *get_array(char *id)
  {
  job_array *pa;
   
    
  pa = (job_array*)GET_NEXT(svr_jobarrays);  
  while (pa != NULL)
    {
    if (strcmp(pa->ai_qs.parent_id, id) == 0)
      {
      return pa;
      }
    if (pa == GET_NEXT(pa->all_arrays))
      {
      pa = NULL;
      }
    else
      {
      pa = (job_array*)GET_NEXT(pa->all_arrays);
      }
    }
    
  return (job_array*)NULL;
  }
  
  
/* save a job array struct to disk returns zero if no errors*/  
int array_save(job_array *pa)

  { 
  

  int fds;
  char namebuf[MAXPATHLEN];
  array_request_node *rn;
  int num_tokens = 0;

  strcpy(namebuf, path_arrays);
  strcat(namebuf, pa->ai_qs.fileprefix);
  strcat(namebuf, ARRAY_FILE_SUFFIX);

  fds = open(namebuf,O_SYNC|O_TRUNC|O_WRONLY|O_CREAT,0600);
  
  if (fds < 0)
    {
    return -1;
    }
    
 write(fds,  &(pa->ai_qs), sizeof(struct array_info));
  
  /* count number of request tokens left */
  num_tokens = 0;
  rn = (array_request_node*)GET_NEXT(pa->request_tokens);
  
  while (rn != NULL) 
    {
    num_tokens++;
    rn = (array_request_node*)GET_NEXT(rn->request_tokens_link);
    }
     
     
  write(fds, &num_tokens, sizeof(num_tokens));
     
  if (num_tokens > 0)
    {

    rn = (array_request_node*)GET_NEXT(pa->request_tokens);
  
    while (rn != NULL) 
      {
      write(fds, rn, sizeof(array_request_node));
      rn = (array_request_node*)GET_NEXT(rn->request_tokens_link);
      }
          
    }
    
  close(fds);
  return 0;
  }
  
  
/* if a job belongs to an array, this will return the id of the parent job
 * this assumes that the caller allocates the storage for the parent id 
 * and that this will only be called on jobs that actually have or had
 *  a parent job 
 */  
void array_get_parent_id(char *job_id, char *parent_id)
  {
  char *c;
  char *pid;
  
  c = job_id;
  *parent_id = '\0';
  pid = parent_id;
  
  /* copy until the '-' */
  while (*c != '-' && *c != '\0')
    {
    *pid = *c;
    c++;
    pid++;
    } 
    
  /* skip the until the first '.' */
  while (*c != '.' && *c != '\0')
    {
    c++;
    }
    
  /* copy the rest of the id */
  *pid = '\0';
  strcat(pid, c);


  }  



/* recover_array_struct reads in  an array struct saved to disk and inserts it into 
   the servers list of arrays */
job_array *recover_array_struct(char *path)
{
   extern tlist_head svr_jobarrays;
   job_array *pa;
   array_request_node *rn;
   int fd;
   int old_version;
   int num_tokens;
   int i;
   
   old_version = ARRAY_QS_STRUCT_VERSION;
   /* allocate the storage for the struct */
   pa = (job_array*)malloc(sizeof(job_array));
   
   if (pa == NULL)
     {
     return NULL;
     }
    
   /* initialize the linked list nodes */  
   CLEAR_LINK(pa->all_arrays);
   CLEAR_HEAD(pa->array_alljobs);
   CLEAR_HEAD(pa->request_tokens);
	 
   fd = open(path, O_RDONLY,0);
   
   /* read the file into the struct previously allocated. */

   if ((read(fd, &(pa->ai_qs), sizeof(pa->ai_qs)) != sizeof(pa->ai_qs) && 
        pa->ai_qs.struct_version == ARRAY_QS_STRUCT_VERSION) || 
       (pa->ai_qs.struct_version != ARRAY_QS_STRUCT_VERSION && 
        array_upgrade(pa, &fd, pa->ai_qs.struct_version, &old_version) != 0))
     {
     sprintf(log_buffer,"unable to read %s", path);

     log_err(errno,"pbsd_init",log_buffer);

     free(pa);
     close(fd);
     return NULL;
     }

   /* check to see if there is any additional info saved in the array file */   
   /* check if there are any array request tokens that haven't been fully
   processed */
   
   if (old_version > 1)
     {
     if (read(fd, &num_tokens, sizeof(int)) != sizeof(int))
       {
       sprintf(log_buffer, "error reading token count from %s", path);
       log_err(errno, "pbsd_init", log_buffer);
       
       free(pa);
       close(fd);
       return NULL;
       }
     
       for (i = 0; i < num_tokens; i++)
         {
	 rn = (array_request_node*)malloc(sizeof(array_request_node));
	 
	 if (read(fd, rn, sizeof(array_request_node)) != sizeof(array_request_node))
           {
           
	   sprintf(log_buffer, "error reading array_request_node from %s", path);
           log_err(errno, "pbsd_init", log_buffer);
           
	   free(rn);
	   
	   rn = (array_request_node*)GET_NEXT(pa->request_tokens);
  
           while (rn != NULL) 
             {
             delete_link(&rn->request_tokens_link);
             free(rn);
             rn = (array_request_node*)GET_NEXT(pa->request_tokens);
             }
           free(pa);
           close(fd);
           return NULL;
           }
	   
	   CLEAR_LINK(rn->request_tokens_link);
	   append_link(&pa->request_tokens,&rn->request_tokens_link,(void*)rn);
	 
	 }
       
         
     
     }
  
   
   
     
   close(fd);

   if (old_version != ARRAY_QS_STRUCT_VERSION)
     {
     array_save(pa);
     }
	 
   /* link the struct into the servers list of job arrays */ 
   append_link(&svr_jobarrays, &pa->all_arrays, (void*)pa);
   
   return pa;

}


/* delete a job array struct from memory and disk. This is used when the number
 *  of jobs that belong to the array becomes zero.
 *  returns zero if there are no errors, non-zero otherwise
 */
int delete_array_struct(job_array *pa)
  {
 
  char path[MAXPATHLEN + 1];
  array_request_node *rn;
 
  
  /* first thing to do is take this out of the servers list of all arrays */
  delete_link(&pa->all_arrays);
  
  strcpy(path, path_arrays);
  strcat(path, pa->ai_qs.fileprefix);
  strcat(path, ARRAY_FILE_SUFFIX);
  
 
  
  /* delete the on disk copy of the struct */
  if (unlink(path))
    {
    sprintf(log_buffer, "unable to delete %s", path);
    log_err(errno, "delete_array_struct", log_buffer);
    }
    
  strcpy(path,path_jobs);	/* delete script file */
  strcat(path,pa->ai_qs.fileprefix);
  strcat(path,JOB_SCRIPT_SUFFIX);

  if (unlink(path) < 0)
    {
    sprintf(log_buffer, "unable to delete %s", path);
    log_err(errno, "delete_array_struct", log_buffer);
    }
    
  else if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer,"removed job script");

    log_record(PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pa->ai_qs.parent_id,
      log_buffer);
    }
    
  /* clear array request linked list */
   
  rn = (array_request_node*)GET_NEXT(pa->request_tokens);
  
  while (rn != NULL) 
    {
    delete_link(&rn->request_tokens_link);
    free(rn);
    rn = (array_request_node*)GET_NEXT(pa->request_tokens);
    }


  /* free the memory allocated for the struct */
  free(pa);  
  return 0;
  }
  
  
int setup_array_struct(job *pjob)
  {
  job_array *pa;
  struct work_task *wt;
  int bad_token_count;
  
  
  /* setup a link to this job array in the servers all_arrays list */
  pa = (job_array*)malloc(sizeof(job_array));
    
  pa->ai_qs.struct_version = ARRAY_QS_STRUCT_VERSION;
  
  /*pa->ai_qs.array_size = pjob->ji_wattr[(int)JOB_ATR_job_array_size].at_val.at_long;*/
  
  strcpy(pa->ai_qs.parent_id, pjob->ji_qs.ji_jobid);
  strcpy(pa->ai_qs.fileprefix, pjob->ji_qs.ji_fileprefix);
  strncpy(pa->ai_qs.owner, pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str, PBS_MAXUSER + PBS_MAXSERVERNAME + 2);
  strncpy(pa->ai_qs.submit_host, get_variable(pjob,pbs_o_host), PBS_MAXSERVERNAME);
  
  pa->ai_qs.num_cloned = 0;
  CLEAR_LINK(pa->all_arrays);
  CLEAR_HEAD(pa->array_alljobs);
  CLEAR_HEAD(pa->request_tokens);
  append_link(&svr_jobarrays, &pa->all_arrays, (void*)pa);
    
  if (job_save(pjob,SAVEJOB_FULL) != 0) 
    {
    job_purge(pjob);

    if (LOGLEVEL >= 6)
      {
      log_record(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          (pjob != NULL) ? pjob->ji_qs.ji_jobid : "NULL",
          "cannot save job");
      }

    return 1;
    }
    
  bad_token_count =
     parse_array_request(pjob->ji_wattr[(int)JOB_ATR_job_array_request].at_val.at_str, pa);

  array_save(pa);

  if (bad_token_count > 0)
    {
    job_purge(pjob);
    delete_array_struct(pa);
    return 2;
    }			
    
  wt = set_task(WORK_Timed,time_now+1,job_clone_wt,(void*)pjob);
  /* svr_setjobstate(pj,JOB_STATE_HELD,JOB_SUBSTATE_HELD);*/
    
  return 0;
  	
  }


static int is_num(char *str)
{
   int i;
   int len;

   len = strlen(str);
   if (len == 0)
     {
     return 0;
     }

   for (i = 0; i < len; i++)
     {
     if (str[i] < '0' || str[i] > '9')
       {
       return 0;
       }
     }

  return 1;
}

int array_request_token_count(char *str)
{
  int token_count;
  int len;
  int i;


  len = strlen(str);

  token_count = 1;

  for (i = 0; i < len; i++)
    {
    if (str[i] == ',')
      {
      token_count++;
      }
    }

  return token_count;

}

static int array_request_parse_token(char *str, int *start, int *end)
{
  int num_ids;
  long start_l;
  long end_l;
  char *idx;
  char *ridx;


  idx = index(str, '-');
  ridx = rindex(str, '-');
  
  if (idx == NULL)
    {
    if (!is_num(str))
      {
      start_l = -1;
      end_l = -1;
      }
    else
      {
      start_l = strtol(str, NULL, 10);
      end_l = start_l;
      }
    }
  else if (idx == ridx)
    {
    *idx = '\0';
    idx++;
    if (!is_num(str) || !is_num(idx))
      {
      start_l = -1;
      end_l = -1;
      }
    else
      {
      start_l = strtol(str, NULL, 10);
      end_l = strtol(idx, NULL, 10);
      }
    }
  else 
    {
    start_l = -1;
    end_l = -1;
    }

  /* restore the string */
  if (idx != NULL)
    {
    idx--;
    *idx = '-';
    }

  if (start_l < 0 || start_l >= INT_MAX || end_l < 0 || end_l >= INT_MAX 
      || start_l > PBS_MAXJOBARRAY || end_l > PBS_MAXJOBARRAY || end_l < start_l)
    {
    *start = -1;
    *end = -1;
    num_ids = 0;
    }
  else
    {
    num_ids = end_l - start_l + 1;
    *start = (int)start_l;
    *end   = (int)end_l;
    }

  return num_ids; 
}


static int parse_array_request(char *request, job_array *pa)
{
   char *temp_str;
   int num_tokens;
   char **tokens;
   int i;
   int j;
   int num_elements;
   int start;
   int end;
   int num_bad_tokens;
   int searching;
   array_request_node *rn;
   array_request_node *rn2;

   pa->ai_qs.array_size = 0;

   temp_str = strdup(request);
   num_tokens = array_request_token_count(request);
   num_bad_tokens = 0;
   
   tokens = (char**)malloc(sizeof(char*) * num_tokens);
   j = num_tokens - 1;
   /* start from back and scan backwards setting pointers to tokens and changing ',' to '\0' */
   for (i = strlen(temp_str) - 1; i >= 0; i--)
     {

     if (temp_str[i] == ',')
       {
       tokens[j--] = &temp_str[i+1];
       temp_str[i] = '\0';
       }
     else if (i == 0)
       {
       tokens[0] = temp_str;
       }
     }

   for (i = 0; i < num_tokens; i++)
     {
     num_elements = array_request_parse_token(tokens[i], &start, &end);
     if (num_elements == 0)
       {
       num_bad_tokens++;
       }
     else 
       {
       rn = (array_request_node*)malloc(sizeof(array_request_node));
       rn->start = start;
       rn->end = end;
       CLEAR_LINK(rn->request_tokens_link);
       
       rn2 = GET_NEXT(pa->request_tokens);
       searching = TRUE;
         
       while (searching)
         {
          
         if (rn2 == NULL)
           {
           append_link(&pa->request_tokens, &rn->request_tokens_link, (void*)rn);
           searching = FALSE;
           }
         else if (rn->start < rn2->start)
           {
           insert_link(&rn2->request_tokens_link, &rn->request_tokens_link, (void*)rn,
                       LINK_INSET_BEFORE);
           searching = FALSE;
           }
         else
           {
           rn2 = GET_NEXT(rn2->request_tokens_link);
           }
          
         }

       rn2 = GET_PRIOR(rn->request_tokens_link);
       if (rn2 != NULL && rn2->end >= rn->start)
         {
         num_bad_tokens++;
         }
       
       rn2 = GET_NEXT(rn->request_tokens_link);
       if (rn2 != NULL && rn2->start <= rn->end)
         {
         num_bad_tokens++;
         }
       pa->ai_qs.array_size += end - start + 1;
       }
     }   

  free (tokens);
  free (temp_str);

  return num_bad_tokens;
}

