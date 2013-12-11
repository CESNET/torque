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
int cloud_transition_into_prerun(job *pjob);
void cloud_transition_into_running(job *pjob);
void cloud_transition_into_stopped(job *pjob);
//void reset_alternative_on_node(job *pjob);
void reset_cloud(struct job *pj);
void set_cloud(char *nodename, char *cloud_name);
void clear_cloud(char *nodename);

void clear_alternative(struct pbsnode *np);
void set_alternative(struct pbsnode *np, char *image);
void set_alternative_from_cache(struct pbsnode *np);


int is_cloud_job_internal(job *pjob, const char *nodespec);
job *cloud_make_build_job(job *pjob, char **destin);

#endif /* CLOUD_H_ */
