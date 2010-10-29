/* $Id: export.c,v 1.2 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * Export symbols for Magrathea kernel helper when compiled as a module.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.2 $ $Date: 2008/05/29 14:51:49 $
 */

#include <linux/module.h>
#include <linux/swap.h>
#include <linux/freezer.h>

EXPORT_SYMBOL(shrink_all_memory);
EXPORT_SYMBOL(try_to_freeze_tasks);
EXPORT_SYMBOL(thaw_tasks);

