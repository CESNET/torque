#ifndef CGROUP_MANAGER_H
#define CGROUP_MANAGER_H

#include <stdint.h>

  extern int cgroup_use_cpu;
  extern int cgroup_use_mem;

  /** \brief Setup cgroup mount path
   */
  void cgroup_set_base_path(const char *path);

  /** \brief Check status of cgroup on this machine
   *
   *  Detects whether cgroups are mounted and if cgroups are detected, checks for CPU limitation and Memory limitation support.
   *
   *  \returns -1 if cgroups are not found, 0 otherwise
   */
  int cgroup_detect_status();

  /** \brief Create a cgroup with a specific name
   *
   *  \returns -1 if cgroup was not succesfully created, 0 otherwise
   */
  int cgroup_create(const char *name);

  /** \brief Destroy an existing cgroup
   *
   *  \returns -1 if cgroup was not succesfully destroyed, 0 otherwise
   */
  int cgroup_destroy(const char *name);

  /** \brief Get CPU limitation support status
   */
  int cgroup_get_cpu_enabled();

  /** \brief Get MEM limitation support status
   */
  int cgroup_get_mem_enabled();

  /** \brief Read cgroup information
   *
   */
  int cgroup_get_info(const char *name, double *cpu_limit, int64_t *mem_limit, int **pids);

  /** \brief Set CPU limit for a cgroup
   */
  int cgroup_set_cpu_limit(const char *name, double cpu_limit);

  /** \brief Set Mem limit for a cgroup
   */
  int cgroup_set_mem_limit(const char *name, int64_t mem_limit);

  /** \brief Attach a process to a cgroup
   */
  int cgroup_add_process(const char *name, int pid);

  /** \brief Detach a process from a cgroup
   */
  int cgroup_remove_proces(const char *name, int pid);

  int cgroup_add_pids(const char *name, int* pids);
  int cgroup_remove_pids(const char *name, int* pids);

#endif // CGROUP_MANAGER_H
