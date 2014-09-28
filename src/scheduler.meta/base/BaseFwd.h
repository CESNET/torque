#ifndef BASE_FWD_H_
#define BASE_FWD_H_

#include "config.h"
#include "constant.h"

struct server_info;

/* resources can get too large for a 32 bit number, so the ability to use the
 * nonstandard type long long is necessary.
 */
typedef RESOURCE_TYPE sch_resource_t;
typedef sch_resource_t usage_t;

enum CheckResult { CheckAvailable, CheckOccupied, CheckNonFit };

#endif
