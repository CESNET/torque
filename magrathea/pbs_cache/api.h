
#define PBS_CACHE_ROOT "/var/cache/pbs_cache"
#define PBS_CACHE_PORT 7878
#define PBS_CACHE_SOCK "/pbs_cache.sock"

FILE *cache_connect_net(char *hostname,int port);
FILE *cache_connect_local();
void cache_close(FILE *stream);

int cache_store_local(char *hostname,char *name,char *value);
int cache_store(FILE *stream,char *hostname,char *name,char *value);

char *cache_get(FILE *stream,char *hostname,char *name);
char *pbs_cache_get_local(char *hostname,char *name);

int cache_remove(FILE *stream,char *hostname,char *name);
int cache_remove_local(char *hostname,char *name);
char *cache_value_only(char *old);

#ifndef PBS_CACHE_API_STORE_ONLY
int cache_remote_metric(FILE *stream,char *name,int type);
int cache_list(FILE *stream,char *metric,FILE *ostream);
void *cache_hash_init();
int cache_hash_fill(FILE *stream,char *metric,void *hash);
int cache_hash_fill_local(char *metric, void *hash);
char *cache_hash_find(void *hash,char *key);
void cache_hash_destroy(void *hash);
#endif

