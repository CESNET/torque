#ifndef _PROLOG_H
#define _PROLOG_H
#include "license_pbs.h" /* See here for the software license */
#include <pwd.h> /* uid_t, gid_t */

#include "pbs_job.h" /* job */

/* static char *resc_to_string(job *pjob, int aindex, char *buf, int buflen); */

/* static int pelog_err(job *pjob, char *file, int n, char *text); */

/* static void pelogalm(int sig); */

int undo_set_euid_egid(int which, uid_t real_uid, gid_t real_gid, int num_gids, gid_t *real_gids, char *id);

int run_pelog(int which, char *specpelog, job *pjob, int pe_io_type);

#endif /* _PROLOG_H */
