/* $Id: vserver-CTX_TYPE.c,v 1.2 2008/05/21 15:56:16 xdenemar Exp $
 *
 * configure:INFO="checking whether VServer supports vcCtxType"
 */

#include <stdlib.h>

typedef unsigned long xid_t;
typedef unsigned long nid_t;
typedef unsigned long tag_t;
#include <vserver.h>

int main(void)
{
    vcCfgStyle cfgstyle;

    (void) vc_getVserverCtx(NULL, cfgstyle, false, NULL, vcCTX_XID);

    return 0;
}

