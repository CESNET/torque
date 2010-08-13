/* $Id: vmm.h,v 1.9 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * VMM specific functions for magrathea master daemon.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.9 $ $Date: 2008/05/29 14:51:49 $
 */

#ifndef VMM_H
#define VMM_H

#include "../config.h"

#include <stdint.h>

#include "../net.h"


/** Data type of VM identifier. */
typedef uint32_t vmid_t;

/** Name of the current VMM. */
extern const char *vmm_name;


/** Automatically detect VMM implementation to be used.
 * If automatic detection was disabled in compile-time, this function just
 * selects the VMM which has been compiled in.
 *
 * @return
 *      zero on success, nonzero when no VMM can be detected.
 */
extern int vmm_detect(void);


/** Detect whether the master is running inside the particular VMM.
 *
 * @return
 *      nonzero iff the VMM is running.
 */
#define F_detect(prefix) \
    int (prefix ## _detect)(void)


/** Setup pointers to VMM-specific functions.
 *
 * @return
 *      nothing.
 */
#define F_select(prefix) \
    void (prefix ## _select)(void)


/** Initialize VMM state.
 *
 * @param state
 *      place where pointer to a VMM state should be stored.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
#define F_init(prefix) \
    int (prefix ## _init)(void **state)

extern F_init(*vmm);


/** Clean up VMM state.
 *
 * @param state
 *      VMM state.
 *
 * @return
 *      nothing.
 */
#define F_close(prefix) \
    void (prefix ## _close)(void **state)

extern F_close(*vmm);


/** Close any connection related to dealing with a VM.
 *
 * @param state
 *      VMM state.
 *
 * @return
 *      nothing.
 */
#define F_vm_close(prefix) \
    void (prefix ## _vm_close)(void *state)

extern F_vm_close(*vmm);


/** Check if a given VM still exists.
 *
 * @param[in] id
 *      VM identifier.
 *
 * @param[in] fqdn
 *      fully qualified domain name of the VM.
 *
 * @param state
 *      VMM state.
 *
 * @return
 *      zero if the domain does not exist, nonzero otherwise.
 */
#define F_vm_exists(prefix) \
    int (prefix ## _vm_exists)(vmid_t id, char *fqdn, void *state)

extern F_vm_exists(*vmm);


/** Get ID of a connected VM from its static ID.
 *
 * @param[in] name
 *      domain's static ID or name.
 *
 * @param state
 *      VMM state.
 *
 * @param id
 *      pointer to a place where VM identifier should be stored. If this is
 *      NULL, return status can be used to check if the domain exists or not.
 *      The stored value is undefined on error.
 *
 * @return
 *      zero when a VM is found and its ID is stored in id, nonzero otherwise.
 */
#define F_vm_name2id(prefix) \
    int (prefix ## _vm_name2id)(const char *name, void *state, vmid_t *id)

extern F_vm_name2id(*vmm);


/** Susped a VM so that it does not consume any CPU or memory resources.
 *
 * @param[in] fqdn
 *      fully qualified domain name of the VM.
 *
 * @param[in] id
 *      VM identifier.
 *
 * @param state
 *      VMM state.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
#define F_vm_suspend(prefix) \
    int (prefix ## _vm_suspend)(char *fqdn, vmid_t id, void *state)

extern F_vm_suspend(*vmm);


/** Resume a VM previously suspended by vmm_vm_suspend().
 * Note that depending on VMM implementation the VM identifier may differ
 * from the identifier the VM before it was suspended.
 *
 * @param[in] fqdn
 *      fully qualified domain name of the VM.
 *
 * @param[in] addr
 *      domain's IP address.
 *
 * @param[out] id
 *      pointer to a place where VM identifier should be stored.
 *      The value is undefined on error.
 *
 * @param[in] oldid
 *      identifier of the VM before it was suspended.
 *
 * @param state
 *      VMM state.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
#define F_vm_resume(prefix) \
    int (prefix ## _vm_resume)(char *fqdn, \
                               char *name, \
                               net_addr_t addr, \
                               vmid_t *id, \
                               vmid_t oldid, \
                               void *state)

extern F_vm_resume(*vmm);

#endif


