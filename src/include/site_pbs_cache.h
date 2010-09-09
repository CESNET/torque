/*
 * site_pbs_cache.h - header file for magrathea, pbs_cache and image repository functions
 *
 */

/* function for repository of images */


#ifndef SITE_PBS_CACHE_H_
#define SITE_PBS_CACHE_H_

#include "site_pbs_cache_common.h"



struct  repository_alternatives **get_bootable_alternatives(char *hostname,char *properties);
struct repository_alternatives **dup_bootable_alternatives(struct repository_alternatives **old);
void free_bootable_alternatives(struct  repository_alternatives **list);
int  alternative_has_property(struct  repository_alternatives *a, char *prop);
struct repository_alternatives *find_alternative_with_property(struct  repository_alternatives **list,char *prop);
char *get_alternative_properties(char *name);


/* function for magrathea */
int magrathea_decode(resource *res,long int *status,long int *count,long int *used,long int *m_free,long int *m_possible);


/* function for account test */ 
int site_user_has_account(char *user, char *host, char *cluster);

#endif
