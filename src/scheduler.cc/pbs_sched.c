/*
*         OpenPBS (Portable Batch System) v2.3 Software License
* 
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
* 
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
* 
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
* 
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
* 
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
* 
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
* 
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
* 
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
* 
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
* 
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
* 
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information 
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
* 
* 7. DISCLAIMER OF WARRANTY
* 
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
* 
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
#include <pbs_config.h>   /* the master config generated by configure */

#include        <unistd.h>
#include	<stdio.h>
#include        <stdlib.h>
#include	<sys/time.h>
#if (PLOCK_DAEMONS & 2)
#include	<sys/lock.h>
#endif /* PLOCK_DAEMONS */
#include	"libpbs.h"
#include        <string.h>
#include        <signal.h>
#include	<time.h>
#include        <ctype.h>
#include        <limits.h>
#include        <fcntl.h>
#include	<errno.h>
#include	<netdb.h>
#include	<grp.h>
#include        <sys/types.h>
#include        <sys/wait.h>
#include        <sys/stat.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
#include        <arpa/inet.h>

#if defined(FD_SET_IN_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#ifdef _CRAY
#include <sys/category.h>
#endif	/* _CRAY */
#include <sys/resource.h>

#include	"portability.h"
#include	"pbs_error.h"
#include	"pbs_ifl.h"
#include	"log.h"
#include	"resmon.h"
#include	"sched_cmds.h"
#include	"server_limits.h"
#include	"net_connect.h"
#include	"rpp.h"
#include	"rm.h"
#include	"libpbs.h"

extern int	pbs_errno;
int		connector;
int		server_sock;

#define		START_CLIENTS	2	/* minimum number of clients */
pbs_net_t	*okclients = NULL;	/* accept connections from */
int		numclients = 0;		/* the number of clients */
pbs_net_t      *maskclient = NULL;      /* wildcard connections */
int             mask_num = 0;
char		*configfile = NULL;	/* name of config file */

char		*oldpath;
extern char	*msg_daemonname;
char		**glob_argv;
char		usage[] = "[-S port][-d home][-p output][-c config][-a alarm]";
struct	sockaddr_in	saddr;
sigset_t	allsigs;
int		alarm_time;
static char    *logfile = (char *)0;
static char	path_log[_POSIX_PATH_MAX];
char	path_acct[_POSIX_PATH_MAX];
int 		pbs_rm_port;

int             schedreq();

extern int get_4byte(int,unsigned int *);


/*
**      Clean up after a signal.
*/

void die(

  int sig)

  {
  char *id = "die";

  if (sig > 0) 
    {
    sprintf(log_buffer, "caught signal %d", sig);

    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
      id, log_buffer);
    }
  else 
    {
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
      id, "abnormal termination");
    }

  log_close(1);

  exit(1);
  }  /* END die() */




  /*
   * Catch core dump signals - set core size so we can see what happened!
   *
   * Chris Samuel - VPAC
   * csamuel@vpac.org - 29th July 2003
   */

static void catch_abort(

  int sig)

  {
  struct rlimit rlimit;
  struct sigaction act;

  /*
   * Reset ourselves to the default signal handler to try and
   * prevent recursive core dumps.
   */

  sigemptyset(&act.sa_mask);
  act.sa_flags   = 0;
  act.sa_handler = SIG_DFL;

  sigaction(SIGSEGV, &act, NULL);
  sigaction(SIGBUS, &act, NULL);
  sigaction(SIGFPE, &act, NULL);
  sigaction(SIGILL, &act, NULL);
  sigaction(SIGTRAP, &act, NULL);
  sigaction(SIGSYS, &act, NULL);

  log_err (sig,"pbs_sched","Caught fatal core signal");
  rlimit.rlim_cur = RLIM_INFINITY;
  rlimit.rlim_max = RLIM_INFINITY;

  setrlimit(RLIMIT_CORE, &rlimit);
  abort();

  return;
  }  /* END catch_abort() */






static int server_disconnect(

  int connect)

  {
  close(connection[connect].ch_socket);

  if (connection[connect].ch_errtxt != (char *)NULL) 
    free(connection[connect].ch_errtxt);

  connection[connect].ch_errno = 0;
  connection[connect].ch_inuse = 0;

  return(0);
  }  /* END server_disconnect() */





/*
**	Got an alarm call.
*/

void toolong(

  int sig)

  {
  char	*id = "toolong";
  struct	stat	sb;
  pid_t	cpid;

	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, 
                   "scheduling iteration took too long");
	DBPRT(("scheduling iteration too long\n"))

	if (connector >= 0 && server_disconnect(connector))
		log_err(errno, id, "server_disconnect");
	if (close(server_sock))
		log_err(errno, id, "close");

	if ((cpid = fork()) > 0) {	/* parent re-execs itself */
		rpp_terminate();
#ifndef linux
		sleep(5);
#endif

		/* hopefully, that gave the child enough */
		/*   time to do its business. anyhow:    */
		(void)waitpid(cpid, NULL, 0);

		if (chdir(oldpath) == -1) {
			sprintf(log_buffer, "chdir to %s", oldpath);
			log_err(errno, id, log_buffer);
		}

		sprintf(log_buffer, "restart dir %s object %s",
			oldpath, glob_argv[0]);
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, log_buffer);

		execv(glob_argv[0], glob_argv);
		log_err(errno, id, "execv");
		exit(3);
	}

	/*
	**	Child (or error on fork) gets here and tried
	**	to dump core.
	*/
	if (stat("core", &sb) == -1) {
		if (errno == ENOENT) {
			log_close(1);
			abort();
			rpp_terminate();
			exit(2);	/* not reached (hopefully) */
		}
		log_err(errno, id, "stat");
	}
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
		id, "exiting without core dump");
	log_close(1);
	rpp_terminate();
	exit(0);
}


/* sock refers to an opened socket */
int
socket_to_conn(int sock)
{
	int     i;

	for (i=0; i<NCONNECTS; i++) {
		if (connection[i].ch_inuse == 0) {

			connection[i].ch_inuse = 1;
			connection[i].ch_errno = 0;
			connection[i].ch_socket= sock;
			connection[i].ch_errtxt = NULL;
			return (i);
		}
	}
	pbs_errno = PBSE_NOCONNECTS;
	return (-1);
}




int restricted(

  char  *name)

  {
  static  char    id[] = "restricted";

  struct  hostent         *host;
  struct  in_addr saddr;
  pbs_net_t       *newclients;
 
  if ((host = gethostbyname(name)) == NULL) 
    {
    sprintf(log_buffer,"host %s not found", 
      name);

    log_err(-1,id,log_buffer);

    return(-1);
    }

  if (mask_num > 0) 
    {
    newclients = realloc(maskclient,sizeof(pbs_net_t) * (mask_num + 1));
    }
  else 
    {
    newclients = malloc(sizeof(pbs_net_t));
    }

  if (newclients == NULL)
    {
    return(-1);
    }

  maskclient = newclients;

  log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,id,name);

  memcpy((char *)&saddr,host->h_addr,host->h_length);

  maskclient[mask_num++] = saddr.s_addr;

  return(0);
  }
 


int addclient(

  char *name)

  {
  static char     id[] = "addclient";
  struct hostent *host;
  struct in_addr  saddr;

  if ((host = gethostbyname(name)) == NULL) 
    {
    sprintf(log_buffer,"host %s not found", 
      name);

    log_err(-1,id,log_buffer);

    return(-1);
    }

	if (numclients >= START_CLIENTS) {
		pbs_net_t	*newclients;

		newclients = realloc(okclients,
				sizeof(pbs_net_t)*(numclients+1));
		if (newclients == NULL)
			return -1;
		okclients = newclients;
	}
	memcpy((char *)&saddr, host->h_addr, host->h_length);
	okclients[numclients++] = saddr.s_addr;
	return 0;
}







/*
 * read_config - read and process the configuration file (see -c option)
 *
 *  Currently, the parameters are 

      $clienthost - specify which systems can contact the scheduler
      $restricted - specify which systems can provide rm services on non-privileged ports
 */

#define CONF_LINE_LEN 120

static int read_config(

  char *file)

  {
  static char *id = "read_config";

  FILE	*conf;
  int	i;
  char	line[CONF_LINE_LEN];
  char	*token;
  struct	specialconfig {
  char	*name;
  int	(*handler)();
  } special[] = {
    {"clienthost", addclient },
    {"restricted", restricted },
    { NULL,	   NULL } };


#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)
  if (chk_file_sec(file,0,0,S_IWGRP|S_IWOTH,1,NULL))
    {
    return(-1);
    }
#endif

  free(maskclient);
 
  mask_num = 0;

  if ((conf = fopen(file,"r")) == NULL) 
    {
    log_err(errno,id,"cannot open config file");

    return(-1);
    }

	while (fgets(line, CONF_LINE_LEN, conf)) {

		if ((line[0] == '#') || (line[0] == '\n'))
			continue;		/* ignore comment & null line */
		else if (line[0] == '$') {	/* special */

			if ((token = strtok(line, " \t")) == NULL)
				token = "";
			for (i=0; special[i].name; i++) {
				if (strcmp(token+1, special[i].name) == 0)
					break;
			}
			if (special[i].name == NULL) {
				sprintf(log_buffer,"config name %s not known",
					token);
				log_record(PBSEVENT_ERROR,
					PBS_EVENTCLASS_SERVER,
					msg_daemonname, log_buffer);
				continue;
			}
			token = strtok(NULL, " \t");
			if (*(token+strlen(token)-1) == '\n')
				*(token+strlen(token)-1) = '\0';
			if (special[i].handler(token)) {
				fclose(conf);
				return (-1);
			}

		} else {
			log_record(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER,
				  msg_daemonname,
				  "invalid line in config file");
			fclose(conf);
			return (-1);
		}
	}
	fclose(conf);
	return (0);
}

void
restart(int sig)
{
	char    *id = "restart";

	if (sig) {
		log_close(1);
		log_open(logfile, path_log);
		sprintf(log_buffer, "restart on signal %d", sig);
	} else {
		sprintf(log_buffer, "restart command");
	}
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
	if (configfile) {
		if (read_config(configfile) != 0)
			die(0);
	}
}

void
badconn(char *msg)
{
	static	char	id[] = "badconn";
	struct	in_addr	addr;
	char		buf[5*sizeof(addr) + 100];
	struct	hostent	*phe;

	addr = saddr.sin_addr;
	phe = gethostbyaddr((void *)&addr, sizeof(addr), AF_INET);
	if (phe == NULL) {
		char	hold[6];
		int	i;
		union {
			struct	in_addr aa;
			u_char		bb[sizeof(addr)];
		} uu;

		uu.aa = addr;
		sprintf(buf, "%u", uu.bb[0]);
		for(i=1; i<(int)sizeof(addr); i++) {
			sprintf(hold, ".%u", uu.bb[i]);
			strcat(buf, hold);
		}
	}
	else {
		strncpy(buf, phe->h_name, sizeof(buf));
		buf[sizeof(buf)-1] = '\0';
	}

	sprintf(log_buffer, "%s on port %u %s",
			buf, ntohs(saddr.sin_port), msg);
	log_err(-1, id, log_buffer);

  return;
  }





int server_command()

  {
  char	*id = "server_command";

  int           new_socket;
  torque_socklen_t	slen;
  int		i;
  unsigned int  cmd;
  pbs_net_t	addr;

  slen = sizeof(saddr);

  new_socket = accept(server_sock,(struct sockaddr *)&saddr,&slen);

  if (new_socket == -1) 
    {
    log_err(errno,id,"accept");

    return(SCH_ERROR);
    }

  addr = (pbs_net_t)saddr.sin_addr.s_addr;
 
  if (ntohs(saddr.sin_port) >= IPPORT_RESERVED) 
    {
    for (i = 0;i < mask_num;i++) 
      {
      if (addr == maskclient[i])
        break;
      }

    if (i == mask_num) 
      {
      badconn("non-reserved port");

      close(new_socket);

      return(SCH_ERROR);
      }
    }
  else 
    {
    for (i = 0;i < numclients;i++) 
      {
      if (addr == okclients[i])
        break;
      }

    if (i == numclients) 
      {
      badconn("unauthorized host");

      close(new_socket);

      return(SCH_ERROR);
      }
    }

  if ((connector = socket_to_conn(new_socket)) < 0) 
    {
    log_err(errno,id,"socket_to_conn");

    return(SCH_ERROR);
    }

  if (get_4byte(new_socket,&cmd) != 1) 
    {
    log_err(errno,id,"get4bytes");

    return(SCH_ERROR);
    }

  return((int)cmd);
  }





/*
 * lock_out - lock out other daemons from this directory.
 *
 * op is either F_WRLCK or F_UNLCK
 */

static void lock_out(int fds, int op)
{
	struct flock flock;

	flock.l_type   = op;
	flock.l_whence = SEEK_SET;
	flock.l_start  = 0;
	flock.l_len    = 0;	/* whole file */
	if (fcntl(fds, F_SETLK, &flock) < 0) {
	    (void)strcpy(log_buffer, "pbs_sched: another scheduler running\n");
	    log_err(errno, msg_daemonname, log_buffer);
	    fprintf(stderr, log_buffer);
	    exit (1);
	}
}




int main(

  int   argc,
  char *argv[])

  {
  char		*id = "main";
  struct	hostent	*hp;
  int		go, c, errflg = 0;
  int		lockfds;
  int		t = 1;
  pid_t		pid;
  char		host[100];
  char		*homedir = PBS_SERVER_HOME;
  unsigned int	port;
  char		*dbfile = "sched_out";
  struct	sigaction	act;
  sigset_t	oldsigs;
  caddr_t	curr_brk = 0;
  caddr_t	next_brk;
  extern char	*optarg;
  extern int	optind, opterr;
  extern int	rpp_fd;
  fd_set	fdset;

  int		schedinit A_((int argc, char **argv));
  int		schedule A_((int com, int connector));

	glob_argv = argv;
	alarm_time = 180;

  /* The following is code to reduce security risks                */
  /* move this to a place where nss_ldap doesn't hold a socket yet */

  c = sysconf(_SC_OPEN_MAX);
  while (--c > 2)
    (void)close(c); /* close any file desc left open by parent */

	port = get_svrport(PBS_SCHEDULER_SERVICE_NAME, "tcp", 
			   PBS_SCHEDULER_SERVICE_PORT);
	pbs_rm_port = get_svrport(PBS_MANAGER_SERVICE_NAME, "tcp",
			   PBS_MANAGER_SERVICE_PORT);

	strcpy(pbs_current_user,"Scheduler");
	msg_daemonname=strdup("pbs_sched");

	opterr = 0;

  while ((c = getopt(argc, argv, "L:S:R:d:p:c:a:-:")) != EOF) {
		switch (c) {
                case '-':
                        if ((optarg == NULL) || (optarg[0] == '\0')) {
                                errflg = 1;
                        }

                        if (!strcmp(optarg,"version")) {
                                fprintf(stderr,"version: %s\n", PACKAGE_VERSION);
                                exit(0);
                        } else {
                                errflg = 1;
                        }
                        break;
		case 'L':
			logfile = optarg;
			break;
		case 'S':
			port = atoi(optarg);
			if (port == 0) {
				fprintf(stderr,
					"%s: illegal port\n", optarg);
				errflg = 1;
			}
			break;
		case 'R':
			if ((pbs_rm_port = atoi(optarg)) == 0) {
			    (void)fprintf(stderr, "%s: bad -R %s\n",
				argv[0], optarg);
			return 1;
			}
			break;
		case 'd':
			homedir = optarg;
			break;
		case 'p':
			dbfile = optarg;
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'a':
			alarm_time = atoi(optarg);
			if (alarm_time == 0) {
				fprintf(stderr,
					"%s: bad alarm time\n", optarg);
				errflg = 1;
			}
			break;
		case '?':
                        errflg = 1;
			break;
		}
	}
	if (errflg) {
		fprintf(stderr, "usage: %s %s\n", argv[0], usage);
		exit(1);
	}

#ifndef DEBUG
        if ((geteuid() != 0) || (getuid() != 0)) {
                fprintf(stderr, "%s: Must be run by root\n", argv[0]);
                return (1);
        }
#endif        /* DEBUG */

	/* Save the original working directory for "restart" */
	if ((oldpath = getcwd((char *)NULL, MAXPATHLEN)) == NULL) {
		fprintf(stderr, "cannot get current working directory\n");
		exit(1);
	}

	(void)sprintf(log_buffer, "%s/sched_priv", homedir);
#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)
	c  = chk_file_sec(log_buffer,1,0,S_IWGRP|S_IWOTH,1,NULL);
	c |= chk_file_sec(PBS_ENVIRON,0,0,S_IWGRP|S_IWOTH,0,NULL);
	if (c != 0) exit(1);
#endif  /* not DEBUG and not NO_SECURITY_CHECK */
	if (chdir(log_buffer) == -1) {
		perror("chdir");
		exit(1);
	}
	(void)sprintf(path_log,   "%s/sched_logs", homedir);
	(void)sprintf(path_acct,   "%s/%s", log_buffer,PBS_ACCT );


	/* The following is code to reduce security risks                */
	/* start out with standard umask, system resource limit infinite */

	umask(022);
	if (setup_env(PBS_ENVIRON)==-1)
		exit(1);
	c = getgid();
	(void)setgroups(1, (gid_t *)&c);	/* secure suppl. groups */

#ifndef DEBUG
#ifdef _CRAY
	(void)limit(C_JOB,      0, L_CPROC, 0);
	(void)limit(C_JOB,      0, L_CPU,   0);
	(void)limit(C_JOBPROCS, 0, L_CPU,   0);
	(void)limit(C_PROC,     0, L_FD,  255);
	(void)limit(C_JOB,      0, L_FSBLK, 0);
	(void)limit(C_JOBPROCS, 0, L_FSBLK, 0);
	(void)limit(C_JOB,      0, L_MEM  , 0);
	(void)limit(C_JOBPROCS, 0, L_MEM  , 0);
#else	/* not  _CRAY */
	{
	struct rlimit rlimit;

	rlimit.rlim_cur = RLIM_INFINITY;
	rlimit.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU,   &rlimit);
	(void)setrlimit(RLIMIT_FSIZE, &rlimit);
	(void)setrlimit(RLIMIT_DATA,  &rlimit);
	(void)setrlimit(RLIMIT_STACK, &rlimit);
#ifdef  RLIMIT_RSS
        (void)setrlimit(RLIMIT_RSS  , &rlimit);
#endif  /* RLIMIT_RSS */
#ifdef  RLIMIT_VMEM
        (void)setrlimit(RLIMIT_VMEM  , &rlimit);
#endif  /* RLIMIT_VMEM */
	}
#endif	/* not _CRAY */
#endif	/* DEBUG */

	if (log_open(logfile, path_log) == -1) {
		fprintf(stderr, "%s: logfile could not be opened\n", argv[0]);
		exit(1);
	}

	if (gethostname(host, sizeof(host)) == -1) {
		log_err(errno, id, "gethostname");
		die(0);
	}
	if ((hp =gethostbyname(host)) == NULL) {
		log_err(errno, id, "gethostbyname");
		die(0);
	}
	if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_err(errno, id, "socket");
		die(0);
	}
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)&t, sizeof(t)) == -1) {
		log_err(errno, id, "setsockopt");
		die(0);
	}
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	memcpy (&saddr.sin_addr, hp->h_addr, hp->h_length);
	if (bind (server_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		log_err(errno, id, "bind");
		die(0);
	}
	if (listen (server_sock, 5) < 0) {
		log_err(errno, id, "listen");
		die(0);
	}

	okclients = (pbs_net_t *)calloc(START_CLIENTS, sizeof(pbs_net_t));
	addclient("localhost");   /* who has permission to call MOM */
	addclient(host);
	if (configfile) {
		if (read_config(configfile) != 0)
			die(0);
	}

	lockfds = open("sched.lock", O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (lockfds < 0) {
		log_err(errno, id, "open lock file");
		exit(1);
	}
	lock_out(lockfds, F_WRLCK);

	fullresp(0);
	if (sigemptyset(&allsigs) == -1) {
		perror("sigemptyset");
		exit(1);
	}
	if (sigprocmask(SIG_SETMASK, &allsigs, NULL) == -1) {	/* unblock */
		perror("sigprocmask");
		exit(1);
	}
	act.sa_flags = 0;
	sigaddset(&allsigs, SIGHUP);    /* remember to block these */
	sigaddset(&allsigs, SIGINT);    /* during critical sections */
	sigaddset(&allsigs, SIGTERM);   /* so we don't get confused */
	act.sa_mask = allsigs;

	act.sa_handler = restart;       /* do a restart on SIGHUP */
	sigaction(SIGHUP, &act, NULL);

	act.sa_handler = toolong;	/* handle an alarm call */
	sigaction(SIGALRM, &act, NULL);

	act.sa_handler = die;           /* bite the biscuit for all following */
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

        /*
         * Catch these signals to ensure we core dump even if
         * our rlimit for core dumps is set to 0 initially.
         * 
         * Chris Samuel - VPAC
         * csamuel@vpac.org - 29th July 2003
         * 
         * Now conditional on the PBSCOREDUMP environment variable
         */

        if (getenv("PBSCOREDUMP"))
          {
          act.sa_handler = catch_abort;   /* make sure we core dump */

          sigaction(SIGSEGV,&act, NULL);
          sigaction(SIGBUS, &act, NULL);
          sigaction(SIGFPE, &act, NULL);
          sigaction(SIGILL, &act, NULL);
          sigaction(SIGTRAP,&act, NULL);
          sigaction(SIGSYS, &act, NULL);
          }

	/*
	 *  Local initialization stuff
	 */

	if (schedinit(argc, argv)) {
		(void) sprintf(log_buffer,
			"local initialization failed, terminating");
		log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,id,log_buffer);
		exit(1);
	}

        if (getenv("PBSDEBUG") == NULL)
          {
	  lock_out(lockfds,F_UNLCK);

          if ((pid = fork()) == -1) 
            {     /* error on fork */
	    perror("fork");

            exit(1);
            }
          else if (pid > 0)               /* parent exits */
            {
            exit(0);
            }

          if ((pid = setsid()) == -1) 
            {
            perror("setsid");

            exit(1);
            }

          lock_out(lockfds, F_WRLCK);

          freopen(dbfile, "a", stdout);

          setvbuf(stdout, NULL, _IOLBF, 0);

          dup2(fileno(stdout), fileno(stderr));
          }
        else
          {
          setvbuf(stdout, NULL, _IOLBF, 0);
          setvbuf(stderr, NULL, _IOLBF, 0);

          pid = getpid();
          }

        freopen("/dev/null", "r", stdin);

        /* write scheduler's pid into lockfile */

        (void)sprintf(log_buffer, "%ld\n", (long)pid);

        if (write(lockfds, log_buffer, strlen(log_buffer)+1) != (ssize_t)(strlen(log_buffer)+1))
          {
          perror("writing to lockfile");

          exit(1);
          }

#if (PLOCK_DAEMONS & 2)
	(void)plock(PROCLOCK);	/* lock daemon into memory */
#endif

	sprintf(log_buffer, "%s startup pid %ld", argv[0], (long)pid);
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

	FD_ZERO(&fdset);

  for (go = 1;go;) 
    {
    int cmd;

		if (rpp_fd != -1)
			FD_SET(rpp_fd, &fdset);
		FD_SET(server_sock, &fdset);
		if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) == -1) {
			if (errno != EINTR) {
				log_err(errno, id, "select");
				die(0);
			}
			continue;
		}

		if (rpp_fd != -1 && FD_ISSET(rpp_fd, &fdset)) {
			if (rpp_io() == -1)
				log_err(errno, id, "rpp_io");
		}
		if (!FD_ISSET(server_sock, &fdset))
			continue;

		cmd = server_command();
		if (sigprocmask(SIG_BLOCK, &allsigs, &oldsigs) == -1)
			log_err(errno, id, "sigprocmaskSIG_BLOCK)");

		alarm(alarm_time);
		if (schedule(cmd, connector))	/* magic happens here */
			go = 0;
		alarm(0);

		if (connector >= 0 && server_disconnect(connector)) {
			log_err(errno, id, "server_disconnect");
			die(0);
		}
		next_brk = (caddr_t)sbrk(0);
		if (next_brk > curr_brk) {
			sprintf(log_buffer, "brk point %ld", (long)next_brk);
			log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER,
				id, log_buffer);
			curr_brk = next_brk;
		}
		if (sigprocmask(SIG_SETMASK, &oldsigs, NULL) == -1)
			log_err(errno, id, "sigprocmask(SIG_SETMASK)");
    }

  sprintf(log_buffer,"%s normal finish pid %ld", 
    argv[0], 
    (long)pid);

  log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,id,log_buffer);

  close(server_sock);

  exit(0);
  }  /* END main() */
