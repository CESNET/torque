/* $Id: vserver.c,v 1.10 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * VServer-specific functions for magrathea master daemon.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.10 $ $Date: 2008/05/29 14:51:49 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/* XXX hack; vserver.h doesn't define these types by itself */
typedef uint32_t xid_t;
typedef uint32_t nid_t;
typedef uint32_t tag_t;
#include <vserver.h>

#include "../utils.h"
#include "vmm.h"


/** Path to VMs network related configuration. */
#define VIRTNET_DIR         "/proc/virtnet"


F_detect(vserver)
{
    return (access(VIRTNET_DIR, R_OK | X_OK) == 0);
}


F_init(vserver)
{
    if (access(VIRTNET_DIR, R_OK | X_OK)) {
        log_msg(LOG_ERR, 0, "Cannot access %s", VIRTNET_DIR);
        return -1;
    }

    *state = NULL;

    return 0;
}


F_close(vserver)
{
    *state = NULL;
}


F_vm_close(vserver)
{
    return;
}


F_vm_exists(vserver)
{
    char *path;
    vcCfgStyle cfgstyle;
    bool running = false;
    int exists;

    if ((path = vc_getVserverByCtx(id, &cfgstyle, NULL)) == NULL)
        return 0;

#if HAVE_CTX_TYPE
    exists = vc_getVserverCtx(path, cfgstyle, false, &running, vcCTX_XID)
             != VC_NOCTX;
#else
    exists = vc_getVserverCtx(path, cfgstyle, false, &running) != VC_NOCTX;
#endif
    exists &= running;

    free(path);

    return exists;
}


F_vm_name2id(vserver)
{
    int found = 0;
    DIR *dir;
    struct dirent *dent;
    vmid_t vmid;
    vcCfgStyle cfgstyle;
    char *path;
    char *vmname;

    if ((dir = opendir(VIRTNET_DIR)) == NULL) {
        log_msg(LOG_ERR, 0, "Cannot open %s directory", VIRTNET_DIR);
        return -1;
    }

    while (!found && (dent = readdir(dir)) != NULL) {
        if (!isdigit(*dent->d_name) || (vmid = atol(dent->d_name)) == 0)
            continue;

        if ((path = vc_getVserverByCtx(vmid, &cfgstyle, NULL)) != NULL) {
            if ((vmname = vc_getVserverName(path, cfgstyle)) != NULL) {
                if (strcmp(vmname, name) == 0) {
                    found = 1;
                    *id = vmid;
                }
                free(vmname);
            }
            free(path);
        }
    }

    closedir(dir);

    return !found;
}


F_vm_suspend(vserver)
{
    log_msg(LOG_INFO, 0,
            "Sending STOP signal to all processes in context %lu", id);

    if (vc_ctx_kill(id, 0, SIGSTOP))
        return -1;
    else
        return 0;
}


F_vm_resume(vserver)
{
    log_msg(LOG_INFO, 0,
            "Sending CONT signal to all processes in context %lu", oldid);

    if (vc_ctx_kill(oldid, 0, SIGCONT))
        return -1;
    else {
        *id = oldid;
        return 0;
    }
}


F_select(vserver)
{
    vmm_name = "vserver";
    vmm_init = vserver_init;
    vmm_close = vserver_close;
    vmm_vm_close = vserver_vm_close;
    vmm_vm_exists = vserver_vm_exists;
    vmm_vm_name2id = vserver_vm_name2id;
    vmm_vm_suspend = vserver_vm_suspend;
    vmm_vm_resume = vserver_vm_resume;
}

