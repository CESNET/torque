#ifndef CLOUD_H_
#define CLOUD_H_

#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_job.h"

/** Test if the given resource is a cluster create request
 *
 * Checks for -l cluster=create
 *
 * @param res Resource to be checked
 * @return 1 if give resource is cluster create request, 0 otherwise
 */
int is_cloud_create(resource *res);

/** Determine whether a job is a cloud job
 *
 * Searches for the cluster resource with create value (-l cluster=create)
 *
 * @param pjob Job to determine
 * @return 1 if job is cloud job, 0 if its not a cloud job
 */
int is_cloud_job(job *pjob);

char *switch_nodespec_to_cloud(job  *pjob, char *nodespec);
void cloud_transition_into_prerun(job *pjob);
void cloud_transition_into_running(job *pjob);
void cloud_transition_into_stopped(job *pjob);

#endif /* CLOUD_H_ */
