/*
 * root_task.c
 *
 *  Created on: 21.8.2012
 *      Author: simon
 */

#include "root_task.h"
#include "svrfunc.h"
#include "unistd.h"

extern void post_checkpoint(job *pjob, int  ev);
extern int post_epilogue(job *pjob, int  ev);

int (*root_task_calls[ROOT_TASK_END])() = { NULL, (int (*)())post_checkpoint, post_epilogue };

int save_roottask(job *pjob)
  {
  save_struct((char*)&pjob->ji_momsubt,sizeof(pjob->ji_momsubt));
  save_struct((char*)&pjob->ji_mompost,sizeof(pjob->ji_mompost));

  return 0;
  }

int recov_roottask( int fds, job *pjob)
  {
  char *id = "root_task";

  pjob->ji_mompost = ROOT_TASK_NULL;
  pjob->ji_momsubt = 0;

  if (read(fds, &pjob->ji_momsubt, sizeof(pjob->ji_momsubt)) != sizeof(pjob->ji_momsubt))
    {
    log_err(errno,id,"read");
    pjob->ji_momsubt = 0;
    pjob->ji_mompost = ROOT_TASK_NULL;
    return 1;
    }

  if (read(fds, &pjob->ji_mompost, sizeof(pjob->ji_mompost)) != sizeof(pjob->ji_mompost))
    {
    log_err(errno,id,"read");
    pjob->ji_momsubt = 0;
    pjob->ji_mompost = ROOT_TASK_NULL;
    return 1;
    }

  if (pjob->ji_mompost >= ROOT_TASK_END)
    {
    pjob->ji_momsubt = 0;
    pjob->ji_mompost = ROOT_TASK_NULL;
    return 1;
    }

  return 0;
  }
