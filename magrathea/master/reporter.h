/* $Id: reporter.h,v 1.12 2008/09/01 09:49:37 xdenemar Exp $ */

/** @file
 * Status reporter for pbs_cache.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.12 $ $Date: 2008/09/01 09:49:37 $
 */

#ifndef REPORTER_H
#define REPORTER_H

#include "../pbs_cache/api.h"


enum metric {
    M_MAGRATHEA,
    M_CLUSTER,
    M_HOST
};


/** Add new status cache to report to.
 *
 * @param host
 *      machine where the cache is running. The function makes its own copy
 *      of the host parameter.
 *
 * @param port
 *      port number the cache is listening on.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
extern int reporter_add(char *host, int port);


/** Start status cache reporter.
 *
 * @return
 *      zeto on success, nonzero otherwise.
 */
extern int reporter_start(void);


/** Add new information to be reported to status cache(s).
 *
 * @param metric
 *      metric to which the change should be reported.
 *
 * @param name
 *      cache entry name.
 *
 * @param value
 *      entry value.
 *
 * @param copy_value
 *      nonzero when the function should make a copy of the value.
 *
 * @return
 *      nothing.
 */
extern void reporter_change(enum metric metric,
                            const char *name,
                            char *value,
                            int copy_value);


/** Propagate all data to status cache(s).
 * The function does not retry failed transmissions.
 */
extern void reporter_propagate(void);

#endif

