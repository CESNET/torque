/* $Id: vmm.c,v 1.2 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * VMM specific functions for magrathea master daemon.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.2 $ $Date: 2008/05/29 14:51:49 $
 */

#include "../config.h"

#include "../utils.h"

#include "vmm.h"

#if XEN
extern F_detect(xen);
extern F_select(xen);
#endif

#if VSERVER
extern F_detect(vserver);
extern F_select(vserver);
#endif

const char *vmm_name = NULL;
F_init(*vmm) = NULL;
F_close(*vmm) = NULL;
F_vm_close(*vmm) = NULL;
F_vm_exists(*vmm) = NULL;
F_vm_name2id(*vmm) = NULL;
F_vm_suspend(*vmm) = NULL;
F_vm_resume(*vmm) = NULL;


int vmm_detect(void)
{
#if VMM_AUTO
    log_msg(LOG_INFO, 0, "Auto-detecting virtual machine monitor: %s", VMM);
#endif

#if XEN
    if (xen_detect()) {
        log_msg(LOG_INFO, 0, "Xen hypervisor detected");
        xen_select();
        return 0;
    };
#endif

#if VSERVER
    if (vserver_detect()) {
        log_msg(LOG_INFO, 0, "Linux VServer detected");
        vserver_select();
        return 0;
    }
#endif

    return -1;
}

