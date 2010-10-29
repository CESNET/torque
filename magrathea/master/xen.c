/* $Id: xen.c,v 1.15 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * Xen-specific functions for magrathea master daemon.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.15 $ $Date: 2008/05/29 14:51:49 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "../utils.h"
#include "../net.h"
#include "vmm.h"

#include <xs.h>
#include <xenctrl.h>


/** Maximum length of a path. */
#define MAX_PATH_LEN        256

/** Path to a list of domains in XenStore. */
#define DOMAINS_PATH        "/local/domain"

/** Format string for suspend/resume checkpoint file. */
#define CHECKPOINT_FILE     RUN_DIR "/%s"

/** How long we can try to connect to the domain after resuming it.
 * Zero means we will wait forever. */
#define CONNECT_TIMEOUT     0


/** Context used for communication with hypervisor and XenStore. */
struct context {
    /** Communication context for hypervisor. */
    int xc;
    /** Connection to XenStore. */
    struct xs_handle *xs;
    /** XenStore transaction. */
    xs_transaction_t trans;
};

/** Initialization value for struct context. */
#define CTX_INIT    { -1, NULL, 0 }


/** Ensure communication context with hypervisor is created.
 * The daemon terminates itself if hypervisor cannot be contacted.
 *
 * @param ctx
 *      pointer to context structure.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
static int ctx_hypervisor(struct context *ctx);


/** Ensure connection to XenStore daemon is initialized.
 *
 * @param ctx
 *      pointer to context structure.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
static int ctx_xenstore(struct context *ctx);


/** Ensure XenStore transaction is started.
 *
 * @param ctx
 *      pointer to context structure.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
static int ctx_transaction_start(struct context *ctx);


/** Commit or abort XenStore transaction (if any).
 *
 * @param ctx
 *      pointer to context structure.
 *
 * @param[in] abort
 *      abort (nonzero) or commit (zero) the transaction.
 *
 * @return
 *      nothing.
 */
static void ctx_transaction_end(struct context *ctx, int abort);


/** Better error handler than xc_default_error_handler().
 *
 * @return
 *      nothing.
 */
static void xen_error_handler(const xc_error const *err);


F_detect(xen)
{
    xc_error_handler handler;
    int xc;

    handler = xc_set_error_handler(NULL);
    xc = xc_interface_open();
    xc_set_error_handler(handler);

    if (xc == -1)
        return 0;

    xc_interface_close(xc);

    return 1;
}


F_init(xen)
{
    struct context ctx_init = CTX_INIT;
    struct context *ctx = NULL;

    if ((ctx = malloc(sizeof(struct context))) == NULL)
        return -1;

    xc_set_error_handler(xen_error_handler);

    *ctx = ctx_init;
    *state = (void *) ctx;

    return ctx_hypervisor(ctx);
}


F_close(xen)
{
    struct context *ctx = (struct context *) *state;

    ctx_transaction_end(ctx, 1);

    if (ctx->xs != NULL)
        xs_daemon_close(ctx->xs);

    xc_interface_close(ctx->xc);

    free(ctx);
    *state = NULL;
}


F_vm_close(xen)
{
    struct context *ctx = (struct context *) state;

    ctx_transaction_end(ctx, 1);
}


F_vm_exists(xen)
{
    struct context *ctx = (struct context *) state;
    xc_dominfo_t info;
    int exists = 0;

    if (fqdn != NULL) {
        char *file;
        int len;

        len = strlen(CHECKPOINT_FILE) + strlen(fqdn);
        if ((file = (char *) malloc(len)) != NULL) {
            sprintf(file, CHECKPOINT_FILE, fqdn);
            if (access(file, R_OK) == 0)
                exists = 1;
        }
    }

    return exists ||
           (xc_domain_getinfo(ctx->xc, id, 1, &info) == 1
            && info.domid == id);
}


F_vm_name2id(xen)
{
    struct context *ctx = (struct context *) state;
    int error = -1;
    char path[MAX_PATH_LEN];
    char **list;
    unsigned int list_size;
    void *value;
    unsigned int len;
    int i;

    if (ctx_transaction_start(ctx))
        return -1;

    list = xs_directory(ctx->xs, ctx->trans, DOMAINS_PATH, &list_size);
    if (list == NULL) {
        log_msg(LOG_ERR, 0, "Cannot fetch list of domains");
        return -1;
    }

    for (i = 0; i < list_size; i++) {
        snprintf(path, MAX_PATH_LEN, "%s/%s/name", DOMAINS_PATH, list[i]);
        path[MAX_PATH_LEN - 1] = '\0';

        if ((value = xs_read(ctx->xs, ctx->trans, path, &len)) != NULL) {
            if ((strcmp(value, name) == 0)) {
                if (id != NULL)
                    *id = atol(list[i]);
                free(value);
                error = 0;
                break;
            }
            free(value);
        }
    }
    free(list);

    return error;
}


F_vm_suspend(xen)
{
    char *cmd;
    int len;
    int err;

    if (fqdn == NULL)
        return -1;

    /* "xm save " + ID + "\""CHECKPOINT_FILE(fqdn)"\"" */
    len = 8 + sizeof(id) * 8 + 1
          + 1 + strlen(CHECKPOINT_FILE) + strlen(fqdn) + 1;

    if ((cmd = (char *) malloc(len + 1)) == NULL) {
        log_msg(LOG_ERR, errno, "Cannot suspend domain %s", fqdn);
        return -1;
    }

    sprintf(cmd, "xm save %lu \"" CHECKPOINT_FILE "\"",
            (unsigned long) id, fqdn);

    log_msg(LOG_INFO, 0, "Running command: %s", cmd);

    err = system_command(cmd);

    if (err)
        log_msg(LOG_ERR, 0, "Command failed: %s", cmd);

    free(cmd);

    return err;
}


F_vm_resume(xen)
{
    char *cmd;
    char *file;
    int len;
    int error;
    int sock;
    int port;

    if (fqdn == NULL)
        return -1;

    len = strlen(CHECKPOINT_FILE) + strlen(fqdn);
    if ((file = (char *) malloc(len + 1)) == NULL) {
        log_msg(LOG_ERR, errno, "Cannot resume domain %s", fqdn);
        return -1;
    }
    sprintf(file, CHECKPOINT_FILE, fqdn);

    /* "xm restore " + "\"" file "\"" */
    len = 11 + 1 + strlen(file) + 1;

    if ((cmd = (char *) malloc(len + 1)) == NULL) {
        free(file);
        log_msg(LOG_ERR, errno, "Cannot resume domain %s", fqdn);
        return -1;
    }

    sprintf(cmd, "xm restore \"%s\"", file);
    log_msg(LOG_INFO, 0, "Running command: %s", cmd);

    error = system_command(cmd);

    if (error)
        log_msg(LOG_ERR, 0, "Command failed: %s", cmd);
    else
        error = xen_vm_name2id(name, state, id);

    free(cmd);

    log_msg(LOG_INFO, 0,
            "Waiting for resumed domain to become reacheable over network");
    port = 0;
    if ((sock = net_socket(net_inaddr_any, &port, 0, 0)) >= 0) {
        net_sockaddr_t slave;
#if CONNECT_TIMEOUT > 0
        time_t start = time(NULL);
# define TIMEOUT    (time(NULL) - start >= CONNECT_TIMEOUT)
#else
# define TIMEOUT    (0)
#endif
        int err = 0;

        net_sockaddr_set_ip(&slave, addr);
        net_sockaddr_set_port(&slave, 9); /* discard service */

        while (!TIMEOUT
               && connect(sock, (struct sockaddr *) &slave, sizeof(slave))) {
            err = errno;
            if (err != EAGAIN
                && err != EINTR
                && err != ENETUNREACH
                && err != EHOSTUNREACH)
                break;
            usleep(100000);
        }

        if (err == 0 || err == ECONNREFUSED)
            log_msg(LOG_INFO, 0, "Domain reacheable");
#if CONNECT_TIMEOUT > 0
        else if (TIMEOUT) {
            log_msg(LOG_WARNING, 0,
                    "Domain still unreachable after %d seconds; giving up",
                    CONNECT_TIMEOUT);
        }
#endif
        else
            log_msg(LOG_WARNING, err, "Connecting to domain failed");

        close(sock);
    }

    if (!error)
        unlink(file);
    free(file);

    return error;
}


static int ctx_hypervisor(struct context *ctx)
{
    if (ctx->xc == -1 && (ctx->xc = xc_interface_open()) == -1) {
        log_msg(LOG_ERR, 0, "Cannot contact Xen hypervisor");
        return -1;
    }
    else
        return 0;
}


static int ctx_xenstore(struct context *ctx)
{
    ctx_hypervisor(ctx);

    if (ctx->xs == NULL && (ctx->xs = xs_daemon_open()) == NULL) {
        log_msg(LOG_ERR, 0, "Cannot contact XenStore daemon");
        return -1;
    }

    return 0;
}


static int ctx_transaction_start(struct context *ctx)
{
    if (ctx_xenstore(ctx))
        return -1;

    if (!ctx->trans && !(ctx->trans = xs_transaction_start(ctx->xs))) {
        log_msg(LOG_ERR, 0, "Cannot start XenStore transaction");
        return -1;
    }

    return 0;
}


static void ctx_transaction_end(struct context *ctx, int abort)
{
    struct context ctx_init = CTX_INIT;

    if (ctx->trans)
        xs_transaction_end(ctx->xs, ctx->trans, abort);
    ctx->trans = ctx_init.trans;
}


static void xen_error_handler(const xc_error const *err)
{
    const char *desc = xc_error_code_to_desc(err->code);

    log_msg(LOG_ERR, 0, "Xen error: %s: %s", desc, err->message);
}


F_select(xen)
{
    vmm_name = "xen";
    vmm_init = xen_init;
    vmm_close = xen_close;
    vmm_vm_close = xen_vm_close;
    vmm_vm_exists = xen_vm_exists;
    vmm_vm_name2id = xen_vm_name2id;
    vmm_vm_suspend = xen_vm_suspend;
    vmm_vm_resume = xen_vm_resume;
}

