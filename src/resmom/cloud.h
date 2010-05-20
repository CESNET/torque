#ifndef CLOUD_H_
#define CLOUD_H_

#include "pbs_job.h"

/** Determine if job is a cloud job
 *
 * Contains -lcluster=create resource
 *
 * @param pjob Job to be determined
 * @return 0 - when not cloud, 1 - if cloud
 */
int is_cloud_job(job *pjob);


/** Set job into prerun state
 *
 * Used when prologs are running.
 */
int cloud_set_prerun(job *pjob);

/** Set job into running state
 *
 * Used when cluster is ready for use.
 */
int cloud_set_running(job *pjob);

int cloud_exec(job *pjob);

#endif /* CLOUD_H_ */
