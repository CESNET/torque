#include "license_pbs.h" /* See here for the software license */
#ifndef _REQ_JOBOBIT_H
#define _REQ_JOBOBIT_H

#include "batch_request.h" /* batch_request */
#include "pbs_job.h" /* job, job_atr */
#include "work_task.h" /* work_task */
#include "attribute.h" /* svrattrl */
#include "list_link.h" /* tlist_head */



/* static char *setup_from(job *pjob, char *suffix); */

struct batch_request *setup_cpyfiles(struct batch_request *preq, job *pjob, char *from, char *to, int direction, int tflag);

/* static int is_joined(job *pjob, enum job_atr ati); */

/* static struct batch_request *return_stdfile(struct batch_request *preq, job *pjob, enum job_atr ati); */

/* static struct batch_request *cpy_stdfile(struct batch_request *preq, job *pjob, enum job_atr ati); */

struct batch_request *cpy_stage(struct batch_request *preq, job *pjob, enum job_atr ati, int direction);

int mom_comm(job *pjob, void (*func)(struct work_task *))

void rel_resc(job *pjob);

int check_if_checkpoint_restart_failed(job *pjob);

int handle_exiting_or_abort_substate(job *pjob);

int handle_returnstd(job *pjob, struct batch_request *preq, int handle, int type);

int handle_stageout(job *pjob, int type, int handle, struct batch_request *preq);

int handle_stagedel(job *pjob, int type, int handle, struct batch_request *preq);

int handle_exited(job *pjob, int handle);

int handle_complete_first_time(job *pjob, int handle);

int handle_complete_second_time(job *pjob);

void on_job_exit(struct work_task *ptask);

void on_job_rerun(struct work_task *ptask);

/* static void wait_for_send(struct work_task *ptask); */

/* static int setrerun(job *pjob); */

int get_used(svrattrl *patlist, char *acctbuf);

#ifdef USESAVEDRESOURCES
void encode_job_used(job *pjob, tlist_head *phead);
#endif /* USESAVEDRESOURCES */

void *req_jobobit(void *vp);

#endif /* _REQ_JOBOBIT_H */
