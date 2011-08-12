#ifndef _SVR_FUNC_H
#define _SVR_FUNC_H
#include "license_pbs.h" /* See here for the software license */

#include "attribute.h" /* attribute, batch_op */
#include "list_link.h" /* tlist_head */
#include "pbs_job.h" /* job */

int encode_svrstate(attribute *pattr, tlist_head *phead, char *atname, char *rsname, int mode, int perm);

void set_resc_assigned(job *pjob, enum batch_op op);

int ck_checkpoint(attribute *pattr, void *pobject, int mode);

int decode_null(attribute *patr, char *name, char *rn, char *val, int perm);

int set_null(attribute *pattr, attribute *new, enum batch_op op);

int poke_scheduler(attribute *pattr, void *pobj, int actmode);

#endif /* _SVR_FUNC_H */
