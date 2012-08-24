/*
 * roottask.h
 *
 *  Created on: 21.8.2012
 *      Author: simon
 */

#ifndef ROOTTASK_H_
#define ROOTTASK_H_

#include "pbs_job.h"
#include <errno.h>

int recov_roottask( int fds, job *pjob);
int save_roottask(job *pjob);

enum { ROOT_TASK_NULL, ROOT_TASK_POST_CHECKPOINT, ROOT_TASK_POST_EPILOGUE, ROOT_TASK_END } root_task_ids;

extern int (*root_task_calls[ROOT_TASK_END])();

#endif /* ROOTTASK_H_ */
