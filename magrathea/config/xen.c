/* $Id: xen.c,v 1.3 2008/05/29 14:51:49 xdenemar Exp $
 *
 * configure:INFO="checking for XEN"
 * configure:LDFLAGS="-lxenctrl -lxenstore -pthread"
 * configure:LDFLAGS_STATIC="-Wl,-Bstatic -lxenctrl -lxenstore -Wl,-Bdynamic -pthread"
 */

#include <stdlib.h>
#include <xs.h>
#include <xenctrl.h>

int main(void)
{
    (void) xc_interface_open();
    (void) xs_daemon_open();

    return 0;
}

