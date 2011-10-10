
#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#ifndef DISABLE_GANGLIA
# include "ganglia.h"
#endif
#ifndef DISABLE_PBS
# include "pbs.h"
#endif
#include "addrlist.h"
#include "api.h"
#include "update.h"
#include "../net.h"
#ifndef DIST_DISABLED
#include "./distrib/main.h"
#endif


struct conf conf;
#ifndef DIST_DISABLED
static int rep_bool=0;
#endif

void set_default_conf()
{
  conf.user=NULL;
  conf.pid=NULL;
  conf.debug=0;
  conf.timeout=0;
  conf.port=PBS_CACHE_PORT;
  conf.sock=PBS_CACHE_ROOT PBS_CACHE_SOCK;
  conf.dumpfile =  PBS_CACHE_ROOT "/pbs_cache.";
  conf.root=PBS_CACHE_ROOT;
  conf.dumpfreq=120;
  conf.socktimeout = 0;
  conf.backup = 0;
  conf.strictip=NULL;
  conf.adminip=NULL;
  conf.strictport=0;
}

/* 
 * check_metric()
 * parse metric string to metric name, server name, update frequency
 */
void check_metric(char *opt,int type)
{ char *s;
  char *ss;
  char *c;
  char *d;
  char *dd;
  int freq;
  struct hostent *h;
  int port=0;
  char *remotename=NULL;
  char *hostname=NULL;
  char *name=NULL;

  if (type==M_REMOTE) { 
      if ((strchr(optarg,':')!=NULL) || (strchr(optarg,'\\')!=NULL)) {
	  fprintf(stderr,"remote metric cannot contain : or \\, %s\n",opt);
	  exit(EX_OSERR);
       }
      memory_add_metric(optarg,M_REMOTE,NULL,0,0,NULL);
      return;
  }
  #ifndef DIST_DISABLED
  if (type == M_DIST) {
        if ((strchr(optarg, ':') != NULL) || (strchr(optarg, '\\') != NULL)) {
            fprintf(stderr, "distributed metric cannot contain : or \\, %s\n", opt);
            exit(EX_OSERR);
        }
        memory_add_metric(optarg, M_DIST, NULL, 0, 0, NULL);
        rep_bool = 1;
        return;
  }
  #endif
  /* metric can be metricname[/remotename]:freq:hostname[:port] */

  /* metric[/remotename] */
  s=strdup(opt);
  if (s==NULL) {
      fprintf(stderr,"malloc failed in check_metric\n");
      exit(EX_OSERR);
  }
  c=strchr(s,':');
  if (c==NULL) {
      fprintf(stderr,"missing : in check_metric: %s\n",opt);
      exit(EX_OSERR);
  }
  *c='\0';

  ss=strchr(s,'/');
  if (ss!=NULL) {
      *ss='\0';
      ss++;
      remotename=strdup(ss);
      if (remotename==NULL) {
        fprintf(stderr,"malloc failed in check_metric\n");
        exit(EX_OSERR);
      }
  }

  name=strdup(s);
  if (name==NULL) {
      fprintf(stderr,"malloc failed in check_metric\n");
      exit(EX_OSERR);
  }

  /* port */
  c++;
  d=strchr(c,':');
  if (d==NULL) {
      fprintf(stderr,"missing : in check_metric: %s\n",opt);
      exit(EX_OSERR);
  }
  *d='\0';
  d++;
  freq=atoi(c);
  if (freq<=0) {
      fprintf(stderr,"cannot parse update frequency in %s\n",opt);
      exit(EX_OSERR);
  }

  /* hostname[:port] */
  dd=strchr(d,':');
  if (dd!=NULL) {
      *dd='\0';
      dd++;

      port=atoi(dd);
      if (port<0) {
	  fprintf(stderr,"cannot parse port number in %s\n",opt);
	  exit(EX_OSERR);
      }
  }

  h=gethostbyname(d);
  if (h==NULL) {
      fprintf(stderr,"cannot resolve server %s\n",d);
      exit(EX_OSERR);
  }

  hostname=strdup(d);
  if (hostname==NULL) {
      fprintf(stderr,"malloc failed in check_metric\n");
      exit(EX_OSERR);
  }


  if ((port==0) && (type==M_PBS))
      port=15003;
  if ((port==0) && (type==M_GANGLIA))
      port=8649;
  if ((port==0) && (type==M_CACHE))
      port=PBS_CACHE_PORT;
  if ((port==0) && (type==M_REMOTE))
      port=PBS_CACHE_PORT;
  if ((port==0) && (type==M_UPDATE))
      port=PBS_CACHE_PORT;
  memory_add_metric(name,type,hostname,freq,port,remotename);
  free(s);
  return;
}

void usage() 
{
#ifndef DISABLE_GANGLIA
# define USAGE_GANGLIA " [-g ganglia_metric]"
#else
# define USAGE_GANGLIA ""
#endif
#ifndef DISABLE_PBS
# define USAGE_PBS " [-M pbs_metric]"
#else
# define USAGE_PBS ""
#endif
#ifndef DIST_DISABLED
# define USAGE_DIST " [-r replicated_metric]"
#else
# define USAGE_DIST ""
#endif

  fprintf(stderr,"pbs_cache %s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
	  "[-d] [-d ] [-u user] [-p pidfile] [-t timeout] [-P port]",
	  "[-s socket] [-f dumpfile] [-b backup_freq]",
	  "[-T net_timeout] [-a] [-A regex ] [ -I regex] [-D dump_freq]",
	  "[-m metric]" USAGE_GANGLIA USAGE_PBS,
          USAGE_DIST,
	  "[-w update_metric] [-c cache_metric]");
  fprintf(stderr,"Metric definition: metricname[/remotename]:freq:hostname[:port]\n");
  fprintf(stderr,"\t metric filled via updates from clients are defined with option -m\n");
#ifndef DISABLE_GANGLIA
  fprintf(stderr,"\t ganglia import - metrics defined by -g\n");
#endif
#ifndef DISABLE_PBS
  fprintf(stderr,"\t PBS import - metrics defined by -M\n");
#endif
  fprintf(stderr,"\t cache import - metrics defined by -c\n");
  fprintf(stderr,"\t cache export - metrics defined by -w\n");
  fprintf(stderr,"Other options\n");
  fprintf(stderr,"\t -u user - change identity from root to user\n");
  fprintf(stderr,"\t -p pidfile - write pid to file pidfife\n");
  fprintf(stderr,"\t -t timeout - remove from cache records older then specified number of seconds\n");
  fprintf(stderr,"\t -P port - listen on this port instead of default 7878\n");
  fprintf(stderr,"\t -s socket - used this unix socket istead of default one\n");
  fprintf(stderr,"\t -f dumpfile - use this prefix for dumpfiles\n");
  fprintf(stderr,"\t -b backup_freq - backup metrics in specified frequency\n");
  fprintf(stderr,"\t -D dump_freq - dump metrics in specified frequency\n");
  fprintf(stderr,"\t -T timeout - network timeout for all communication\n");
  fprintf(stderr,"\t -a - allow only connection from port <1023\n");
  fprintf(stderr,"\t -I regex - allow only connection from IP matching regex \n");
  fprintf(stderr,"\t -A regex - connection from IP matching regex is priviledged\n");
  exit(EX_USAGE);
}

void parse_args(int argc,char **argv)
{ int ch;
  char ibuf[10240];
  char *icopy;

  ibuf[0]='\0';

  while ((ch = getopt(argc, argv, "u:p:dt:P:s:f:b:D:T:aA:I:m:r:g:M:w:c:")) != -1) {
      switch (ch) {
	  case 'u':
	      if (optarg == NULL)
		  usage();
	      if (geteuid() != 0) {
		  fprintf(stderr,"only root can use -u");
		  usage();
	      }
	      conf.user=strdup(optarg);
	      break;
	  case 'p':
	      if (optarg == NULL)
		  usage();
	      conf.pid=strdup(optarg);
	      break;
	  case 'd':
	      conf.debug++;
	      break;
	  case 't':
	      if (optarg == NULL) 
		  usage();
	      conf.timeout=atoi(optarg);
	      break;
	  case 'P':
	      if (optarg == NULL) 
		  usage();
	      conf.port=atoi(optarg);
	      break;
	  case 's':
	      if (optarg == NULL)
		  usage();
	      conf.sock=strdup(optarg);
	      break;
	  case 'f':
	      if (optarg == NULL)
		  usage();
	      conf.dumpfile=strdup(optarg);
	      break;
	  case 'b':
	      if (optarg == NULL)
		  usage();
	      conf.backup=atoi(optarg);
	      break;
	  case 'D':
	      if (optarg == NULL) 
		  usage();
	      conf.dumpfreq=atoi(optarg);
	      break;
	  case 'T':
	      if (optarg == NULL) 
		  usage();
	      conf.socktimeout=atoi(optarg);
	      break;
	  case 'a':
	      conf.strictport=1;
	      break;
	  case 'A':
	      if (optarg == NULL) 
		  usage();
	      conf.adminip=(void *)set_pattern_list(optarg);
	      if (conf.adminip==NULL) {
		  my_log(LOG_ERR, "cannot parse address limitation");
		  exit(EX_OSERR);
	      }
	      break;
	  case 'I':
	      if (optarg == NULL) 
		  usage();

	      strcat(ibuf,optarg);
	      strcat(ibuf," ");
	      icopy=strdup(ibuf);
	      conf.strictip=(void *)set_pattern_list(icopy);
	      if (conf.strictip==NULL) {
		  my_log(LOG_ERR, "cannot parse address limitation");
		  exit(EX_OSERR);
	      }
	      /* free(icopy); */
	      break;
	  case 'm':
	      if (optarg == NULL)
		  usage();
	      check_metric(optarg,M_REMOTE);
	      break;
#ifndef DIST_DISABLED
         case 'r':
              if (optarg == NULL)
                    usage();
              check_metric(optarg, M_DIST);
              break;
#endif
#ifndef DISABLE_GANGLIA
	  case 'g':
	      if (optarg == NULL)
		  usage();
	      check_metric(optarg,M_GANGLIA); 
	      break;
#endif
#ifndef DISABLE_PBS
	  case 'M':
	      if (optarg == NULL)
		  usage();
	      check_metric(optarg,M_PBS); 
	      break;
#endif
	  case 'w':
	      if (optarg == NULL)
		  usage();
	      check_metric(optarg,M_UPDATE); 
	      break;
	  case 'c':
	      if (optarg == NULL)
		  usage();
	      check_metric(optarg,M_CACHE); 
	      break;
	  default:
	      usage();
	      break;
      }
  }

}

void writepidfile() 
{
 FILE *f; 

 if (conf.pid != NULL) {
      if ((f = fopen(conf.pid, "w")) == NULL) {
                my_log(LOG_ERR, "failed open pidfile %s for writing: %s",
                    conf.pid, strerror(errno));
                exit(EX_OSERR);
        }

      fprintf(f, "%ld\n", (long)getpid());
      fclose(f);
 }

}

void droprootpriv() 
{ struct passwd *pw = NULL;

  if (conf.user) {
       if ((pw = getpwnam(conf.user)) == NULL) {
                        my_log(LOG_ERR, "cannot get user %s data: %s",
                            conf.user, strerror(errno));
                        exit(EX_OSERR);
       }

       if (setgid(pw->pw_gid) != 0 ||
                    setegid(pw->pw_gid) != 0) {
                        my_log(LOG_ERR, "cannot change GID: %s",
                            strerror(errno));
                        exit(EX_OSERR);
        }


        if ((setuid(pw->pw_uid) != 0) ||
                    (seteuid(pw->pw_uid) != 0)) {
                        my_log(LOG_ERR, "cannot change UID: %s",
                            strerror(errno));
                        exit(EX_OSERR);
        }
   }

}

/* 
 * final_func()
 * called from atexit, dump metrics, do cleanup
 */
void final_func()
{ final_dump();
  /*
    TODO LATER atexit should contain also socket cleanup etc.
  */
  return;
}

int main(int argc,char **argv) 
{ int ret;
  pthread_t tid;
  struct metric_source *s;
  struct metric *m;

  net_proto(NPROTO_AUTO);

  set_default_conf();
  memory_init();
#ifndef DIST_DISABLED
  pax_report_init();
  preinit_rep();
  init_paxos_log();
#endif

  parse_args(argc, argv);

#ifndef DIST_DISABLED
  if(rep_bool)
      if (init_rep() != 0) {
        fprintf(stderr, "replicated metric cannot be initiated\n");
        exit(EX_OSERR);
      }
#endif

  dump_init();
#ifndef DISABLE_GANGLIA
  ganglia_init();
#endif
#ifndef DISABLE_PBS
  pbs_init();
#endif

  openlog("pbs-cache",0,LOG_DAEMON);

  /* is it correct?*/
  signal(SIGPIPE,SIG_IGN);

  local_socket();
  remote_socket();

  load_dump();

  chdir(PBS_CACHE_ROOT);
  /* become daemon */
  if (conf.debug==0) {
                (void)close(0);
                (void)open("/dev/null", O_RDONLY, 0);
                (void)close(1);
                (void)open("/dev/null", O_WRONLY, 0);
                (void)close(2);
                (void)open("/dev/null", O_WRONLY, 0);

                if (chdir("/") != 0) {
                        my_log(LOG_ERR, "cannot chdir to root: %s",
                            strerror(errno));
                        exit(EX_OSERR);
                }

                switch (fork()) {
                case -1:
                        my_log(LOG_ERR,"%s: cannot fork: %s",
                            strerror(errno));
                        exit(EX_OSERR);
                        break;

                case 0:
                        break;

                default:
                        exit(EX_OK);
                        break;
                }

                if (setsid() == -1) {
                        my_log(LOG_ERR, "setsid failed: %s",
                            strerror(errno));
                        exit(EX_OSERR);
                }
   }

   writepidfile(); 
   droprootpriv(); 
  
   /* start dump proces */
   if ((ret = pthread_create(&tid, NULL, dumper_proces, NULL)) != 0) {
             my_log(LOG_ERR, "cannot start dumper thread: %s", strerror(ret));
             exit(EX_OSERR);
   }

   /* start listener on IP interface */
   if ((ret = pthread_create(&tid, NULL, listener_net_proces, NULL)) != 0) {
             my_log(LOG_ERR, "cannot start receiver thread: %s", strerror(ret));
             exit(EX_OSERR);
   }
    
   /* start listener on unix socket */
   if ((ret = pthread_create(&tid, NULL, listener_local_proces, NULL)) != 0) {
             my_log(LOG_ERR, "cannot start answer thread: %s", strerror(ret));
             exit(EX_OSERR);
   }

   atexit(final_func);
   for(;;) {
       for(m=metrics;m;m=m->next) {
             for(s=m->source;s;s=s->next) {
#ifndef DISABLE_GANGLIA
	           if ((s->type==M_GANGLIA) && (s->th==0)) {
                      ret = pthread_create(&tid, NULL, ganglia_proces, s);
		      if (ret!=0) {
                        my_log(LOG_ERR, "cannot start ganglia thread: %s", 
			       strerror(ret));
                        exit(EX_OSERR);
		      }
		      s->th=tid;
		   }
#endif
#ifndef DISABLE_PBS
	           if ((s->type==M_PBS) && (s->th==0)) {
                      ret = pthread_create(&tid, NULL, pbs_proces, s);
		      if (ret!=0) {
                        my_log(LOG_ERR, "cannot start pbs thread: %s", 
			       strerror(ret));
                        exit(EX_OSERR);
		      }
		      s->th=tid;
		   }
#endif
	           if ((s->type==M_UPDATE) && (s->th==0)) {
                      ret = pthread_create(&tid, NULL, update_to_proces, s);
		      if (ret!=0) {
                        my_log(LOG_ERR, "cannot start update_to thread: %s", 
			       strerror(ret));
                        exit(EX_OSERR);
		      }
		      s->th=tid;
		   }
	           if ((s->type==M_CACHE) && (s->th==0)) {
                      ret = pthread_create(&tid, NULL, update_from_proces, s);
		      if (ret!=0) {
                        my_log(LOG_ERR, "cannot start update_from thread: %s", 
			       strerror(ret));
                        exit(EX_OSERR);
		      }
		      s->th=tid;
		   }
	     }
       }
       sleep(100);
   }

   /* 
    TODO LATER check for thread exit
   tid=waitpid(-1,&ret,__WALL|~__WNOTHREAD); 
   my_log(LOG_ERR,"thread %d died, exiting",tid);
   */
   exit(EX_OSERR);
}
