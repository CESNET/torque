#include "license_pbs.h" /* See here for the software license */
#include <stdlib.h>
#include <stdio.h> /* fprintf */

#include "pbs_ifl.h" /* attrl */

int pbs_errno = 0;
char *pbs_server = NULL;

int pbs_disconnect(int connect)
  { 
  fprintf(stderr, "The call to pbs_disconnect needs to be mocked!!\n");
  exit(1);
  }

void set_attr(struct attrl **attrib, char *attrib_name, char *attrib_value)
  { 
  fprintf(stderr, "The call to set_attr needs to be mocked!!\n");
  exit(1);
  }

int check_job_name(char *name, int chk_alpha)
  { 
  fprintf(stderr, "The call to check_job_name needs to be mocked!!\n");
  exit(1);
  }

int parse_equal_string(char *start, char **name, char **value)
  { 
  fprintf(stderr, "The call to parse_equal_string needs to be mocked!!\n");
  exit(1);
  }

int locate_job(char *job_id, char *parent_server, char *located_server)
  { 
  fprintf(stderr, "The call to locate_job needs to be mocked!!\n");
  exit(1);
  }

int cnt2server(char *SpecServer)
  { 
  fprintf(stderr, "The call to cnt2server needs to be mocked!!\n");
  exit(1);
  }

int pbs_alterjob(int c, char *jobid, struct attrl *attrib, char *extend, int *any_failed)
  { 
  fprintf(stderr, "The call to pbs_alterjob needs to be mocked!!\n");
  exit(1);
  }

int prepare_path(char *path_in, char *path_out, char *host)
  { 
  fprintf(stderr, "The call to prepare_path needs to be mocked!!\n");
  exit(1);
  }

time_t cvtdate(char *datestr)
  { 
  fprintf(stderr, "The call to cvtdate needs to be mocked!!\n");
  exit(1);
  }

int parse_stage_list(char *list)
  { 
  fprintf(stderr, "The call to parse_stage_list needs to be mocked!!\n");
  exit(1);
  }

int get_server(char *job_id_in, char *job_id_out, char *server_out)
  { 
  fprintf(stderr, "The call to get_server needs to be mocked!!\n");
  exit(1);
  }

int parse_depend_list(char *list, char *rtn_list, int rtn_size)
  { 
  fprintf(stderr, "The call to parse_depend_list needs to be mocked!!\n");
  exit(1);
  }

void prt_job_err(char *cmd, int connect, char *id)
  { 
  fprintf(stderr, "The call to prt_job_err needs to be mocked!!\n");
  exit(1);
  }

char *pbs_strerror(int err)
  { 
  fprintf(stderr, "The call to pbs_strerror needs to be mocked!!\n");
  exit(1);
  }

int pbs_alterjob_async(int c, char *jobid, struct attrl *attrib, char *extend, int *any_failed)
  { 
  fprintf(stderr, "The call to pbs_alterjob_async needs to be mocked!!\n");
  exit(1);
  }

int set_resources(struct attrl **attrib, char *resources, int add)       
  { 
  fprintf(stderr, "The call to set_resources needs to be mocked!!\n");
  exit(1);
  }

int parse_at_list(char *list, int use_count, int abs_path)
  { 
  fprintf(stderr, "The call to parse_at_list needs to be mocked!!\n");
  exit(1);
  }

