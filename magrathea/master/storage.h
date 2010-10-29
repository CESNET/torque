/* $Id: storage.h,v 1.6 2008/08/08 10:59:14 xdenemar Exp $ */

/** @file
 * Manipulation with permanent storage of domains.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.6 $ $Date: 2008/08/08 10:59:14 $
 */

#ifndef MASTER_STORAGE_H
#define MASTER_STORAGE_H

#include <stdio.h>
#include "paths.h"
#include "domains.h"


/** Maximum interval (mins) between two domains_state_save() calls. */
#define SAVE_INTERVAL       10


/** Status codes. */
enum storage_status_codes {
    SCSTO_OK,
    SCSTO_OUT_OF_MEMORY,
    SCSTO_UNINITIALIZED,
    SCSTO_REOPEN,
    SCSTO_OPEN,
    SCSTO_CLOSED,
    SCSTO_SYNTAX,
    SCSTO_EMPTY,
    SCSTO_END_OF_STATUS_CODE_LIST
};


/** Decode status code.
 *
 * @param[in] status
 *      status code.
 *
 * @return
 *      string representation of the status.
 */
extern const char *storage_status_code_string(int status);


/** Initialize permanent storage according to IP and port.
 *
 * @param[in] ip
 *      IP address the master daemon is bound to. @c NULL has to be passed
 *      instead of 0.0.0.0 or :: in which case the IP part will be missing
 *      in the name of the storage file.
 *
 * @param[in] port
 *      port number the master daemon listens on.
 *
 * @return
 *      status code.
 */
extern int storage_init(const char *ip, int port);


/** Open permanent storage.
 *
 * @param[in] write
 *      nonzero when the storage should be open for writing instead of reading.
 *
 * @return
 *      status code.
 */
extern int storage_open(int write);


/** Close permanent storage.
 *
 * @return
 *      nothing.
 */
extern void storage_close(void);


/** Detect end of permanent storage file.
 *
 * @return
 *      nonzero when reading reached end of the file.
 */
extern int storage_eof(void);


/** Store a domain in the permanent storage.
 *
 * @param[in] dom
 *      domain to be stored.
 *
 * @return
 *      status code.
 */
extern int storage_domain_save(struct domain *dom);


/** Load a domain from the permanent storage.
 *
 * @param[inout] dom
 *      structure to which the domain will be loaded.
 *
 * @return
 *      status code.
 */
extern int storage_domain_load(struct domain *dom);

#endif

