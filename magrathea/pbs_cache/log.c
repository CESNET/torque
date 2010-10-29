

#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"

void my_log(int level,char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (conf.debug) {
           vfprintf(stderr,fmt,ap);
           fprintf(stderr, "\n");
    } else {
           vsyslog(level, fmt, ap);
    }
    va_end(ap);
    return ;
}

