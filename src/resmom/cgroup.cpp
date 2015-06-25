extern "C" {
#include "cgroup.h"
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>


static char cgroup_path[PATH_MAX];
static int cgroup_detection_mem = 0;
static int cgroup_detection_cpu = 0;

int cgroup_detect_status()
{
  struct stat fs;
  int ret = stat(cgroup_path,&fs);
  if (ret != 0) // cgroup not found
  {
    printf("Could not find cgroup path\n");
    cgroup_detection_cpu = 0;
    cgroup_detection_mem = 0;
    return -1;
  }

  char temp_path[PATH_MAX] = {0};
  int cpu_detected = 0;

  sprintf(temp_path,"%s/cpu.cfs_quota_us",cgroup_path);
  ret = stat(temp_path,&fs);
  if (ret == 0)
    ++cpu_detected;

  sprintf(temp_path,"%s/cpu.cfs_period_us",cgroup_path);
  ret = stat(temp_path,&fs);
  if (ret == 0)
    ++cpu_detected;

  if (cpu_detected == 2)
    cgroup_detection_cpu = 1;
  else
    cgroup_detection_cpu = 0;

  sprintf(temp_path,"%s/memory.limit_in_bytes",cgroup_path);
  ret = stat(temp_path,&fs);
  if (ret == 0)
    cgroup_detection_mem = 1;
  else
    cgroup_detection_mem = 0;

  return 0;
}

int cgroup_create(const char *name)
{
  struct stat fs;
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);
  int ret = stat(temp_path,&fs);
  if (ret != 0) // cgroup does not exist yet, we need to create it
  {
    if (mkdir(temp_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
    // log error
    {
      return -1;
    }

    ret = stat(temp_path,&fs);
    if (ret != 0) // mkdir succeeded, but cgroup was not created
    // log error
    {
      return -1;
    }

    // copy cpuset cpu & mem from parent cgroup
    off_t bytesCopied = 0;

    sprintf(temp_path,"%s/%s",cgroup_path,"cpuset.cpus");
    ret = stat(temp_path,&fs);
    if (ret == 0)
      {
      int cpu_in,cpu_out;
      sprintf(temp_path,"%s/%s",cgroup_path,"cpuset.cpus");
      cpu_in = open(temp_path,O_RDONLY);
      sprintf(temp_path,"%s/%s/%s",cgroup_path,name,"cpuset.cpus");
      cpu_out = open(temp_path,O_WRONLY);
      ret = fstat(cpu_in,&fs);

      if (cpu_in != -1 && cpu_out != -1 && ret == 0)
        sendfile(cpu_in,cpu_out,&bytesCopied,fs.st_size);
      // TODO error handling
      }

    sprintf(temp_path,"%s/%s",cgroup_path,"cpuset.mems");
    ret = stat(temp_path,&fs);
    if (ret == 0)
      {
      int mem_in,mem_out;
      sprintf(temp_path,"%s/%s",cgroup_path,"cpuset.mems");
      mem_in = open(temp_path,O_RDONLY);
      sprintf(temp_path,"%s/%s/%s",cgroup_path,name,"cpuset.m");
      mem_out = open(temp_path,O_WRONLY);
      ret = fstat(mem_in,&fs);

      if (mem_in != -1 && mem_out != -1 && ret == 0)
        sendfile(mem_in,mem_out,&bytesCopied,fs.st_size);
      // TODO error handling
      }
  }

  return 0;
}

int cgroup_destroy(const char *name)
{
  struct stat fs;
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return 0;

  char pidlist[PATH_MAX] = {0};
  sprintf(pidlist,"%s/%s",temp_path,"tasks");
  FILE *f = fopen(pidlist,"w");
  if (f == NULL)
  // log error
  {
    return -1;
  }
  fclose(f);

  if (rmdir(temp_path) != 0)
  // log error
  {
    return -1;
  }

  return 0;
}

void cgroup_set_base_path(const char *path)
{
  strncpy(cgroup_path,path,PATH_MAX-1);
}

int cgroup_get_cpu_enabled()
{
  return cgroup_detection_cpu;
}

int cgroup_get_mem_enabled()
{
  return cgroup_detection_mem;
}

int cgroup_get_info(const char *name, double *cpu_limit, int64_t *mem_limit, pid_t **pids)
{
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cpu_limit != NULL && cgroup_detection_cpu != 0)
  {

    FILE *cpu_period, *cpu_quota;
    char file_path[PATH_MAX] = {0};

    long int cpu_per = 0, cpu_quo = 0;

    sprintf(file_path,"%s/%s",temp_path,"cpu.cfs_period_us");
    cpu_period = fopen(file_path,"r");
    sprintf(file_path,"%s/%s",temp_path,"cpu.cfs_quota_us");
    cpu_quota = fopen(file_path,"r");


    if (cpu_period != NULL && cpu_quota != NULL)
    {
      fscanf(cpu_period,"%ld",&cpu_per);
      fscanf(cpu_quota,"%ld",&cpu_quo);
      *cpu_limit = (double)cpu_quo/(double)cpu_per;
    }

    if (cpu_period != NULL)
      fclose(cpu_period);
    if (cpu_quota != NULL)
      fclose(cpu_quota);
  }


  if (mem_limit != NULL && cgroup_detection_mem != 0)
  {
    FILE *mem;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"memory.limit_in_bytes");
    mem = fopen(file_path,"r");
    int64_t cap;
    if (mem != NULL)
    {
      fscanf(mem,"%ld",&cap);
      *mem_limit = cap;
    }

    if (mem != NULL)
      fclose(mem);
  }

  if (pids != NULL)
  {
    FILE *tasks;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"tasks");
    tasks = fopen(file_path,"r");

    int *buff = static_cast<int*>(malloc(1024*sizeof(int)));
    if (buff == NULL) // memory error
      return -1;

    size_t cap = 1024;
    size_t size = 0;
    buff[0] = -1;

    int single_pid;

    while (fscanf(tasks,"%d",&single_pid) == 1)
    {
      if (cap == size+1)
      {
        int *tmp_buff = static_cast<int*>(realloc(buff,cap*2*sizeof(int)));
        if (tmp_buff == NULL) // memory error
          return -1;

        cap*=2;
        buff=tmp_buff;
      }

      buff[size] = single_pid;
      buff[size+1] = -1;
      ++size;
    }

    fclose(tasks);

    *pids = buff;
  }

  return 0;
}

int cgroup_set_cpu_limit(const char *name, double cpu_limit)
{
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_cpu != 0)
  {

    FILE *cpu_period, *cpu_quota;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"cpu.cfs_period_us");
    cpu_period = fopen(file_path,"w");
    sprintf(file_path,"%s/%s",temp_path,"cpu.cfs_quota_us");
    cpu_quota = fopen(file_path,"w");

    if (cpu_period != NULL && cpu_quota != NULL)
    {
      if (fprintf(cpu_period,"%ld",1000000L) <= 0)
        return -1;
      if (fprintf(cpu_quota,"%ld",static_cast<long int>(1000000*cpu_limit)) <= 0)
        return -1;
    }

    if (cpu_period != NULL)
      fclose(cpu_period);
    if (cpu_quota != NULL)
      fclose(cpu_quota);
  }

  return 0;
}

/** \brief Set Mem limit for a cgroup
 */
int cgroup_set_mem_limit(const char *name, int64_t mem_limit)
{
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_mem != 0)
  {
    FILE *mem;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"memory.limit_in_bytes");
    mem = fopen(file_path,"w");

    if (mem != NULL)
    {
      if (fprintf(mem,"%ld",mem_limit) <= 0)
        return -1;
    }

    if (mem != NULL)
      fclose(mem);
  }

  return 0;
}

/** \brief Attach a process to a cgroup
 */
int cgroup_add_process(const char *name, int pid)
{
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_mem != 0 || cgroup_detection_cpu != 0)
  {
    FILE *tasks;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"tasks");
    tasks = fopen(file_path,"a");

    if (tasks != NULL)
    {
      if (fprintf(tasks,"%d\n",pid) <= 0)
        return -1;
    }

    if (tasks != NULL)
      fclose(tasks);
  }

  return 0;
}

/** \brief Detach a process from a cgroup
 */
int cgroup_remove_proces(const char *name, int pid)
{
  // to remove a process from a cgroup we need
  // to write it to the root cgroup task file

  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s",cgroup_path);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_mem != 0 || cgroup_detection_cpu != 0)
  {
    FILE *tasks;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"tasks");
    tasks = fopen(file_path,"a");

    if (tasks != NULL)
    {
      if (fprintf(tasks,"%d\n",pid) <= 0)
        return -1;
    }

    if (tasks != NULL)
      fclose(tasks);
  }

  return 0;
}

/** \brief Attach a process to a cgroup
 */
int cgroup_add_pids(const char *name, int* pids)
{
  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s/%s",cgroup_path,name);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_mem != 0 || cgroup_detection_cpu != 0)
  {
    FILE *tasks;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"tasks");
    tasks = fopen(file_path,"a");

    if (tasks != NULL)
    {
      int *i = pids;
      while (*i > 0)
        {
        if (fprintf(tasks,"%d\n",*i) <= 0)
          return -1;
        ++i;
        }
    }

    if (tasks != NULL)
      fclose(tasks);
  }

  return 0;
}

/** \brief Detach a process from a cgroup
 */
int cgroup_remove_pids(const char *name, int* pids)
{
  // to remove a process from a cgroup we need
  // to write it to the root cgroup task file

  char temp_path[PATH_MAX] = {0};
  sprintf(temp_path,"%s",cgroup_path);

  struct stat fs;
  int ret = stat(temp_path,&fs);
  if (ret != 0) // this cgroup does not exist
    return -1;

  if (cgroup_detection_mem != 0 || cgroup_detection_cpu != 0)
  {
    FILE *tasks;
    char file_path[PATH_MAX] = {0};

    sprintf(file_path,"%s/%s",temp_path,"tasks");
    tasks = fopen(file_path,"a");

    if (tasks != NULL)
    {
      int *i = pids;
      while (*i > 0)
        {
        if (fprintf(tasks,"%d\n",*i) <= 0)
          return -1;
        ++i;
        }
    }

    if (tasks != NULL)
      fclose(tasks);
  }

  return 0;
}

