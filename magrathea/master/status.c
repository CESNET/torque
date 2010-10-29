/* $Id: status.c,v 1.3 2008/05/21 15:56:16 xdenemar Exp $ */

/** @file
 * Return status codes and conversions to string.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.3 $ $Date: 2008/05/21 15:56:16 $
 */

#include "../config.h"

#include "cpus.h"
#include "domains.h"
#include "storage.h"
#include "status.h"


const char *status_decode(int status)
{
    switch (status & STATUS_MAGIC_MASK) {
    case SMAGIC_DOMAINS:
        return domains_status_code_string(status);

    case SMAGIC_STORAGE:
        return storage_status_code_string(status);

    default:
        if (status == 0)
            return "success";
        else
            return "unknown status code";
    }
}

