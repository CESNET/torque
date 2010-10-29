#ifndef SITE_PBS_CACHE_COMMON_H_
#define SITE_PBS_CACHE_COMMON_H_

#include <time.h>

#define CHECK_OK 1
#define CHECK_NO 0
#define CHECK_USER 2
#define CHECK_DATA 3


typedef struct  repository_alternatives{
  char     *r_name;
  char   *r_proplist;
  int   r_mark;
} repository_alternatives;

/* function for reading config files */
typedef struct {
        char *key;
        char *first;
        char *second;
} config_line_s;


int check_and_read_config(char *filename,config_line_s **lines,time_t *last_modification,int *last_line);
void store_cluster_attr(char **metric, char *name, char *value);
void retrieve_cluster_attr(char *metric, char *name, char **value);

int is_users_in_group(char *group, char *user);

#endif /* SITE_PBS_CACHE_COMMON_H_ */
