/*
 * site_pbs_cache.h - header file for magrathea, pbs_cache and image repository functions
 *
 */

/* function for repository of images */


#ifndef SITE_PBS_CACHE_H_
#define SITE_PBS_CACHE_H_

#define CHECK_OK 1
#define CHECK_NO 0
#define CHECK_USER 2
#define CHECK_DATA 3


struct  repository_alternatives{
	char     *r_name;
	char	 *r_proplist;
	int 	r_mark;
};

struct  repository_alternatives **get_bootable_alternatives(char *hostname,char *properties);
struct repository_alternatives **dup_bootable_alternatives(struct repository_alternatives **old);
void free_bootable_alternatives(struct  repository_alternatives **list);
int  alternative_has_property(struct  repository_alternatives *a, char *prop);
struct repository_alternatives *find_alternative_with_property(struct  repository_alternatives **list,char *prop);
char *get_alternative_properties(char *name);

/* function for magrathea */
int magrathea_decode(resource *res,long int *status,long int *count,long int *used,long int *m_free,long int *m_possible);


/* function for reading config files */
typedef struct {
        char *key;
        char *first;
        char *second;
} config_line_s;

int check_and_read_config(char *filename,config_line_s **lines,time_t *last_modification,int *last_line);

/* function for account test */ 
int site_user_has_account(char *user, char *host, char *cluster);

#endif
