#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <pthread.h>
#include <sysexits.h>
#include <sys/queue.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <regex.h>
#include <ctype.h>
#include <signal.h>

struct conf {
      char *user;
      char *pid;
      int debug;
      time_t timeout;
      int port;
      char *sock;
      char *dumpfile;
      char *root;
      int dumpfreq;
      int socktimeout;
      int backup;
      void *strictip;
      void *adminip;
      int strictport;
};

extern struct conf conf;

#define MUTEXLOCK(lock,name) {                                  \
    int err;                                                    \
    if (conf.debug>2)                                           \
         my_log(LOG_DEBUG,"%s:%d mtex lock %s",                 \
                __FILE__, __LINE__,name);                       \
    if ((err=pthread_mutex_lock(&lock)) !=0 ) {                 \
        my_log(LOG_ERR,"mutex lock failed %",strerror(err));    \
        exit(EX_SOFTWARE);                                      \
    }                                                           \
}

#define MUTEXUNLOCK(lock,name) {                                \
    int err;                                                    \
    if (conf.debug>2)                                           \
         my_log(LOG_DEBUG,"%s:%d mtex unlock %s",               \
                __FILE__, __LINE__,name);                       \
    if ((err=pthread_mutex_unlock(&lock)) !=0 ) {               \
        my_log(LOG_ERR,"mutex unlock failed %",strerror(err));  \
        exit(EX_SOFTWARE);                                      \
    }                                                           \
}

