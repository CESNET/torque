

#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"


#define BIG_BUFFER 1024 * 1024
char big_buf[BIG_BUFFER];

/* 
 * dump_init()
 * initialisation for dumper proces - check, whether dump directory exists 
 */
int dump_init()
{ struct stat s;
  int ret;

  ret=stat(PBS_CACHE_ROOT,&s);
  if (ret!=0) {
      my_log(LOG_ERR,"cannot stat %s: %s",PBS_CACHE_ROOT,strerror(errno));
      exit(EX_OSERR);
  }

  if ((s.st_mode & S_IFDIR) == 0) { 
      my_log(LOG_ERR,"%s is not directory",PBS_CACHE_ROOT); 
      exit(EX_OSERR); 
  }

  return 0;
}

/*
 * load dump()
 * for all defined metrics, load dumpfile if exists
 */
int load_dump()
{
  FILE *dump;
  struct metric *m;
  char file[MAXPATHLEN+100];

  for (m=metrics;m;m=m->next) {
    strcpy(file,conf.dumpfile);
    strcat(file,"dump.");
    strcat(file,m->name);
    if ((dump = fopen(file, "r")) == NULL) {
        my_log(LOG_ERR, "cannot read dumpfile \"%s\"", file);
        continue;
    } else {
        memory_load_s(m,dump,0);
        m->should_dump=0;
        fclose(dump);
    }
  }

  return 0;
}

/*
 * do_dump()
 * for all defined metrics, dump them to file if changed
 * may be also called with backup - in this case, dont' replace
 * dump file but store dump into separate file
 */
int do_dump(int backup)
{  
  int fd;
  char newdumpfile[MAXPATHLEN + 1];
  FILE *dump;
  struct metric *m;
  char file[MAXPATHLEN+100];


  for (m=metrics;m;m=m->next) {

    if ((m->should_dump) || (backup==1)) {
       if (conf.debug)
          my_log(LOG_DEBUG,"%s started for %s",(backup)?"backup":"dump",m->name);
       m->should_dump=0;
       strcpy(file,conf.dumpfile);
       if (backup) 
	   strcat(file,"backup.");
       else
	   strcat(file,"dump.");
       strcat(file,m->name);
       if (backup) {
	   char s[MAXPATHLEN+100];

	   strcat(file,".");
	   sprintf(s,"%ld",time(NULL));
	   strcat(file,s);
       }

       snprintf(newdumpfile, MAXPATHLEN,"%s-XXXXXXXX", file);
       if ((fd = mkstemp(newdumpfile)) == -1) {
          my_log(LOG_ERR, "mkstemp(\"%s\") failed: %s",newdumpfile, strerror(errno));
          exit(EX_OSERR);
       }
       if ((dump = fdopen(fd, "w")) == NULL) {
          my_log(LOG_ERR, "cannot write dumpfile \"%s\": %s",
	         newdumpfile, strerror(errno));
          exit(EX_OSERR);
       }
       setvbuf(dump, big_buf, _IOFBF, BIG_BUFFER);

       memory_dump_s(m,dump,0,0,NULL);

       fclose(dump);
       if (rename(newdumpfile, file) != 0) {
           my_log(LOG_ERR, "rename failed");
           exit(EX_OSERR);
       }
       if (conf.debug)
           my_log(LOG_DEBUG,"%s finished for metric %s",(backup)?"backup":"dump",m->name);
    }
  }

  return 0;
}

/* 
 * dumper_proces()
 * dumper thread, calls periodicaly dump,backup,cache_cleanup
 */
void *dumper_proces(void *arg)
{ int b;
  time_t t;

  b=conf.backup;
  for (;;) {
      t=time(NULL);
      sleep(conf.dumpfreq); 
      if (conf.timeout)
	  memory_timeout();
      do_dump(0);
      if (conf.backup) {
	  b=b-(time(NULL)-t);
	  if (b<0) {
	      do_dump(1);
	      b=conf.backup;
	  }
      }
  }
  /* v milter_grey je to slozitejsi, budi se i pres dump_sleepflag */

  /* not reached */
  my_log(LOG_ERR,"dumper proces died unexpectedly");
  return NULL;
}

/* 
 * final_dump()
 * call from atexit()
 */
void final_dump() 
{
  do_dump(0);
  return;
}
