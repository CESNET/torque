/* $Id: status.h,v 1.2 2007/12/10 23:17:00 xdenemar Exp $ */

/** @file
 * Return status codes and conversions to string.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.2 $ $Date: 2007/12/10 23:17:00 $
 */

#ifndef MASTER_STATUS_H
#define MASTER_STATUS_H

/** Bits reserved for status codes. */
#define STATUS_CODE_MASK      0x0fff

/** Bits reserved for status code type. */
#define STATUS_TYPE_MASK      0xf000

/** Bits reserved for module magic. */
#define STATUS_MAGIC_MASK   (~(STATUS_CODE_MASK | STATUS_TYPE_MASK))

/** OK status code. */
#define STATUS_OK             0x0000

/** Warning status code. */
#define STATUS_WARN           0x1000

/** Error status code. */
#define STATUS_ERROR          0x2000

/** Status code magic for domains module. */
#define SMAGIC_DOMAINS      0x010000

/** Status code magic for storage module. */
#define SMAGIC_STORAGE      0x020000

/** Return nonzero when status code is a successful code. */
#define STATUS_IS_OK(sc)        (((sc) & STATUS_TYPE_MASK) == STATUS_OK)

/** Return nonzero when status code is a warning code. */
#define STATUS_IS_WARNING(sc)   (((sc) & STATUS_TYPE_MASK) == STATUS_WARN)

/** Return nonzero when status code is an error code. */
#define STATUS_IS_ERROR(sc)     (((sc) & STATUS_TYPE_MASK) == STATUS_ERROR)


/** Get string representation of a status code.
 *
 * @param[in] status
 *      status code.
 *
 * @return
 *      string representation of the status as returned by appropriate module.
 */
extern const char *status_decode(int status);

#endif

