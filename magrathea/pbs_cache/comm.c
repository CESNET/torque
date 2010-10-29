
#include "pbs_cache.h"
#include "comm.h"
#include "memory.h"
#include "dump.h"
#include "log.h"
#include "api.h"

#include "addrlist.h"

#include "../net.h"

int handle_request(FILE *stream,int priv);
void *handle_proces(void *arg);

/*
 * local_socket()
 * initialisation of local unix socket
 */
int local_socket()
{
  struct stat s;

  if (stat(conf.sock,&s) != 0)
      return 0;

  if ((s.st_mode & S_IFSOCK) == 0)
      return 0;

  return unlink(conf.sock);

}

/*
 * network_bind()
 * bind to IP socket
 */
int network_bind()
{ int s;
  int optval;
  int ret;

  s = net_socket(net_inaddr_any, &conf.port, 0, 1);
  if (s == -1) {
      my_log(LOG_ERR,"socket failed in listener_net: %s",strerror(errno));
      exit(EX_OSERR);
  }

  optval = 1;
  setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,&optval, sizeof(optval));

  ret=listen(s,100);
  if (ret == -1) {
      my_log(LOG_ERR,"listen failed in listener_net: %s",strerror(errno));
      exit(EX_OSERR);
  }

  return s;
}

/*
 * remote_socket()
 * initialisation of network socket - test, whether program can bind to
 * socket
 */
int remote_socket()
{ int s;

  s=network_bind();
  if (s>-1) {
      close(s);
      return 0;
  }
  return -1;
}

/*
 * listener_net_proces()
 * proces listening on network socket, authorisation tests are done here
 * for each connection, handle_request() is called
 */
void *listener_net_proces(void *arg)
{ int s;
  int fd;
  FILE *stream;
  net_sockaddr_t a1;
  socklen_t len;
  int priv=0;

  s=network_bind();
  if (s<0) {
      my_log(LOG_ERR, "network_bind failed");
      exit(EX_OSERR);
  }

  for(;;) {
    bzero((void *)&a1, sizeof(a1));
    len=sizeof(a1);
    if ((fd = accept(s, (struct sockaddr *)&a1, &len)) == -1) {
	  my_log(LOG_ERR, "accept failed: %s", strerror(errno));
          if (errno != ECONNABORTED)
	      exit(EX_OSERR);
          continue;
    }

    if (conf.strictip!=NULL) {
	const char *ip=net_addr_str(net_sockaddr_get_ip(&a1));
	address_list *reg_addr=(address_list *)conf.strictip;

	if (check_address_in_pattern(ip,reg_addr)==NULL) {
	    my_log(LOG_ERR,"connect from inpriviledged ip %s",ip);
	    close(fd);
	    continue;
	}
    }

    if (conf.adminip!=NULL) {
	const char *ip=net_addr_str(net_sockaddr_get_ip(&a1));
	address_list *reg_addr=(address_list *)conf.adminip;

	if (check_address_in_pattern(ip,reg_addr)!=NULL) {
	    priv=1;
	}
    }

    if (conf.strictport!=0) {
	int port;

	port=net_sockaddr_get_port(&a1);
	if (port >1023) {
	    my_log(LOG_ERR,"connect from inpriviledged port %d",port);
	    close(fd);
	    continue;
	}
    }

    stream = fdopen(fd, "w+");
    setvbuf(stream, NULL,_IONBF, 0);
    handle_request(stream,priv);
    //fclose(stream);
  }

  /* not reached */
  my_log(LOG_ERR,"listener_net proces died unexpectedly");
  return NULL;

}

/*
 * listener_local_proces()
 * proces listening on local unix socket
 * for each connection, handle_request() is called
 */
void *listener_local_proces(void *arg)
{
  int s;
  int fd;
  socklen_t len;
  struct sockaddr_un u,u2;
  FILE *stream;
  int ret;

  s=socket(PF_UNIX,SOCK_STREAM,0);
  if (s == -1) {
      my_log(LOG_ERR,"socket failed in listener_local_proces proces: %s",strerror(errno));
      exit(EX_OSERR);
  }
  u.sun_family=AF_UNIX;
  strcpy(u.sun_path,conf.sock);
  ret=bind(s,(struct sockaddr *)&u,sizeof(u));
  if (ret == -1) {
      my_log(LOG_ERR,"bind failed in listener_local_proces proces: %s",strerror(errno));
      exit(EX_OSERR);
  }
  ret=listen(s,100);
  if (ret == -1) {
      my_log(LOG_ERR,"listen failed in listener_local_proces proces: %s",strerror(errno));
      exit(EX_OSERR);
  }
  for(;;) {
    len=sizeof(struct sockaddr);
    fd=accept(s,(struct sockaddr *)&u2,&len);
    stream = fdopen(fd, "w+");
    setvbuf(stream, NULL, _IONBF, 0);
    handle_request(stream,1);
    //fclose(stream);
  }

  /* not reached */
  my_log(LOG_ERR,"listener_local_proces proces died unexpectedly");
  return NULL;

}

struct handle_req {
    FILE *stream;
    int priv;
};

/*
 * handle_request()
 * handle one request - pack all needed information and create new thread
 */
int handle_request(FILE *stream,int priv)
{ struct handle_req *h;
  int ret;
  pthread_t tid;

  if ((h=malloc(sizeof(struct handle_req))) == NULL) {
      my_log(LOG_ERR,"malloc failed in handle_request %s",strerror(errno));
      exit(EX_OSERR);
  }
  h->stream=stream;
  h->priv=priv;
  if ((ret = pthread_create(&tid, NULL, handle_proces, (void *)h)) != 0) {
     my_log(LOG_ERR, "cannot start handle thread: %s", strerror(ret));
     exit(EX_OSERR);
  }
  pthread_detach(tid);
  return 0;
}

/*
 * handle_proces()
 * handle on connection, read commands, send respose ..
 */
void *handle_proces(void *arg)
{ char line[8*1024];
  char *cookie;
  char sep[] = " \n\t\r";
  char *cmd;
  struct handle_req *h=(struct handle_req *)arg;
  FILE *stream;
  int priv;

  priv=h->priv;
  stream=h->stream;
  free(h);

  fprintf(stream, "200 pbs cache 1.0\n");
  fflush(stream);
  for (;;) {
        int fd=fileno(stream);
	struct timeval tv = { conf.socktimeout , 0 };
	fd_set rdfs;
	int ret;

	if (conf.socktimeout>0) {
	    FD_ZERO(&rdfs);
	    FD_SET(fd, &rdfs);
	    if ((ret = select(fd + 1, &rdfs, NULL, NULL,&tv))== -1) {
		my_log(LOG_ERR,"select failed in handle_proces: %s",
		       strerror(errno));
		break;
	    }
	    if (ret==0) {
		my_log(LOG_ERR,"read timeout in handle_proces");
		break;
	    }
	}

	//fprintf(stderr,"pred fgets\n");
	if ((fgets(line, 8*1024-1, stream)) == NULL)
	                            break;
	//fprintf(stderr,"handle: %s",line);
	fflush(stream);
	/*
	 TODO check lenght and return "too long" if needed
	 */
	cookie = NULL;
        if ((cmd = strtok_r(line, sep, &cookie)) == NULL) {
                        fprintf(stream, "101 No command\n");
                        fflush(stream);
                        continue;
        }
        if (strncmp(cmd,"quit",3) == 0) {
           break;
        } else if (strncmp(cmd,"add",3) == 0) {
           char *hostname;
           char *name;
	   char *value;

           if ((hostname = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command add\n");
                        fflush(stream);
                        continue;
           }

           if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command add\n");
                        fflush(stream);
                        continue;
           }
           if ((value = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command add\n");
                        fflush(stream);
                        continue;
           }

           /*
           { time_t now=time(NULL);
	   fprintf(stderr,"update %s %s %s %s\n",ctime(&now),hostname,name,value);
	   }
	   */

           if (memory_update(hostname,name,value,0,0) !=0) {
	       fprintf(stream, "110 error\n");
	   } else {
	       fprintf(stream, "201 OK add\n");
	   }
           fflush(stream);
        } else if (strncmp(cmd,"radd",4) == 0) {
           char *hostname;
           char *name;
	   char *value;
	   char *timestamp;

           if ((hostname = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command radd\n");
                        fflush(stream);
                        continue;
           }

           if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command radd\n");
                        fflush(stream);
                        continue;
           }
           if ((timestamp = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command radd\n");
                        fflush(stream);
                        continue;
           }
           if ((value = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command radd\n");
                        fflush(stream);
                        continue;
           }
           if (memory_update(hostname,name,value,atol(timestamp),0) !=0) {
	       fprintf(stream, "110 error\n");
	   } else {
	       fprintf(stream, "201 OK radd\n");
	   }
           fflush(stream);
        } else if (strncmp(cmd,"show",4) == 0) {
           char *hostname;
	   char *name;
           char *value;
	   time_t timestamp;

           if ((hostname = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command show\n");
                        fflush(stream);
                        continue;
           }

           if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command show\n");
                        fflush(stream);
                        continue;
           }

           if ((value=memory_find(hostname,name,&timestamp))!=NULL)
	       fprintf(stream, "201\t%ld\t%s\n",timestamp,value);
	   else
	       fprintf(stream, "110 not found\n");
           fflush(stream);
	   /*
	   { time_t now=time(NULL);
	     fprintf(stderr,"show %s %s %s %s\n",ctime(&now),hostname,name,value);
	   }
	   */

	   if (value) free(value);
	} else if (strncmp(cmd,"list",4) == 0) {
	    char *name;
	    struct metric *m;

	    if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
		fprintf(stream, "103 Incomplete command list\n");
		fflush(stream);
		continue;
	    }

	    for (m=metrics;m;m=m->next) {
		if (strcmp(m->name,name)==0) {
		    fprintf(stream, "205 list ok\n");
		    fflush(stream);
		    memory_dump(name,stream,1);
		    fprintf(stream, "201 OK list\n");
		    break;
		}
	    }
	    if (m==NULL)
		fprintf(stream, "110 not found\n");
	    fflush(stream);
	} else if (strncmp(cmd,"metrics",7) == 0) {
	    struct metric *m;
            struct metric_source *s;

	    fprintf(stream, "205 metrics ok\n");
	    fflush(stream);
	    for(m=metrics;m;m=m->next) {
             for(s=m->source;s;s=s->next) {
		fprintf(stream, "%s\t%d\t%s\n",m->name,s->type,s->server);
		fflush(stream);
	     }
	    }
	    fprintf(stream, "201 OK metrics\n");
	    fflush(stream);
	} else if (strncmp(cmd,"metric",6) == 0) {
	    struct metric *m;
	    struct metric_source *s;
	    char *name;
	    int ret=0;

	    if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
		fprintf(stream, "103 Incomplete command metric\n");
		fflush(stream);
		continue;
	    }

	    for(m=metrics;m;m=m->next) {
		if (strcmp(m->name,name)==0) {
		    fprintf(stream, "205 metric ok\n");
		    for(s=m->source;s;s=s->next) {
			 fprintf(stream, "%s\t%d\t%s\n",
				 m->name,s->type,s->server);
			 fflush(stream);
		    }
		    ret=1;
		    break;
		}
	    }
	    if (ret==0) {
		fprintf(stream, "110 not found\n");
	        fflush(stream);
	    } else {
		fprintf(stream, "201 OK metric\n");
		fflush(stream);
	    }
	} else if (strncmp(cmd,"shutdown",8) == 0) {
	    if (!priv) {
		fprintf(stream, "102 authorization failed\n");
		fflush(stream);
		break;
	    }
	    fprintf(stream, "201 OK shutdown\n");
	    fflush(stream);
	    exit(0);
	} else if (strncmp(cmd,"del",3) == 0) {
            char *hostname;
            char *name;

	    if (!priv) {
		fprintf(stream, "102 authorization failed\n");
		fflush(stream);
		break;
	    }

            if ((hostname = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command del\n");
                        fflush(stream);
                        continue;
            }

            if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
                        fprintf(stream, "103 Incomplete command del\n");
                        fflush(stream);
                        continue;
            }

            if (memory_remove(hostname,name,0)!=0)
		fprintf(stream, "110 not found\n");
	    else
		fprintf(stream, "201 OK del\n");
            fflush(stream);
	} else if (strncmp(cmd,"info",4) == 0) {
	    char *hostname;
	    char *value;
	    struct metric *m;
	    time_t timestamp;

	    if ((hostname = strtok_r(NULL, sep, &cookie)) == NULL) {
		fprintf(stream, "103 Incomplete command info\n");
		fflush(stream);
		continue;
	    }

	    /*
	     TODO don't send "info ok" unless hostname is really found
	     */
	    fprintf(stream, "205 info ok\n");
	    fflush(stream);
	    for(m=metrics;m;m=m->next) {
		if ((value=memory_find_host(m,hostname,&timestamp))!=NULL) {
		    fprintf(stream, "%s\t%ld\t%s\n",m->name,timestamp,value);
		    fflush(stream);
		    free(value);
		}
	    }
	    fprintf(stream, "201 OK info\n");
	    fflush(stream);
	} else if (strncmp(cmd,"new",3) == 0) {
	    char *name;

	    if (!priv) {
		fprintf(stream, "102 authorization failed\n");
		fflush(stream);
		break;
	    }

	    if ((name = strtok_r(NULL, sep, &cookie)) == NULL) {
		fprintf(stream, "103 Incomplete command new\n");
		fflush(stream);
		continue;
	    }

	    if (memory_add_metric(name,M_REMOTE,NULL,0,0,NULL)!=0)
		fprintf(stream, "110 error\n");
	    else
		fprintf(stream, "201 OK new\n");
	    fflush(stream);
	} else if (strncmp(cmd,"help",4) == 0) {
	    fprintf(stream, "205 help ok, %d\n",10);
	    fflush(stream);
            fprintf(stream, "add hostname metric_name value\n");
	    fflush(stream);
            fprintf(stream, "show hostname metric_name\n");
	    fflush(stream);
            fprintf(stream, "del hostname metric_name\n");
	    fflush(stream);
            fprintf(stream, "list metric\n");
	    fflush(stream);
            fprintf(stream, "new metric\n");
	    fflush(stream);
            fprintf(stream, "info host\n");
	    fflush(stream);
            fprintf(stream, "metrics\n");
	    fflush(stream);
            fprintf(stream, "metric name\n");
	    fflush(stream);
            fprintf(stream, "shutdown\n");
	    fflush(stream);
            fprintf(stream, "help\n");
	    fflush(stream);
	    fprintf(stream, "201 OK help\n");
	    fflush(stream);
        } else {
           fprintf(stream, "102 Invalid command \"%s\"\n", cmd);
           fflush(stream);
           continue;
        }
	/*
	 TODO LATER add command AUTH krb5
	 TODO LATER add command for metric removal, will require locking
	 */
  }

  //fprintf(stderr,"konec\n");

  fprintf(stream, "202 Good bye\n");
  fclose(stream);

  pthread_exit(NULL);
  return NULL;
}


