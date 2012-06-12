#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "pbs_job.h"
#include "mom_func.h"

#define ENVIRONGEN "/usr/bin/mom_environ"

void set_resource_vars(job *pjob, struct var_table *vtable);
void read_environ_script(job *pjob, struct var_table *vtable);

#endif /* RESOURCES_H_ */
