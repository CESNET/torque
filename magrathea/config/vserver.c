/* $Id: vserver.c,v 1.3 2008/05/29 14:51:49 xdenemar Exp $
 *
 * configure:INFO="checking for VServer"
 * configure:LDFLAGS="-lvserver"
 * configure:LDFLAGS_STATIC="-Wl,-Bstatic -lvserver -Wl,-Bdynamic"
 */

#include <stdlib.h>

typedef unsigned long xid_t;
typedef unsigned long nid_t;
typedef unsigned long tag_t;
#include <vserver.h>

int main(void)
{
    (void) vc_getVserverByCtx(0, NULL, NULL);

    return 0;
}

