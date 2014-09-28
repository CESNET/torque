/*
 * site_pbs_cache_scheduler.h
 *
 *  Created on: 17.4.2011
 *      Author: simon
 */

#ifndef SITE_PBS_CACHE_SCHEDULER_H_
#define SITE_PBS_CACHE_SCHEDULER_H_

extern "C" {
#include "site_pbs_cache.h"
}
#include "data_types.h"

/** \brief Decode magrathea resource */
int magrathea_decode_new(Resource *res, MagratheaState *state, bool &is_rebootable);

/* function for magrathea */
int magrathea_decode(Resource *res, long int *status,long int *count,long int *used,long int *m_free,long int *m_possible);

#endif /* SITE_PBS_CACHE_SCHEDULER_H_ */
