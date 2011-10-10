
struct metric_source {
    char *server;
    int type;
    int freq;
    int port;
    char *remotename;
    pthread_t th;
    struct metric *parent;
    struct metric_source *next;
};

struct metric {
     char *name;
     struct listhead *head;
     pthread_mutex_t lock;
     struct metric *next;
     int should_dump;
     struct mylist *lastused;
     struct metric_source *source;
};

extern struct metric *metrics;

#define M_ANY 0
#define M_REMOTE 1
#define M_GANGLIA 2
#define M_PBS 3
#define M_CACHE 4
#define M_UPDATE 5
#define M_DIST 6

int memory_init();
int memory_add_metric(char *name,int type,char *server,int freq,int port,char *remotename);
int memory_remove(char *hostname,char *name, int replic_commited);
char *memory_find(char *hostname,char *name,time_t *timestamp);
char *memory_find_host(struct metric *m,char *hostname,time_t *timestamp);
int memory_dump(char *name,FILE *file,int flush);
int memory_dump_s(struct metric *m, FILE *file,int flush,int remote,char *remotename);
int memory_load(char * name, FILE *file,int remote);
int memory_load_s(struct metric *m, FILE *file,int remote);
int memory_update(char *hostname,char *name, char *value,time_t timestamp, int replic_commited);
int memory_update_s(struct metric *m,char *hostname,char *value,time_t timestamp, int replic_commited);
int memory_timeout();
