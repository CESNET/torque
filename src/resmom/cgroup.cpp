extern "C" {
#include "log.h"
}


#include "cgroup.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>

static const char * cgroup_mem_limit = "memory.limit_in_bytes";
static const char * cgroup_swmem_limit = "memory.memsw.limit_in_bytes";
static const char * cgroup_mem_usage = "memory.usage_in_bytes";
static const char * cgroup_swmem_usage = "memory.memsw.usage_in_bytes";
static const char * cgroup_cpu_quota = "cpu.cfs_quota_us";
static const char * cgroup_cpu_period = "cpu.cfs_period_us";

// Temporary Debian 6 fix
#ifndef SCNd64
#define SCNd64 "ld"
#endif

static char cgroup_path_cpu[PATH_MAX];
static char cgroup_path_mem[PATH_MAX];

void cgroup_set_path_cpu(const char *path)
  {
  strncpy(cgroup_path_cpu,path,PATH_MAX-1);
  }

void cgroup_set_path_mem(const char *path)
  {
  strncpy(cgroup_path_mem,path,PATH_MAX-1);
  }

static int cgroup_detection_mem = 0;
static int cgroup_detection_cpu = 0;
static int cgroup_detected = 0;

int cgroup_get_cpu_enabled()
{
  return cgroup_detection_cpu;
}

int cgroup_get_mem_enabled()
{
  return cgroup_detection_mem;
}


/** \brief Check whether file exists - helper */
static int check_file_exists(const char *format, ...)
  {
  struct stat fs;
  int ret;
  char temp_path[PATH_MAX] = {0};

  va_list valist;
  va_start(valist, format);

  if ((ret = vsprintf(temp_path,format,valist)) < 0)
    {
    va_end(valist);
    return -1;
    }

  if ((ret = stat(temp_path,&fs)) != 0)
    {
    va_end(valist);
    return -1;
    }

  va_end(valist);
  return 0;
  }

/** \brief Create directory helper */
static int create_directory(const char *format, ...)
  {
  int ret;
  struct stat fs;
  char temp_path[PATH_MAX] = {0};

  va_list valist;
  va_start(valist,format);

  if ((ret = vsprintf(temp_path,format,valist)) < 0)
    {
    va_end(valist);
    return -1;
    }

  if ((ret = mkdir(temp_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) != 0)
    {
    va_end(valist);
    return -1;
    }

  if ((ret = stat(temp_path,&fs)) != 0)
    {
    va_end(valist);
    return -1;
    }

  va_end(valist);
  return 0;
  }

/** \brief Read file to buffer helper */
static int read_file_to_buf(char * buff, size_t max, const char *format, ...)
  {
  int ret;
  char temp_path[PATH_MAX] = {0};

  va_list valist;
  va_start(valist,format);

  if ((ret = vsprintf(temp_path,format,valist)) < 0)
    {
    va_end(valist);
    return -1;
    }

  if ((ret = check_file_exists(temp_path)) != 0)
    {
    va_end(valist);
    return -1;
    }

  int cpu_in;

  if ((cpu_in = open(temp_path,O_RDONLY)) == -1)
    {
    va_end(valist);
    return -1;
    }

  int dsize = read(cpu_in,buff,max);
  close(cpu_in);

  return dsize;
  }

/** \brief Write buffer to file helper */
static int write_buf_to_file(char * buff, int size, const char *format, ...)
  {
  int ret;
  char temp_path[PATH_MAX] = {0};

  va_list valist;
  va_start(valist,format);

  if ((ret = vsprintf(temp_path,format,valist)) < 0)
    {
    va_end(valist);
    return -1;
    }

  int cpu_out;

  if ((cpu_out = open(temp_path,O_WRONLY)) == -1)
    {
    va_end(valist);
    return -1;
    }

  if (write(cpu_out,buff,size) != size)
    {
    close(cpu_out);
    va_end(valist);
    return -1;
    }

  close(cpu_out);
  va_end(valist);
  return 0;
  }

/** \brief Detect whether cgroups are enabled on machine */
int cgroup_detect_status()
  {
  if (cgroup_detected == 1)
    return 0;

  int ret;

  if (cgroup_path_cpu[0] != '\0')
    {
    if ((ret = check_file_exists(cgroup_path_cpu)) != 0) // cgroup not found
      {
      snprintf(log_buffer,LOG_BUF_SIZE,"[CGROUP] Could not find cgroup CPU path %s\n",strerror(errno));
      fprintf(stderr,log_buffer);
      log_err(-1,const_cast<char*>("cgroup_detect_status"),log_buffer);
      cgroup_detection_cpu = 0;
      goto memory_detect;
      }

    int cpu_detected = 0;

    if ((ret = check_file_exists("%s/%s",cgroup_path_cpu,cgroup_cpu_quota)) == 0)
      ++cpu_detected;

    if ((ret = check_file_exists("%s/%s",cgroup_path_cpu,cgroup_cpu_period)) == 0)
      ++cpu_detected;

    if (cpu_detected == 2)
      cgroup_detection_cpu = 1;
    else
      {
      cgroup_detection_cpu = 0;
      snprintf(log_buffer,LOG_BUF_SIZE,"[CGROUP] Could not find one of required cpu options (%s,%s).\n",cgroup_cpu_period,cgroup_cpu_quota);
      fprintf(stderr,log_buffer);
      log_err(-1,const_cast<char*>("cgroup_detect_status"),log_buffer);
      }
    }

memory_detect:
  if (cgroup_path_mem[0] != '\0')
    {
    if ((ret = check_file_exists(cgroup_path_mem)) != 0) // cgroup not found
      {
      snprintf(log_buffer,LOG_BUF_SIZE,"[CGROUP] Could not find cgroup MEM path %s\n",strerror(errno));
      fprintf(stderr,log_buffer);
      log_err(-1,const_cast<char*>("cgroup_detect_status"),log_buffer);
      cgroup_detection_mem = 0;
      goto done;
      }

    int mem_detected = 0;

    if ((ret = check_file_exists("%s/%s",cgroup_path_mem,cgroup_mem_limit)) == 0)
      ++mem_detected;

    if ((ret = check_file_exists("%s/%s",cgroup_path_mem,cgroup_swmem_limit)) == 0)
      ++mem_detected;

    if ((ret = check_file_exists("%s/%s",cgroup_path_mem,cgroup_mem_usage)) == 0)
      ++mem_detected;

    if ((ret = check_file_exists("%s/%s",cgroup_path_mem,cgroup_swmem_usage)) == 0)
      ++mem_detected;

    if (mem_detected == 4)
      cgroup_detection_mem = 1;
    else
      {
      cgroup_detection_mem = 0;
      snprintf(log_buffer,LOG_BUF_SIZE,"[CGROUP] Could not find one of required memory options (%s,%s,%s,%s).\n",cgroup_mem_limit,cgroup_swmem_limit,cgroup_mem_usage,cgroup_swmem_usage);
      fprintf(stderr,log_buffer);
      log_err(-1,const_cast<char*>("cgroup_detect_status"),log_buffer);
      }
    }

done:
  cgroup_detected = 1;
  return 0;
  }

/** \brief Initialize cgroup for a job */
int cgroup_create(const char *name)
  {
  int ret;
  if (cgroup_detection_cpu == 1)
    {
    if ((ret = check_file_exists("%s/%s",cgroup_path_cpu,name)) == 0)
      goto cgroup_create_memory; // cgroup already exists

    if ((ret = create_directory("%s/%s",cgroup_path_cpu,name)) != 0)
      return -1;
    }

cgroup_create_memory:
  if (cgroup_detection_mem == 1)
    {
    if ((ret = check_file_exists("%s/%s",cgroup_path_mem,name)) == 0)
      goto cgroup_create_post; // cgroup already exists

    if ((ret = create_directory("%s/%s",cgroup_path_mem,name)) != 0)
      return -1;
    }

cgroup_create_post:
  // UNINITIALIZED CPUSET FIX
  if (cgroup_detection_cpu == 1)
    {
    // we need make sure that the cpuset.cpus and cpuset.mems is initialized correctly
    char buff[8*1024] = {0};
    int bytes;

    if ((bytes = read_file_to_buf(buff,8*1024,"%s/%s",cgroup_path_cpu,"cpuset.cpus")) <= 0)
      return 0;

    if ((ret = write_buf_to_file(buff,bytes,"%s/%s/%s",cgroup_path_cpu,name,"cpuset.cpus")) != 0)
      return 0;
    }

  if (cgroup_detection_cpu == 1)
    {
    char buff[8*1024] = {0};
    int bytes;

    strcpy(buff,"0");
    bytes = 1;
    if ((ret = write_buf_to_file(buff,bytes,"%s/%s/%s",cgroup_path_cpu,name,"cpuset.mems")) != 0)
      return 0;
    }

  return 0;
}

int get_cgroup_cpu_info(const char *name, double *cpu_limit)
  {
  if (cpu_limit == NULL)
    return 0;

  // If CPU cgroup is not enabled, or does not exist yet, gracefully exit
  if ((cgroup_detection_cpu == 0) ||
      (check_file_exists("%s/%s",cgroup_path_cpu,name) != 0))
    {
    *cpu_limit = 0;
    return 0;
    }

  // read CPU limits
  FILE *cpu_period, *cpu_quota;
  char file_path[PATH_MAX] = {0};

  long int cpu_per = 0, cpu_quo = 0;

  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,cgroup_cpu_period);
  cpu_period = fopen(file_path,"r");
  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,cgroup_cpu_quota);
  cpu_quota = fopen(file_path,"r");

  if (cpu_period != NULL && cpu_quota != NULL)
    {
    fscanf(cpu_period,"%ld",&cpu_per);
    fscanf(cpu_quota,"%ld",&cpu_quo);
    *cpu_limit = (double)cpu_quo/(double)cpu_per;
    }
  else // could not open one of the required files
    {
    *cpu_limit = 0;
    return -1;
    }

  if (cpu_period != NULL)
    fclose(cpu_period);
  if (cpu_quota != NULL)
    fclose(cpu_quota);

  return 0;
  }

int get_cgroup_mem_info(const char *name, int64_t *mem_limit, int64_t *swmem_limit, int64_t *mem_usage, int64_t *swmem_usage)
  {
  if (mem_limit == NULL && mem_usage == NULL)
    return 0;

  // If MEM cgroup is not enabled, or does not exist yet, gracefully exit
  if ((mem_limit != NULL) &&
      ((cgroup_detection_mem == 0) ||
      (check_file_exists("%s/%s",cgroup_path_mem,name) != 0)))
    {
    *mem_limit = 0;
    return 0;
    }

  // read memory limit && usage
  FILE *mem_lim = NULL, *swmem_lim = NULL, *mem_use = NULL, *swmem_use = NULL;
  char file_path[PATH_MAX] = {0};

  if (mem_limit != NULL)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_mem_limit);
    mem_lim = fopen(file_path,"r");
    }

  if (swmem_limit != NULL)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_swmem_limit);
    swmem_lim = fopen(file_path,"r");
    }

  if (mem_usage != NULL)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_mem_usage);
    mem_use = fopen(file_path,"r");
    }

  if (swmem_usage != NULL)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_swmem_usage);
    swmem_use = fopen(file_path,"r");
    }

  int64_t cap;
  if (mem_lim != NULL && mem_limit != NULL)
    {
    fscanf(mem_lim,"%" SCNd64,&cap);
    *mem_limit = cap;
    }
  // if requested and not read - report error
  else if (mem_limit != NULL && mem_lim == NULL)
    {
    *mem_limit = 0;
    return -1;
    }

  if (swmem_lim != NULL && swmem_limit != NULL)
    {
    fscanf(swmem_lim,"%" SCNd64,&cap);
    *swmem_limit = cap;
    }
  // if requested and not read - report error
  else if (swmem_limit != NULL && swmem_lim == NULL)
    {
    *swmem_limit = 0;
    return -1;
    }

  if (mem_use != NULL && mem_usage != NULL)
    {
    fscanf(mem_use,"%" SCNd64,&cap);
    *mem_usage = cap;
    }
  // if requested and not read - report error
  else if (mem_usage != NULL && mem_use == NULL)
    {
    *mem_usage = 0;
    return -1;
    }

  if (swmem_use != NULL && swmem_usage != NULL)
    {
    fscanf(swmem_use,"%" SCNd64,&cap);
    *swmem_usage = cap;
    }
  else if (swmem_usage != NULL && swmem_use == NULL)
    {
    *swmem_usage = 0;
    return -1;
    }

  if (mem_use != NULL)
    fclose(mem_use);
  if (swmem_usage != NULL)
    fclose(swmem_use);
  if (mem_lim != NULL)
    fclose(mem_lim);
  if (swmem_lim != NULL)
    fclose(swmem_lim);

  return 0;
  }

int get_cgroup_exists(const char *name)
  {
  int ret;

  // CPU cgroup enabled and not present
  if (cgroup_detection_cpu != 0 && (ret = check_file_exists("%s/%s",cgroup_path_cpu,name)) != 0)
    return -1;

  // MEM cgroup enabled and not present
  if (cgroup_detection_mem != 0 && (ret = check_file_exists("%s/%s",cgroup_path_mem,name)) != 0)
    return -1;

  return 0;
  }

int get_cgroup_pid_info(const char *name, int **pids)
  {
  // as we are keeping pids in sync between multiple cgroup groups (cpu/mem)
  // we will read te list prefferably from CPU and if that is not enabled/fails
  // try to read it from MEM cgroup
  size_t cap = 1024;
  size_t size = 0;

  if (pids == NULL)
    return 0;

  FILE *tasks = NULL;
  char file_path[PATH_MAX] = {0};

  if (cgroup_detection_cpu != 0)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,"tasks");
    tasks = fopen(file_path,"r");
    }

  if (tasks == NULL && cgroup_detection_mem != 0)
    {
    sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,"tasks");
    tasks = fopen(file_path,"r");
    }

  if (tasks == NULL)
    return -1;

  int *buff = static_cast<int*>(malloc(1024*sizeof(int)));
  if (buff == NULL) // memory error
    goto cgroup_pid_err;

  buff[0] = -1;

  int single_pid;
  while (fscanf(tasks,"%d",&single_pid) == 1)
  {
    if (cap == size+1)
    {
      int *tmp_buff = static_cast<int*>(realloc(buff,cap*2*sizeof(int)));
      if (tmp_buff == NULL) // memory error
        goto cgroup_pid_err2;


      cap*=2;
      buff=tmp_buff;
    }

    buff[size] = single_pid;
    buff[size+1] = -1;
    ++size;
  }

  *pids = buff;
  return 0;

cgroup_pid_err2:
  free(buff);

cgroup_pid_err:
  fclose(tasks);
  return -1;
  }

/** \brief Attach a process to a cgroup
 */
int cgroup_add_pid(const char *name, int pid)
{
  FILE *tasks;
  char file_path[PATH_MAX] = {0};

  if (cgroup_detection_cpu == 0)
    goto cgroup_add_pid_memory;

  if (check_file_exists("%s/%s",cgroup_path_cpu,name) != 0)
    {
    if (cgroup_create(name) != 0)
      return -1;
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    {
    printf("Could not open tasks file : %s\n",strerror(errno));
    return -1;
    }

  if (fprintf(tasks,"%d\n",pid) <= 0)
    return -1;

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_add_pid"), log_buffer);
    fprintf(stderr,"cgroup_add_pid() : %s\n",log_buffer);
    }

  // if the cgroup does not exist yet, create it

cgroup_add_pid_memory:
  if (cgroup_detection_mem == 0 || strcmp(cgroup_path_cpu,cgroup_path_mem) == 0)
    return 0;

  if (check_file_exists("%s/%s",cgroup_path_mem,name) != 0)
    {
    if (cgroup_create(name) != 0)
      return -1;
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    return -1;

  if (fprintf(tasks,"%d\n",pid) <= 0)
    return -1;

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_add_pid"), log_buffer);
    fprintf(stderr,"cgroup_add_pid() : %s\n",log_buffer);
    }

  return 0;
}

/** \brief Attach a process to a cgroup
 */
int cgroup_add_pids(const char *name, int* pids)
{
  FILE *tasks;
  char file_path[PATH_MAX] = {0};
  int *i;

  if (cgroup_detection_cpu == 0)
    goto cgroup_add_pids_memory;

  if (check_file_exists("%s/%s",cgroup_path_cpu,name) != 0)
    {
    if (cgroup_create(name) != 0)
      return -1;
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    {
    printf("Could not open %s file : %s\n",file_path,strerror(errno));
    return -1;
    }

  i = pids;
  while (*i > 0)
    {
    if (fprintf(tasks,"%d\n",*i) <= 0)
      return -1;
    ++i;
    }

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_add_pids"), log_buffer);
    fprintf(stderr,"cgroup_add_pids() : %s\n",log_buffer);
    }

  // if the cgroup does not exist yet, create it

cgroup_add_pids_memory:
  if (cgroup_detection_mem == 0 || strcmp(cgroup_path_cpu,cgroup_path_mem) == 0)
    return 0;

  if (check_file_exists("%s/%s",cgroup_path_mem,name) != 0)
    {
    if (cgroup_create(name) != 0)
      return -1;
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    {
    printf("Could not open %s file : %s\n",file_path,strerror(errno));
    return -1;
    }

  i = pids;
  while (*i > 0)
    {
    if (fprintf(tasks,"%d\n",*i) <= 0)
      return -1;
    ++i;
    }

  if (tasks != NULL && fclose(tasks))
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_add_pids"), log_buffer);
    fprintf(stderr,"cgroup_add_pids() : %s\n",log_buffer);
    }

  return 0;
}

/** \brief Detach a process from a cgroup
 */
int cgroup_remove_pids(int* pids)
{
  FILE *tasks;
  char file_path[PATH_MAX] = {0};
  int *i;

  if (cgroup_detection_cpu == 0)
    goto cgroup_remove_pids_memory;

  sprintf(file_path,"%s/%s",cgroup_path_cpu,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    return -1;

  i = pids;
  while (*i > 0)
    {
    if (fprintf(tasks,"%d\n",*i) <= 0)
      return -1;
    ++i;
    }

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_remove_pids"), log_buffer);
    fprintf(stderr,"cgroup_remove_pids() : %s\n",log_buffer);
    }

  // if the cgroup does not exist yet, create it

cgroup_remove_pids_memory:
  if (cgroup_detection_mem == 0 || strcmp(cgroup_path_cpu,cgroup_path_mem) == 0)
    return 0;

  sprintf(file_path,"%s/%s",cgroup_path_mem,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    return -1;

  i = pids;
  while (*i > 0)
    {
    if (fprintf(tasks,"%d\n",*i) <= 0)
      return -1;
    ++i;
    }

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_remove_pids"), log_buffer);
    fprintf(stderr,"cgroup_remove_pids() : %s\n",log_buffer);
    }


  return 0;
}

/** \brief Detach a process from a cgroup
 */
int cgroup_remove_pid(int pid)
{
  FILE *tasks;
  char file_path[PATH_MAX] = {0};

  if (cgroup_detection_cpu == 0)
    goto cgroup_remove_pid_memory;

  sprintf(file_path,"%s/%s",cgroup_path_cpu,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    return -1;

  if (fprintf(tasks,"%d\n",pid) <= 0)
    return -1;

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_remove_pid"), log_buffer);
    fprintf(stderr,"cgroup_remove_pid() : %s\n",log_buffer);
    }

  // if the cgroup does not exist yet, create it

cgroup_remove_pid_memory:
  if (cgroup_detection_mem == 0 || strcmp(cgroup_path_cpu,cgroup_path_mem) == 0)
    return 0;

  sprintf(file_path,"%s/%s",cgroup_path_mem,"tasks");
  if ((tasks = fopen(file_path,"a")) == NULL)
    return -1;

  if (fprintf(tasks,"%d\n",pid) <= 0)
    return -1;

  if (tasks != NULL && fclose(tasks) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush tasks : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_remove_pid"), log_buffer);
    fprintf(stderr,"cgroup_remove_pid() : %s\n",log_buffer);
    }

  return 0;
}

int cgroup_destroy(const char *name)
{
  pid_t *buff = NULL;
  if (get_cgroup_pid_info(name,&buff) != 0)
    return -1;

  if (cgroup_remove_pids(buff) != 0)
    {
    free(buff);
    return -1;
    }

  free(buff);

  char temp_path[PATH_MAX] = {0};
  if (cgroup_detection_cpu != 0)
    {
    sprintf(temp_path,"%s/%s",cgroup_path_cpu,name);
    rmdir(temp_path);
    }

  if (cgroup_detection_mem != 0 && strcmp(cgroup_path_cpu,cgroup_path_mem) != 0)
    {
    sprintf(temp_path,"%s/%s",cgroup_path_mem,name);
    rmdir(temp_path);
    }

  return 0;
}






int cgroup_set_cpu_limit(const char *name, double cpu_limit)
  {
  if (cgroup_detection_cpu == 0)
    return 0;

  FILE *cpu_period, *cpu_quota;
  char file_path[PATH_MAX] = {0};

  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,cgroup_cpu_period);
  cpu_period = fopen(file_path,"w");
  if (cpu_period == NULL)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not open %s file : %s\n",file_path,strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_cpu_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_cpu_limit() : %s\n",log_buffer);
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_cpu,name,cgroup_cpu_quota);
  cpu_quota = fopen(file_path,"w");

  if (cpu_quota == NULL)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not open %s file : %s\n",file_path,strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_cpu_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_cpu_limit() : %s\n",log_buffer);
    }

  if (cpu_period != NULL && cpu_quota != NULL)
    {
    if (fprintf(cpu_period,"%ld",1000000L) <= 0)
      return -1;
    if (fprintf(cpu_quota,"%ld",static_cast<long int>(1000000*cpu_limit)) <= 0)
      return -1;
    }

  if (cpu_period != NULL && fclose(cpu_period) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush cpu_period : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_mem_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_mem_limit() : %s\n",log_buffer);
    }

  if (cpu_quota != NULL && fclose(cpu_quota) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush cpu_quota : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_cpu_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_cpu_limit() : %s\n",log_buffer);
    }

  return 0;
  }

/** \brief Set Mem limit for a cgroup
 */
int cgroup_set_mem_limit(const char *name, int64_t mem_limit)
  {
  if (cgroup_detection_mem == 0)
    return 0;

  FILE *mem, *swmem;
  char file_path[PATH_MAX] = {0};

  sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_mem_limit);
  mem = fopen(file_path,"w");
  if (mem == NULL)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not open %s file : %s\n",file_path,strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_mem_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_mem_limit() : %s\n",log_buffer);
    }

  sprintf(file_path,"%s/%s/%s",cgroup_path_mem,name,cgroup_swmem_limit);
  swmem = fopen(file_path,"w");
  if (swmem == NULL)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not open %s file : %s\n",file_path,strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_mem_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_mem_limit() : %s\n",log_buffer);
    }

  if (mem != NULL)
    {
    if (fprintf(mem,"%" SCNd64,mem_limit) <= 0)
      return -1;
    }

  if (swmem != NULL)
    {
    if (fprintf(swmem,"%" SCNd64,mem_limit) <= 0)
      return -1;
    }

  if (mem != NULL && fclose(mem) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush memory : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_mem_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_mem_limit() : %s\n",log_buffer);
    }

  if (swmem != NULL && fclose(swmem) != 0)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"Could not close/flush swmemory : %s\n",strerror(errno));
    log_err(-1,const_cast<char*>("cgroup_set_mem_limit"), log_buffer);
    fprintf(stderr,"cgroup_set_mem_limit() : %s\n",log_buffer);
    }

  return 0;
  }
