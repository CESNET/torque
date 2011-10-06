#include "license_pbs.h" /* See here for the software license */
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h> /* struct sockaddr */
#include <netinet/in.h> /* in_addr_t */


#include "log.h" /* LOG_BUF_SIZE */
#include "resizable_array.h" /* resizable_array */

int pbs_errno;
time_t time_now;
char log_buffer[LOG_BUF_SIZE];

int insert_thing(resizable_array *ra, void *thing)
  { 
  fprintf(stderr, "The call to insert_thing needs to be mocked!!\n");
  exit(1);
  }

void DIS_tcp_reset(int fd, int i)
  { 
  fprintf(stderr, "The call to DIS_tcp_reset needs to be mocked!!\n");
  exit(1);
  }

int diswsl(int stream, long value)
  { 
  fprintf(stderr, "The call to diswsl needs to be mocked!!\n");
  exit(1);
  }

int DIS_tcp_wflush(int fd)
  { 
  fprintf(stderr, "The call to DIS_tcp_wflush needs to be mocked!!\n");
  exit(1);
  }

resizable_array *initialize_resizable_array(int size)
  { 
  fprintf(stderr, "The call to initialize_resizable_array needs to be mocked!!\n");
  exit(1);
  }

long disrsl(int stream, int *retval)
  { 
  fprintf(stderr, "The call to disrsl needs to be mocked!!\n");
  exit(1);
  }

void log_err(int errnum, char *routine, char *text)
  { 
  fprintf(stderr, "The call to log_err needs to be mocked!!\n");
  exit(1);
  }

int socket_connect_addr(int *local_socket, struct sockaddr *remote, size_t remote_size, int is_privileged, char **err_msg)
  { 
  fprintf(stderr, "The call to socket_connect_addr needs to be mocked!!\n");
  exit(1);
  }

int socket_get_tcp_priv(in_addr_t *s_addr)
  { 
  fprintf(stderr, "The call to socket_get_tcp_priv needs to be mocked!!\n");
  exit(1);
  }

