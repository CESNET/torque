/* $Id: magrathea-master.c,v 1.69 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * Magrathea master daemon for batch system to VMM interaction.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.69 $ $Date: 2009/06/04 11:12:30 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <assert.h>

#include "../utils.h"
#include "../net.h"
#include "../cfg.h"
#include "paths.h"
#include "vmm.h"
#include "cpus.h"
#include "domains.h"
#include "storage.h"
#include "reporter.h"
#include "status.h"
#include "versions-magrathea-master.h"


/** Calback request. */
enum callback_bitmask {
    /* No VMM callback is required to be called. */
    CB_NONE = 0x0,
    /* Run a callback to change numbers of CPUs assigned to each domain. */
    CB_CPU = 0x1,
    /* Run a callback to switch free, running and preempted domains. */
    CB_SWITCH = 0x2
};


/** Command's origin, that is who issued the command. */
enum command_origin {
    CO_ADMIN,
    CO_DEFROST,
    CO_SLAVE
};


/** Slave command. */
enum slave_command {
    SLAVE_SUSPEND_FREE,
    SLAVE_SUSPEND_PREEMPTED,
    SLAVE_WAKEUP,
    SLAVE_REBOOT
};

/** Textual representation of a slave command. */
char *slave_command_str[] = {
    "suspend",
    "suspend",
    "wakeup",
    "reboot"
};


/** Command handler. */
struct command {
    /** Command name. */
    const char *name;
    /** Nonzero iff the command is enabled even when booting domains. */
    int booting;
    /** Nonzero iff the command requires domain's FQDN to be specified. */
    int fqdn;
    /** Handler function.
     * @param vmm_state
     *      VMM state.
     * @param conn
     *      pointer to the connection to write result into.
     * @param dom
     *      domain or NULL if command was received without FQDN argument.
     * @param args
     *      command arguments, with <tt>args->argv[0]</tt> being the command
     *      name and <tt>args->argv[1]</tt> being domain's FQDN.
     * @return
     *      nothing.
     */
    void (*handler)(enum command_origin origin,
                    void *vmm_state,
                    struct connection *conn,
                    struct domain *dom,
                    struct args *args);
};


/** Memory configuration. */
struct memory {
    unsigned long free;
    unsigned long preempted;
    unsigned long running;
};


/** Register a domain.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "register"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: port number
 */
static void cmd_register(enum command_origin origin,
                         void *vmm_state,
                         struct connection *conn,
                         struct domain *dom,
                         struct args *args);


/** Unregister a domain.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] dom
 *      domain.
 *
 * @param[in] conn
 *      connection to send response to.
 *
 * @return
 *      nothing.
 */
static void cmd_unregister(void *vmm_state,
                           struct domain *dom,
                           struct connection *conn);


/** Check domain status.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "status"
 */
static void cmd_status(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args);


/** Get number of seconds a particular job was preempted.
 *
 * @param origin
 * 	source of the command (CO_ADMIN, CO_SLAVE)
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] dom
 *      domain.
 *
 * @param[in] jobid
 *      job ID.
 *
 * @param conn
 *      connection to send response to.
 *
 * @param[in] args
 * 	arguments (fqdn, jobid).
 *
 * @return
 *      nothing.
 */
static void cmd_preempted(enum command_origin origin,
			  void *vmm_state,
                          struct connection *conn,
                          struct domain *dom,
			  struct args *args);


/** Job start/stop processing.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] start
 *      request job start (nonzero) or inform about job end (zero).
 *
 * @param[in] dom
 *      domain.
 *
 * @param[in] jobid
 *      job ID. If start is equal to zero and jobid == NULL, all jobs will
 *      be stopped.
 *
 * @param[in] cpus
 *      number of CPUs the job uses.
 *
 * @param[in] preemptible
 *      specifies whether the job can be preempted.
 *
 * @param[in] conn
 *      connection to send response to.
 *
 * @return
 *      nothing.
 */
static void cmd_job(void *vmm_state,
                    int start,
                    struct domain *dom,
                    const char *jobid,
                    int cpus,
                    int preemptible,
                    struct connection *conn,
		    int cluster);


/** Create a new domain.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "create"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: name (static identifier of the domain)
 * - <tt>argv[3]</tt>: priority ("priority" | "normal")
 * - <tt>argv[4]</tt>: IP address of the domain
 */
static void cmd_create(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args);


/** Mark a domain as booting and wait for it to come up.
 * @sa command::handler.
 *
 * Either
 * - <tt>argv[0]</tt>: "boot"
 * - <tt>argv[1]</tt>: FQDN
 *
 * or
 * - <tt>argv[0]</tt>: "force-boot"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_boot(enum command_origin origin,
                     void *vmm_state,
                     struct connection *conn,
                     struct domain *dom,
                     struct args *args);


/** Mark a domain as booting and wait for it to get down and come up again.
 * @sa command::handler.
 *
 * Either
 * - <tt>argv[0]</tt>: "reboot"
 * - <tt>argv[1]</tt>: FQDN
 *
 * or
 * - <tt>argv[0]</tt>: "force-reboot"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_reboot(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args);


/** Mark a booting domain as down.
 * @sa command::handler.
 *
 * Either
 * - <tt>argv[0]</tt>: "down"
 * - <tt>argv[1]</tt>: FQDN
 *
 * or
 * - <tt>argv[0]</tt>: "force-down"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_down(enum command_origin origin,
                     void *vmm_state,
                     struct connection *conn,
                     struct domain *dom,
                     struct args *args);


/** Remove a domain which is down.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "boot"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_remove(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args);


/** Change priority of a domain.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "priority"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: priority ("priority" | "normal")
 */
static void cmd_priority(enum command_origin origin,
                         void *vmm_state,
                         struct connection *conn,
                         struct domain *dom,
                         struct args *args);


/** Freeze (that is, suspend) a domain.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "freeze"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_freeze(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args);


/** Defrost a frozen domain.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "defrost"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_defrost(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args);


/** Start a job in a domain.
 * @sa command::handler.
 *
 * Either
 * - <tt>argv[0]</tt>: "startjob"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: JobID
 * - <tt>argv[3]</tt>: CPUs
 * or
 * - <tt>argv[0]</tt>: "startjob-preemptible"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: JobID
 * - <tt>argv[3]</tt>: CPUs
 * or
 * - <tt>argv[0]</tt>: "startcluster"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: JobID
 * - <tt>argv[3]</tt>: CPUs
 */
static void cmd_startjob(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args);


/** Stop a job (or all jobs) in a domain.
 * @sa command::handler.
 *
 * Either
 * - <tt>argv[0]</tt>: "stopjob"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: JobID
 * or
 * - <tt>argv[0]</tt>: "stopcluster"
 * - <tt>argv[1]</tt>: FQDN
 * - <tt>argv[2]</tt>: JobID
 * or
 * - <tt>argv[0]</tt>: "stopalljobs"
 * - <tt>argv[1]</tt>: FQDN
 */
static void cmd_stopjob(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args);


/** Ping.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "ping-master"
 */
static void cmd_master_ping(enum command_origin origin,
                            void *vmm_state,
                            struct connection *conn,
                            struct domain *dom,
                            struct args *args);


/** Stop the daemon.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "stop-master"
 */
static void cmd_master_stop(enum command_origin origin,
                            void *vmm_state,
                            struct connection *conn,
                            struct domain *dom,
                            struct args *args);


/** Manipulate with domain's cluster.
 * @sa command::handler.
 *
 * - <tt>argv[0]</tt>: "cluster"
 * - <tt>argv[1]</tt>: FQDN (optional)
 * - <tt>argv[2]</tt>: "set"/"remove" (optional)
 * - <tt>argv[3]</tt>: cluster (only when argv[2] == "set")
 */
static void cmd_cluster(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args);


/** Check incoming connection and detect source virtual machine.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] addr
 *      source address of the connection.
 *
 * @param[out] dom
 *      domain which initiated the connection if it can be found.
 *
 * @return
 *      nonzero when the connection is accepted, zero otherwise.
 */
static int check_connection(void *vmm_state,
                            const net_sockaddr_t *addr,
                            struct domain **dom);


/** Process request(s) coming from a single connection.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] dom
 *      connected domain (if it is known).
 *
 * @param[in] conn
 *      the connection.
 *
 * @return
 *      nothing.
 */
static void handle_connection(void *vmm_state,
                              struct domain *dom,
                              struct connection *conn);


/** Check and handle connection for defrost requests.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] addr
 *      source address of the connection.
 *
 * @param[in] conn
 *      the connection.
 *
 * @return
 *      nothing.
 */
static void defrost_connection(void *vmm_state,
                               const net_sockaddr_t *addr,
                               struct connection *conn);


/** Handle administrative connection.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] conn
 *      the connection.
 *
 * @return
 *      nothing.
 */
static void admin_connection(void *vmm_state, struct connection *conn);


/** Create socket for defrost requests and register it as a defrost point.
 *
 * @param[in] addr
 *      IP address to bind the socket to.
 *
 * @param[in] port
 *      port number to listen on.
 *
 * @return
 *      socket filedescriptor or -1 on error.
 */
static int defrost_socket(const char *addr, int port);


/** Convert status code returned by domains_*() functions to callback type.
 *
 * @param[in] sc
 *      status code.
 *
 * @param[inout] cb
 *      callback type.
 *
 * @return
 *      nothing.
 */
static void status_code_2_callback_type(int sc, int *cb);


/** Execute external callback script.
 *
 * @param vmm_state
 *      VMM state.
 *
 * @param[in] type
 *      callback type bitmask.
 *
 * @return
 *      nothing.
 */
static void callback(void *vmm_state, int type);


/** Send command to slave daemon.
 *
 * @param[in] dom
 *      a virtual machine to send the message to.
 *
 * @param[in] command
 *      a command to be sent.
 *
 * @return
 *      nothing.
 */
static void slave_command(const struct domain *dom, enum slave_command cmd);


/** Administrative commands and their handlers. */
static struct command admin_commands[] = {
    /* name    booting  fqdn    handler         */
    { "create",      1,    1,   cmd_create      },
    { "boot",        1,    1,   cmd_boot        },
    { "force-boot",              1,    1,   cmd_boot         },
    { "reboot",      0,    1,   cmd_reboot      },
    { "force-reboot",            0,    1,   cmd_reboot       },
    { "down",                    0,    1,   cmd_down         },
    { "force-down",              0,    1,   cmd_down         },
    { "remove",      1,    1,   cmd_remove      },
    { "register",    1,    1,   cmd_register    },
    { "priority",    0,    1,   cmd_priority    },
    { "freeze",      0,    1,   cmd_freeze      },
    { "defrost",     0,    1,   cmd_defrost     },
    { "status",      1,    0,   cmd_status      },
    { "startjob",                0,    1,   cmd_startjob     },
    { "startjob-preemptible",    0,    1,   cmd_startjob     },
    { "startcluster",            0,    1,   cmd_startjob     },
    { "stopjob",     0,    1,   cmd_stopjob     },
    { "stopalljobs", 0,    1,   cmd_stopjob     },
    { "stopcluster",             0,    1,   cmd_stopjob      },
    { "preempted",               0,    1,   cmd_preempted    },
    { "ping-master", 1,    0,   cmd_master_ping },
    { "stop-master", 1,    0,   cmd_master_stop },
    { "cluster",     1,    0,   cmd_cluster     },

    /* end of command list */
    { NULL, 0, 0, NULL }
};

/** Number of items in admin_commands array. */
#define admin_commands_size \
    ((sizeof(admin_commands) / sizeof(struct command)) - 1)


/** Default memory configuration. */
static struct memory memory = { 0, 0, 0 };


/** Usage description. */
#define USAGE   \
    "Usage:   %s [-hvsd46] [-c config_file] [-p pid_file]\n" \
    "Options:\n" \
    "   -h  print usage\n" \
    "   -v  print version information\n" \
    "   -s  just sync magrathea with VMM state\n" \
    "   -d  print debug information (printed to stdout instead of syslog)\n" \
    "       the process will remain running in foreground\n" \
    "   -4  force IPv4 (overrides ipv6 option in configuration file)\n" \
    "   -6  prefer IPv6 (overrides ipv6 option in configuration file)\n" \
    "   -c config_file\n" \
    "       configuration file (defaults to " CONFIG_FILE ")\n" \
    "   -p pid_file\n" \
    "       PID file (defaults to " PID_FILE ")\n"

/** Print usage description and exit.
 *
 * @param[in] error
 *      nonzero when usage description is prented as a result of incorrect
 *      parameters passed to the daemon.
 *
 * @param[in] argv0
 *      name of the daemon's binary (i.e., argv[0] of the main() function).
 *
 * @return
 *      nothing, actually, the function never returns.
 */
static void usage(int error, char *argv0)
{/*{{{*/
    if (error)
        log_msg(LOG_ERR, 0, USAGE, argv0);
    else
        printf(USAGE "\n", argv0);

    exit(error);
}/*}}}*/


/** Daemon's main function.
 *
 * @param[in] argc
 *      number of arguments given.
 *
 * @param[in] argv
 *      array of arguments.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
int main(int argc, char **argv)
{
    int opt;
    int sync = 0;
    char *config_file = CONFIG_FILE;
    struct args *args;
    struct cfg_file *cfg;
    int cpus;
    const char *bind_addr;
    net_addr_t bind_addr_n;
    int port;
    const char *defrost_ip;
    int defrost_port;
    int sock_defrost;
    const char *admin_path;
    int sock_admin;
    void *vmm_state = NULL;
    int sock;
    fd_set fds;
    int ret;
    int err;
    int sc;
    int detach = 1;
    char *pidfile = PID_FILE;
    enum net_protocol protocol = NPROTO_AUTO;
    char fqdn[HOST_NAME_MAX + 1] = { 0 };

    log_init("magrathea-master");

    while ((opt = getopt(argc, argv, "hvsd46c:p:")) != -1) {
        switch (opt) {
        case 's':
            sync = 1;
            log_msg(LOG_NOTICE, 0, "Just syncing magrathea and VMM state");
            break;
        case 'd':
            log_stderr();
            log_debug(1);
            detach = 0;
            break;
        case 'c':
            config_file = optarg;
            break;
        case 'p':
            pidfile = optarg;
            break;
        case 'h':
            usage(0, argv[0]);
            break;
        case 'v':
            printf("magrathea-master daemon\n%s", VERSIONS);
            return 0;
        case '4':
            protocol = NPROTO_IPV4;
            break;
        case '6':
#if IPv6
            protocol = NPROTO_IPV6;
#else
            log_msg(LOG_ERR, 0, "%s was compiled without IPv6 support",
                    argv[0]);
#endif
            break;
        default:
            usage(2, argv[0]);
        }
    }


    log_msg(LOG_NOTICE, 0, "Starting magrathea version " VERSION_TAG);

    if (vmm_detect()) {
        log_msg(LOG_ERR, 0,
                "None of the following VMMs was detected: %s", VMM);
        return 1;
    }

    if (gethostname(fqdn, sizeof(fqdn))) {
        log_msg(LOG_ERR, errno, "Cannot get host's FQDN");
        return 1;
    }
    domains_fqdn(fqdn);

    if (detach && daemonize(pidfile, &terminate_handler)) {
        log_msg(LOG_ERR, errno, "Daemonization failed");
        return 1;
    }

    /* ignore broken pipe signal which is sent when we try to write to
     * a closed connection */
    signal(SIGPIPE, SIG_IGN);

    if ((cfg = cfg_parse(config_file)) == NULL)
        usage(2, argv[0]);

    if (strcmp(cfg_get_string(cfg, CFG_FIRST, "debug", "no"), "yes") == 0)
        log_debug(1);

    if (protocol == NPROTO_AUTO) {
        const char *ipv6 = cfg_get_string(cfg, CFG_FIRST, "ipv6", "auto");

        if (strcmp(ipv6, "yes") == 0) {
#if IPv6
            protocol = NPROTO_IPV6;
#else
            log_msg(LOG_ERR, 0, "%s was compiled without IPv6 support",
                    argv[0]);
#endif
        }
        else if (strcmp(ipv6, "auto") != 0)
            protocol = NPROTO_IPV4;
    }
    net_proto(protocol);

    if ((cpus = (int) cfg_get_long(cfg, CFG_FIRST, "cpus", 0)) < 0)
        cpus = 0;
    cpus_init(cpus);

#if XEN
    memory.free = cfg_get_long(cfg, CFG_FIRST, "xen-mem-free", 160);
    memory.running = cfg_get_long(cfg, CFG_FIRST, "xen-mem-running", 0);
    memory.preempted = cfg_get_long(cfg, CFG_FIRST, "xen-mem-preempted", 200);

    if (memory.free < 0)
        memory.free = 0;
    if (memory.running < 0)
        memory.running = 0;
    if (memory.preempted < 0)
        memory.preempted = 0;

    memory.free *= 1000;
    memory.running *= 1000;
    memory.preempted *= 1000;
#endif

    cfg_reset(cfg);
    while ((args = cfg_get_list(cfg, CFG_NEXT, "cache")) != NULL) {
        if (args->argc > 0) {
            int port = PBS_CACHE_PORT;
            if (args->argc > 1)
                port = atoi(args->argv[1]);

            if (reporter_add(args->argv[0], port))
                return 1;
        }
        cfg_free_list(args);
    }
    if (reporter_start())
        return 1;

    bind_addr = cfg_get_string(cfg, CFG_FIRST,
                               "listen-address", net_inaddr_any_str);
    port = (int) cfg_get_long(cfg, CFG_FIRST, "listen-port", DEFAULT_PORT);
    if (!net_addr_parse(bind_addr, &bind_addr_n))
        usage(2, argv[0]);
    net_local_address(bind_addr_n);

    if (net_addr_equal(bind_addr_n, net_inaddr_any))
        sc = storage_init(NULL, port);
    else
        sc = storage_init(bind_addr, port);

    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot initialize permanent storage: %s",
                status_decode(sc));
        return 1;
    }

    defrost_ip = cfg_get_string(cfg, CFG_FIRST, "defrost-address", bind_addr);
    defrost_port = (int) cfg_get_long(cfg, CFG_FIRST, "defrost-port", 0);
    if ((sock_defrost = defrost_socket(defrost_ip, defrost_port)) < 0) {
        log_msg(LOG_ERR, 0, "Defrost socket initialization failed");
        return 1;
    }

    admin_path = cfg_get_string(cfg, CFG_FIRST, "admin-socket", ADMIN_SOCKET);
    if ((sock_admin = net_socket_un(admin_path)) < 0) {
        log_msg(LOG_ERR, 0, "Cannot create administrative socket");
        close(sock_defrost);
        return 1;
    }
    else {
        log_msg(LOG_INFO, 0, "Waiting for administration on unix://%s",
                admin_path);
    }

    if (vmm_init(&vmm_state) != 0) {
        log_msg(LOG_ERR, 0, "Cannot initialize VMM state");
        close(sock_defrost);
        close(sock_admin);
        return 1;
    }

    if (STATUS_IS_ERROR(sc = domains_state_load(vmm_state)))
        log_msg(LOG_WARNING, 0, "Error loading state: %s", status_decode(sc));

    if (cpus_enabled() && !STATUS_IS_OK(sc = domains_reset_counters())) {
        log_msg(LOG_ERR, 0, "Cannot initialize CPU counting: %s",
                status_decode(sc));
        return 1;
    }

    domains_optimize();
    domains_state_update();
    domains_state_save();
    callback(vmm_state, CB_CPU | CB_SWITCH);

    if (sync) {
        close(sock_defrost);
        close(sock_admin);
        vmm_close(&vmm_state);
        return 0;
    }

    if ((sock = net_socket(bind_addr_n, &port, 0, 1)) < 0
        || listen(sock, 5)) {
        close(sock_defrost);
        close(sock_admin);
        vmm_close(&vmm_state);
        return 1;
    }

    log_msg(LOG_NOTICE, 0, "Listening on %s port %d", bind_addr, port);

    while (1) {
        int max;
        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
        int cb = CB_NONE;

        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        FD_SET(sock_defrost, &fds);
        FD_SET(sock_admin, &fds);

        max = sock;
        if (sock_defrost > max)
            max = sock_defrost;
        if (sock_admin > max)
            max = sock_admin;

        ret = select(max + 1, &fds, NULL, NULL, &tv);
        err = errno;

        domains_check(vmm_state);

        if (ret == -1) {
            if (err != EINTR)
                break;
        }
        else if (ret > 0) {
            struct connection conn;
            net_sockaddr_t addr;
            socklen_t len = sizeof(addr);

            if (FD_ISSET(sock_admin, &fds)) {
                if (!connection_init(&conn, accept(sock_admin, NULL, 0))) {
                    log_msg(LOG_INFO, 0, "Accepted administrative connection");
                    admin_connection(vmm_state, &conn);
                    log_msg(LOG_INFO, 0, "Administrative connection closed");
                    connection_close(&conn);
                    vmm_vm_close(vmm_state);
                }
            }

            if (FD_ISSET(sock, &fds)) {
                if (!connection_init(&conn,
                        accept(sock, (struct sockaddr *) &addr, &len))) {
                    struct domain *dom;

                    if (check_connection(vmm_state, &addr, &dom)
                        && !connection_gss(&conn, CONN_SERVER, NULL, NULL)) {
                        handle_connection(vmm_state, dom, &conn);
                    }

                    connection_close(&conn);
                    vmm_vm_close(vmm_state);
                }
            }

            if (FD_ISSET(sock_defrost, &fds)) {
                if (!connection_init(&conn, 
                        accept(sock_defrost, (struct sockaddr *) &addr, &len))) {
                    defrost_connection(vmm_state, &addr, &conn);
                    connection_close(&conn);
                }
            }
        }

        if (domains_changed()) {
            status_code_2_callback_type(domains_optimize(), &cb);
            status_code_2_callback_type(domains_state_update(), &cb);
            domains_state_save();
            callback(vmm_state, cb);
        }
        else if (time(NULL) - domains_last_save >= SAVE_INTERVAL * 60)
            domains_state_save();
    }

    log_msg(LOG_ERR, err, "Waiting for incoming connection failed");

    close(sock_defrost);
    close(sock_admin);
    vmm_close(&vmm_state);
    close(sock);

    return 0;
}


static void status_code_2_callback_type(int sc, int *cb)
{/*{{{*/
    if (STATUS_IS_OK(sc) && (sc & STATUS_MAGIC_MASK) == SMAGIC_DOMAINS) {
        switch (sc & STATUS_CODE_MASK) {
        case SCDOM_CHANGED_DOMAINS:
            *cb |= CB_SWITCH;
            break;

        case SCDOM_CHANGED_CPUS:
            *cb |= CB_CPU;
            break;

        case SCDOM_CHANGED_DOMAINS_AND_CPUS:
            *cb |= CB_SWITCH | CB_CPU;
            break;

        default:
            break;
        }
    }
}/*}}}*/


static void callback(void *vmm_state, int type)
{/*{{{*/
    const char *cb;
    char *cmd;
    struct domain *dom;
    char vm_state;
    int max;
    int len;
    int ret;
    int noop = 1;

    if (domains_count() == 0)
        return;

    if ((type & (CB_CPU | CB_SWITCH)) == (CB_CPU | CB_SWITCH)) {
        callback(vmm_state, CB_CPU);
        callback(vmm_state, CB_SWITCH);
        return;
    }
    else if (type & CB_CPU) {
        cb = CALLBACK_SWITCH_CPU;
        if (!cpus_enabled())
            return;
    }
    else if (type & CB_SWITCH)
        cb = CALLBACK_SWITCH;
    else
        return;

    max = strlen(cb) + 1 + strlen(vmm_name) + 1 + domains_count() * (9 + 11);

    if ((cmd = (char *) malloc(max)) == NULL) {
        log_msg(LOG_ERR, errno, "Cannot execute callback");
        return;
    }

    len = snprintf(cmd, max, "%s.%s", cb, vmm_name);

    for (dom = domains_first(); dom != NULL; dom = dom->next) {
        switch (domains_status_get(dom)) {
        case UNDEFINED:
        case UP:
        case DOMAINS_STATUS_INTERNAL:
        case DOMAIN_STATUS_LAST:
            /* internal states/separators */
        case REMOVED:
        case DOWN:
	case DOWN_BOOTABLE:
        case FROZEN:
            continue;
        case FREE:
            vm_state = 'f';
            break;
        case OCCUPIED_WOULD_PREEMPT:
        case OCCUPIED:
            vm_state = 'o';
            break;
        case BOOTING:
            if (!dom->rebooting)
                continue;
        case RUNNING_PREEMPTIBLE:
        case RUNNING:
        case RUNNING_CLUSTER:
            vm_state = 'r';
            break;
        case PREEMPTED:
            vm_state = 'p';
            break;
        }

        noop = 0;

        len += snprintf(cmd + len, max - len, " %lu:%c:%u",
                        (unsigned long) dom->id, vm_state, dom->cpus);
    }

    if (noop) {
        free(cmd);
        return;
    }

    if (type & CB_SWITCH) {
        for (dom = domains_first(); dom != NULL; dom = dom->next) {
            switch (domains_status_get(dom)) {
            case UNDEFINED:
            case UP:
            case DOMAINS_STATUS_INTERNAL:
            case DOMAIN_STATUS_LAST:
                /* internal states/separators */
            case REMOVED:
            case DOWN:
            case DOWN_BOOTABLE:
            case BOOTING:
            case RUNNING_PREEMPTIBLE:
            case RUNNING:
            case RUNNING_CLUSTER:
            case FROZEN:
                break;

            case FREE:
            case OCCUPIED_WOULD_PREEMPT:
            case OCCUPIED:
                slave_command(dom, SLAVE_SUSPEND_FREE);
                break;

            case PREEMPTED:
                slave_command(dom, SLAVE_SUSPEND_PREEMPTED);
                break;
            }
        }
    }

    vmm_vm_close(vmm_state);

    cmd[max - 1] = '\0';

    log_msg(LOG_INFO, 0, "Running callback \"%s\"", cmd);

    if ((ret = system_command(cmd)) == -1)
        log_msg(LOG_ERR, errno, "Callback execution failed");
    else if (WIFSIGNALED(ret))
        log_msg(LOG_ERR, 0, "Callback killed with signal %d", WTERMSIG(ret));
    else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
        log_msg(LOG_ERR, 0, "Callback exited with status %d", WEXITSTATUS(ret));

    free(cmd);

    for (dom = domains_first(); dom != NULL; dom = dom->next) {
        switch (domains_status_get(dom)) {
        case UNDEFINED:
        case UP:
        case DOMAINS_STATUS_INTERNAL:
        case DOMAIN_STATUS_LAST:
            /* internal states/separators */
        case REMOVED:
        case DOWN:
        case DOWN_BOOTABLE:
        case FREE:
        case OCCUPIED_WOULD_PREEMPT:
        case OCCUPIED:
        case PREEMPTED:
        case FROZEN:
            continue;

        case BOOTING:
            if (!dom->rebooting)
                continue;

        case RUNNING_PREEMPTIBLE:
        case RUNNING:
        case RUNNING_CLUSTER:
            slave_command(dom, SLAVE_WAKEUP);
        }
    }
}/*}}}*/


static void slave_command(const struct domain *dom, enum slave_command cmd)
{/*{{{*/
    struct connection conn;
    int pos = 0;
    int len = MAX_BUF;
    char buf[MAX_BUF];
    int i;
    unsigned long mem;
    int jobs = 0;

    switch (cmd) {
    case SLAVE_SUSPEND_FREE:
    case SLAVE_SUSPEND_PREEMPTED:
    case SLAVE_WAKEUP:
        if (cmd == SLAVE_SUSPEND_FREE)
            mem = memory.free;
        else if (cmd == SLAVE_SUSPEND_PREEMPTED)
            mem = memory.preempted;
        else
            mem = memory.running;

        jobs = snprintf(buf, len - 1, "%s %lu %d",
                        slave_command_str[cmd], mem, dom->running_jobs);
        break;

    case SLAVE_REBOOT:
        snprintf(buf, len - 1, "%s", slave_command_str[cmd]);
        break;
    }

    if (jobs > 0) {
        pos = jobs;
        for (i = 0; i < dom->running_jobs; i++) {
            if (strlen(dom->jobs[i]->id) >= len - pos) {
                log_msg(LOG_ERR, 0,
                        "Too many jobs; sending %s command without jobs",
                        slave_command_str[cmd]);
                buf[jobs] = '\0';
                break;
            }

            pos += snprintf(buf + pos, len - pos - 1, " '%s'",
                            dom->jobs[i]->id);
        }
    }

    log_msg(LOG_INFO, 0, "Sending %s command to %s: \"%s\"",
            slave_command_str[cmd], dom->fqdn, buf);

    if (connection_init(&conn, net_connect(dom->ip, dom->port, 1))) {
        log_msg(LOG_ERR, 0, "Cannot connect to magrathea-slave daemon");
        return;
    }

    net_write(&conn, buf);

    /* just wait for any reply, we are not interested in result */
    len = 0;
    pos = 0;
    buf[0] = '\0';
    net_read(&conn, buf, &pos, &len, MAX_BUF, 0);

    connection_close(&conn);
}/*}}}*/


static int check_connection(void *vmm_state,
                            const net_sockaddr_t *addr,
                            struct domain **dom)
{/*{{{*/
    net_addr_t ip = net_sockaddr_get_ip(addr);
    in_port_t port = net_sockaddr_get_port(addr);

    if (port >= 1024) {
        log_msg(LOG_DEBUG, 0,
                "Rejected connection from %s:%u: unprivileged source port",
                net_addr_str(ip), port);
        return 0;
    }

    *dom = domains_find_ip(ip);

    if (*dom != NULL) {
        log_msg(LOG_INFO, 0, "Accepted connection from %s:%u, domain %s",
                net_addr_str(ip), port, (*dom)->fqdn);
    }
    else {
        log_msg(LOG_INFO, 0, "Rejected connection from %s:%u, unknown domain",
                net_addr_str(ip), port);
        return 0;
    }

    return 1;
}/*}}}*/


static void handle_connection(void *vmm_state,
                              struct domain *dom,
                              struct connection *conn)
{/*{{{*/
    char buf[MAX_BUF];
    char *line;
    struct args *args;
    int len = 0;
    int pos = 0;

    assert(dom != NULL);

    if ((line = net_read(conn, buf, &pos, &len, MAX_BUF, READ_TIMEOUT)) != NULL
        && (args = line_parse(line)) != NULL) {

        if (strcmp(args->argv[0], "register") == 0)
            cmd_register(CO_SLAVE, vmm_state, conn, dom, args);
        else if (strcmp(args->argv[0], "unregister") == 0)
            cmd_unregister(vmm_state, dom, conn);
        else if (domains_status_get(dom) == BOOTING)
            net_write(conn, "failed: unregistered domain");
        else if (strcmp(args->argv[0], "status") == 0)
            cmd_status(CO_SLAVE, vmm_state, conn, dom, args);
        else if (domains_booting())
            net_write(conn, "failed: booting a domain");
        else if (domains_status_get(dom) == FROZEN) {
            log_msg(LOG_ERR, 0, "Connection initiated by a frozen domain");
            net_write(conn, "failed: the domain is supposed to be frozen");
        }
        else if ( (strcmp(args->argv[0], "reboot") == 0) ||
		  (strcmp(args->argv[0], "force-reboot") == 0))
            cmd_reboot(CO_SLAVE, vmm_state, conn, dom, args);
        else if (((strcmp(args->argv[0], "startjob") == 0)
                  || (strcmp(args->argv[0], "startjob-preemptible") == 0)
                  || (strcmp(args->argv[0], "startcluster") == 0))
                 && (args->argc > 1)) {
            int cpus = 0;
            int preempt = strcmp(args->argv[0], "startjob-preemptible") == 0;
	    int cluster = strcmp(args->argv[0], "startcluster") == 0;

            if (args->argc > 2)
                cpus = atoi(args->argv[2]);

            cmd_job(vmm_state, 1, dom, args->argv[1], cpus, preempt, conn,cluster);
        }
        else if (strcmp(args->argv[0], "stopjob") == 0 && args->argc > 1)
            cmd_job(vmm_state, 0, dom, args->argv[1], 0, 0, conn,0);
        else if (strcmp(args->argv[0], "stopcluster") == 0 && args->argc > 1)
            cmd_job(vmm_state, 0, dom, args->argv[1], 0, 0, conn,1);
        else if (strcmp(args->argv[0], "stopalljobs") == 0)
            cmd_job(vmm_state, 0, dom, NULL, 0, 0, conn,0);
        else if (strcmp(args->argv[0], "preempted") == 0 && args->argc > 1) 
            cmd_preempted(CO_ADMIN,vmm_state, conn,dom,args);
        else if (strcmp(args->argv[0], "freeze") == 0)
            cmd_freeze(CO_SLAVE, vmm_state, conn, dom, args);
        else {
            line_parse_undo(args);
            net_write(conn,
                "failed: unknown request or insufficient parameters: %s",
                line);
        }

        free(args);
    }
}/*}}}*/


static void admin_connection(void *vmm_state, struct connection *conn)
{/*{{{*/
    char buf[MAX_BUF];
    char *line;
    struct args *args;
    int len = 0;
    int pos = 0;
    struct domain *dom;
    int i;
    const struct command *command;

    if ((line = net_read(conn, buf, &pos, &len, MAX_BUF, READ_TIMEOUT)) != NULL
        && *line != '\n' &&  *line != '\0') {
        if ((args = line_parse(line)) == NULL) {
            net_write(conn, "failed: cannot parse request: %s", line);
            return;
        }

        for (i = 0; i < admin_commands_size; i++) {
            if (strcmp(admin_commands[i].name, args->argv[0]) == 0)
                break;
        }
        command = admin_commands + i;

        if (command->name == NULL) {
            line_parse_undo(args);
            net_write(conn, "failed: unknown request: %s", line);
            return;
        }

        if (domains_booting() && !command->booting) {
            net_write(conn, "failed: booting a domain");
            return;
        }

        if (command->fqdn && args->argc < 2) {
            net_write(conn, "failed: missing domain's FQDN");
            free(args);
            return;
        }

        dom = NULL;
        if (args->argc > 1) {
            dom = domains_find_fqdn(args->argv[1]);

            if (dom == NULL && strcmp(command->name, "create") != 0) {
                net_write(conn, "failed: domain not found");
                free(args);
                return;
            }
        }

        command->handler(CO_ADMIN, vmm_state, conn, dom, args);
        free(args);
    }
}/*}}}*/


static void defrost_connection(void *vmm_state,
                               const net_sockaddr_t *addr,
                               struct connection *conn)
{/*{{{*/
    char buf[MAX_BUF];
    char *line;
    struct args *args;
    int len = 0;
    int pos = 0;
    net_addr_t ip = net_sockaddr_get_ip(addr);
    in_port_t port = net_sockaddr_get_port(addr);

    if (port >= 1024) {
        log_msg(LOG_DEBUG, 0,
                "Rejected connection from %s:%u: unprivileged source port",
                net_addr_str(ip), port);
        return;
    }
    if (domains_booting()) {
        log_msg(LOG_INFO, 0,
                "Rejected defrost connection from %s:%u: booting a domain",
                net_addr_str(ip), port);
        net_write(conn, "failed: booting a domain");
        return;
    }

    log_msg(LOG_INFO, 0,
            "Accepted defrost connection from %s:%u",
            net_addr_str(ip), port);

    if ((line = net_read(conn, buf, &pos, &len, MAX_BUF,
                         READ_TIMEOUT)) != NULL
        && (args = line_parse(line)) != NULL) {

        if (strcmp(args->argv[0], "defrost") == 0
            && args->argc > 1) {
            cmd_defrost(CO_DEFROST, vmm_state, conn,
                        domains_find_fqdn(args->argv[1]), args);
        }
        else {
            line_parse_undo(args);
            net_write(conn,
                "failed: unknown request or insufficient parameters: %s",
                line);
        }

        free(args);
    }
}/*}}}*/


static int defrost_socket(const char *addr, int port)
{/*{{{*/
    net_addr_t ip;
    int sock;
    char *defrost_addr;
    int addrlen;

    if (!net_addr_parse(addr, &ip)) {
        log_msg(LOG_ERR, 0, "Invalid IP address: %s", addr);
        return -1;
    }

    if ((sock = net_socket(ip, &port, 0, 1)) < 0)
        return -1;

    if (listen(sock, 5)) {
        log_msg(LOG_ERR, errno, "Cannot start listening on %s port %d",
                addr, port);
        return -1;
    }

    addrlen = strlen(addr);
    if (addrlen < INET_ADDRSTRLEN)
        addrlen = INET_ADDRSTRLEN;

    /*                             [   IP addr   ]   :  port \0 */
    defrost_addr = (char *) malloc(1 + addrlen + 1 + 1 + 5 + 1);
    if (defrost_addr == NULL) {
        log_msg(LOG_ERR, 0, "Not enough memory");
        close(sock);
        return -1;
    }

    switch (net_addr_type(ip)) {
    case NADDR_IPV4:
        sprintf(defrost_addr, "%s:%u", addr, port);
        break;
    case NADDR_IPV6_MAPPED_IPV4:
        sprintf(defrost_addr, "%s:%u", net_addr_str(ip), port);
        break;
    case NADDR_IPV6:
        sprintf(defrost_addr, "[%s]:%u", addr, port);
        break;
    }

    domains_defrost_address(defrost_addr);

    return sock;
}/*}}}*/


static void cmd_register(enum command_origin origin,
                         void *vmm_state,
                         struct connection *conn,
                         struct domain *dom,
                         struct args *args)
{/*{{{*/
    int port_number;
    vmid_t id;
    int cb = CB_NONE;
    int sc;

    if (args->argc < 3) {
        net_write(conn, "failed: too few arguments to \"register\" command");
        return;
    }

    port_number = atoi(args->argv[2]);
    if (port_number <= 0 || port_number > 0xffff) {
        net_write(conn, "failed: invalid port number");
        return;
    }

    if (vmm_vm_name2id(dom->name, vmm_state, &id)) {
        net_write(conn, "failed: cannot decode domain's ID");
        return;
    }

    sc = domains_register(args->argv[1], id, port_number);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot register domain %s: %s",
                args->argv[1], status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    status_code_2_callback_type(sc, &cb);
    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    log_msg(LOG_INFO, 0, "Domain registered: %s", args->argv[1]);
    net_write(conn, "ok");
}/*}}}*/


static void cmd_unregister(void *vmm_state,
                           struct domain *dom,
                           struct connection *conn)
{/*{{{*/
    int sc;

    if (dom != NULL) {
        sc = domains_jobs_stop(dom, NULL);
        if (!STATUS_IS_ERROR(sc))
            sc = domains_remove(dom);

        if (STATUS_IS_ERROR(sc)) {
            log_msg(LOG_ERR, 0, "Cannot remove domain %s: %s",
                    dom->fqdn, status_decode(sc));
            net_write(conn, "failed: %s", status_decode(sc));
            return;
        }
    }

    domains_state_save();

    net_write(conn, "ok");
}/*}}}*/


static void cmd_status(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args)
{/*{{{*/
    domains_preemption_update();

    if (dom != NULL)
        net_write(conn, domains_status_full(dom, domains_booting()));
    else {
        net_write(conn, "ok LIST");

        for (dom = domains_first(); dom != NULL; dom = dom->next) {
            net_write(conn, "%s %s",
                      dom->fqdn, domains_status_full(dom, domains_booting()));
        }

        net_write(conn, "/LIST");
    }
}/*}}}*/


static void cmd_preempted(enum command_origin origin,
			  void *vmm_state,
                          struct connection *conn,
                          struct domain *dom,
			  struct args *args)
{/*{{{*/
    if (args->argc < 3)
	net_write(conn, "failed: missing arguments");
    else 
	net_write(conn, "%ld", domains_jobs_preempted_time(dom, args->argv[2], 0));
}/*}}}*/


static void cmd_job(void *vmm_state,
                    int start,
                    struct domain *dom,
                    const char *jobid,
                    int cpus,
                    int preemptible,
                    struct connection *conn,
		    int cluster)
{/*{{{*/
    char *action;
    char *action_ing;
    int sc;

    if (cpus_enabled() && cpus == 0)
        cpus = cpus_count(CPUS_NODE);

    if (start) {
        if (jobid == NULL) {
            net_write(conn, "failed: no job specified");
            return;
        }

        action = "start";
        action_ing = "Starting";
        sc = domains_jobs_start(dom, jobid, cpus, preemptible, cluster);
    }
    else {
        action = "stop";
        action_ing = "Stopping";
        sc = domains_jobs_stop(dom, jobid);
    }

    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot %s job %s%sin domain %s: %s",
                action, (jobid != NULL) ? jobid : "",
                (jobid != NULL) ? " " : "",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }
    else {
        int cb = CB_NONE;

        status_code_2_callback_type(sc, &cb);
        status_code_2_callback_type(domains_optimize(), &cb);
        status_code_2_callback_type(domains_state_update(), &cb);
        domains_state_save();
        callback(vmm_state, cb);

        if (STATUS_IS_WARNING(sc)) {
            log_msg(LOG_WARNING, 0, "%s job %s%sin domain %s: %s",
                    action_ing, (jobid != NULL) ? jobid : "",
                    (jobid != NULL) ? " " : "",
                    dom->fqdn, status_decode(sc));
            net_write(conn, "warning: %s", status_decode(sc));
        }
        else
            net_write(conn, "ok");
    }
}/*}}}*/


static void cmd_create(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args)
{/*{{{*/
    int priority = 0;
    int sc;
    const char *fqdn;
    const char *name;
    net_addr_t ip;

    if (args->argc < 5) {
        net_write(conn, "failed: too few arguments to \"create\" command");
        return;
    }

    fqdn = args->argv[1];
    name = args->argv[2];

    if (strcmp(args->argv[3], "priority") == 0)
        priority = 1;

    if (!net_addr_parse(args->argv[4], &ip)) {
        net_write(conn, "failed: invalid IP address: %s", args->argv[4]);
        return;
    }

    sc = domains_create(fqdn, name, priority, ip);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot create domain %s: %s",
                fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    domains_state_save();

    log_msg(LOG_INFO, 0, "New domain created: %s, IP: %s",
            fqdn, net_addr_str(ip));

    net_write(conn, "ok");
}/*}}}*/


static void cmd_boot(enum command_origin origin,
                     void *vmm_state,
                     struct connection *conn,
                     struct domain *dom,
                     struct args *args)
{/*{{{*/
    int cb = CB_NONE;
    int sc;
    int ret;
    char *cmd;

    assert(dom != NULL);

    sc = domains_boot(dom);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot boot domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    status_code_2_callback_type(sc, &cb);
    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    net_write(conn, "ok");


    if (strcmp(args->argv[0], "force-boot") == 0) {
        if ((cmd = (char *) malloc(strlen(dom->name) + 11)) == NULL) {
            log_msg(LOG_ERR, errno, "Cannot force boot - "
                                    "use xm create <domain>");
            return;
        }
        sprintf(cmd, "xm create %s", dom->name); 
        cmd[strlen(cmd)] = '\0';

        log_msg(LOG_INFO, 0, "Booting domain: \"%s\"", cmd);

        if ((ret = system_command(cmd)) == -1)
            log_msg(LOG_ERR, errno, "Boot execution failed");
        else if (WIFSIGNALED(ret))
            log_msg(LOG_ERR, 0, "Boot process killed with signal %d",
                    WTERMSIG(ret));
        else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
            log_msg(LOG_ERR, 0, "Boot program exited with status %d",
                    WEXITSTATUS(ret));

        free(cmd);
    }

}/*}}}*/


static void cmd_reboot(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args)
{/*{{{*/
    int cb = CB_NONE;
    int sc;
    int ret;
    char *cmd;

    assert(dom != NULL);

    sc = domains_reboot(dom);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot reboot domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    slave_command(dom, SLAVE_REBOOT);

    status_code_2_callback_type(sc, &cb);
    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    log_msg(LOG_INFO, 0, "Domain marked as booting: %s", dom->fqdn);
    net_write(conn, "ok");


    if (strcmp(args->argv[0], "force-reboot") == 0) {
        if ((cmd = (char *) malloc(strlen(dom->name) + 11)) == NULL) {
            log_msg(LOG_ERR, errno, "Cannot force reboot - use shutdown -r");
            return;
        }
        sprintf(cmd, "xm reboot %s", dom->name); 
        cmd[strlen(cmd)] = '\0';

        log_msg(LOG_INFO, 0, "Rebooting domain: \"%s\"", cmd);

        if ((ret = system_command(cmd)) == -1)
            log_msg(LOG_ERR, errno, "Reboot execution failed");
        else if (WIFSIGNALED(ret))
            log_msg(LOG_ERR, 0, "Reboot process killed with signal %d",
                    WTERMSIG(ret));
        else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
            log_msg(LOG_ERR, 0, "Reboot program exited with status %d",
                    WEXITSTATUS(ret));

         free(cmd);
    }

}/*}}}*/


static void cmd_down(enum command_origin origin,
                     void *vmm_state,
                     struct connection *conn,
                     struct domain *dom,
                     struct args *args)
{/*{{{*/
    int cb = CB_NONE;
    int sc;
    int ret;
    char *cmd;


    assert(dom != NULL);

    if (dom->running_jobs) cmd_job(vmm_state, 0, dom, NULL, 0, 0, conn, 0);

    sc = domains_down(dom);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot get down domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    status_code_2_callback_type(sc, &cb);
    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    log_msg(LOG_INFO, 0, "Domain marked as being down: %s", dom->fqdn);
    net_write(conn, "ok");

 
    if (strcmp(args->argv[0], "force-down") == 0) {
        if ((cmd = (char *) malloc(strlen(dom->name) + 13)) == NULL) {
            log_msg(LOG_ERR, errno, "Cannot force remote shutdown - "
                                    "use /sbin/shutdown locally");
            return;
        }
        sprintf(cmd, "xm shutdown %s", dom->name); 
        cmd[strlen(cmd)] = '\0';

        log_msg(LOG_INFO, 0, "Shutting down domain: \"%s\"", cmd);

        if ((ret = system_command(cmd)) == -1)
            log_msg(LOG_ERR, errno, "Shutdown failed");
        else if (WIFSIGNALED(ret))
            log_msg(LOG_ERR, 0, "Shutdown process killed with signal %d",
                    WTERMSIG(ret));
        else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
            log_msg(LOG_ERR, 0, "Shutdown program exited with status %d",
                    WEXITSTATUS(ret));

        free(cmd);
    }

}/*}}}*/


static void cmd_remove(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args)
{/*{{{*/
    int sc;
    char *fqdn;

    assert(dom != NULL);

    fqdn = strdup(dom->fqdn);

    sc = domains_remove(dom);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot remove domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        free(fqdn);
        return;
    }

    domains_state_save();

    log_msg(LOG_INFO, 0, "Domain %s removed", fqdn);
    free(fqdn);

    net_write(conn, "ok");
}/*}}}*/


static void cmd_priority(enum command_origin origin,
                         void *vmm_state,
                         struct connection *conn,
                         struct domain *dom,
                         struct args *args)
{/*{{{*/
    int priority = 0;
    int cb = CB_NONE;
    int sc;

    assert(dom != NULL);

    if (args->argc < 3) {
        net_write(conn, "too few arguments to \"priority\" command");
        return;
    }

    if (strcmp(args->argv[2], "priority") == 0)
        priority = 1;

    log_msg(LOG_INFO, 0, "Changing priority of %s to %s",
            dom->fqdn, args->argv[2]);

    sc = domains_priority(dom, priority);
    if (STATUS_IS_ERROR(sc)) {
        log_msg(LOG_ERR, 0, "Cannot change priority of domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    log_msg(LOG_INFO, 0, "Domain %s is %s", dom->fqdn, args->argv[2]);
    net_write(conn, "ok");
}/*}}}*/


static void cmd_freeze(enum command_origin origin,
                       void *vmm_state,
                       struct connection *conn,
                       struct domain *dom,
                       struct args *args)
{/*{{{*/
    int sc;
    int cb = CB_NONE;
    int frozen = 0;

    if (dom == NULL) {
        net_write(conn, "failed: unregistered domain");
        return;
    }

    if (STATUS_IS_ERROR(sc = domains_freeze(dom))) {
        log_msg(LOG_ERR, 0, "Cannot freeze domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    /* We need to send answer to the domain before it gets actually frozen. */
    if (origin == CO_SLAVE) {
        net_write(conn, "ok");
        connection_close(conn);
    }

    if (vmm_vm_suspend(dom->fqdn, dom->id, vmm_state)) {
        log_msg(LOG_ERR, 0, "Cannot freeze domain %s, aborting...",
                dom->fqdn);
        domains_defrost(dom);
    }
    else {
        frozen = 1;
        log_msg(LOG_INFO, 0, "Domain %s frozen", dom->fqdn);

        status_code_2_callback_type(sc, &cb);
    }

    status_code_2_callback_type(domains_optimize(), &cb);
    status_code_2_callback_type(domains_state_update(), &cb);
    domains_state_save();
    callback(vmm_state, cb);

    if (origin != CO_SLAVE) {
        if (frozen)
            net_write(conn, "ok");
        else
            net_write(conn, "failed: aborted");
    }
}/*}}}*/


static void cmd_defrost(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args)
{/*{{{*/
    int sc;

    if (dom == NULL) {
        log_msg(LOG_ERR, 0, "Cannot defrost domain %s: domain not found",
                args->argv[1]);
        net_write(conn, "failed: domain not found");
        return;
    }

    if (domains_status_get(dom) != FROZEN) {
        log_msg(LOG_WARNING, 0, "Cannot defrost unfrozen domain %s",
                dom->fqdn);
        net_write(conn, "warning: domain is not frozen");
        return;
    }

    if (STATUS_IS_ERROR(sc = domains_defrost(dom))) {
        log_msg(LOG_ERR, 0, "Cannot defrost domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }
    else {
        int cb = CB_NONE;
        vmid_t id;

        /* apply preemption */
        status_code_2_callback_type(sc, &cb);
        if (cb & CB_SWITCH) {
            enum domain_status old = domains_status_get(dom);
            domains_status_set(dom, FROZEN);
            domains_state_save();
            callback(vmm_state, CB_SWITCH);
            domains_status_set(dom, old);
            cb ^= CB_SWITCH;
        }

        if (vmm_vm_resume(dom->fqdn, dom->name, dom->ip,
                          &id, dom->id, vmm_state)) {
            log_msg(LOG_ERR, 0, "Cannot defrost domain %s, aborting...",
                    dom->fqdn);
            net_write(conn, "failed: cannot defrost domain");

            status_code_2_callback_type(domains_freeze(dom), &cb);
        }
        else {
            log_msg(LOG_INFO, 0, "Domain %s defrosted", dom->fqdn);
            net_write(conn, "ok");

            if (id != dom->id) {
                log_msg(LOG_INFO, 0,
                        "Domain %s has ID %lu after defrosting",
                        dom->fqdn, id);
                dom->id = id;
            }
        }

        status_code_2_callback_type(domains_optimize(), &cb);
        status_code_2_callback_type(domains_state_update(), &cb);
        domains_state_save();
        callback(vmm_state, cb);
    }
}/*}}}*/


static void cmd_startjob(enum command_origin origin,
                         void *vmm_state,
                         struct connection *conn,
                         struct domain *dom,
                         struct args *args)
{
    if (args->argc < 4)
        net_write(conn, "failed: missing arguments");
    else {
        if (strcmp(args->argv[0] ,"startjob-preemptible") == 0)
            cmd_job(vmm_state, 1, dom, args->argv[2],
                    atoi(args->argv[3]), 1, conn, 0);
        else if (strcmp(args->argv[0] ,"startcluster") == 0)
            cmd_job(vmm_state, 1, dom, args->argv[2],
                    atoi(args->argv[3]), 0, conn, 1);
        else
            cmd_job(vmm_state, 1, dom, args->argv[2],
                    atoi(args->argv[3]), 0, conn, 0);
    }
}


static void cmd_stopjob(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args)
{/*{{{*/
    if (strcmp(args->argv[0], "stopalljobs") == 0)
        cmd_job(vmm_state, 0, dom, NULL, 0, 0, conn,0);
    else {
        if (args->argc < 3)
            net_write(conn, "failed: missing job ID");
        else {
           if (strcmp(args->argv[0], "stopcluster") == 0)
               cmd_job(vmm_state, 0, dom, args->argv[2], 0, 0, conn,1);
           else
               cmd_job(vmm_state, 0, dom, args->argv[2], 0, 0, conn,0);
        }
    }
}/*}}}*/


static void cmd_master_ping(enum command_origin origin,
                            void *vmm_state,
                            struct connection *conn,
                            struct domain *dom,
                            struct args *args)
{/*{{{*/
    net_write(conn, "ok");
}/*}}}*/


static void cmd_master_stop(enum command_origin origin,
                            void *vmm_state,
                            struct connection *conn,
                            struct domain *dom,
                            struct args *args)
{/*{{{*/
    domains_check(vmm_state);
    domains_state_save();
    reporter_propagate();

    net_write(conn, "ok");

    log_msg(LOG_INFO, 0, "Terminating");

    exit(0);
}/*}}}*/


static void cmd_cluster(enum command_origin origin,
                        void *vmm_state,
                        struct connection *conn,
                        struct domain *dom,
                        struct args *args)
{/*{{{*/
    const char *cluster;
    int sc;

    if (dom == NULL) {
        net_write(conn, "ok LIST");

        for (dom = domains_first(); dom != NULL; dom = dom->next) {
            net_write(conn, "%s %s",
                      dom->fqdn, (dom->cluster != NULL) ? dom->cluster : "");
        }

        net_write(conn, "/LIST");
        return;
    }

    if (args->argc == 2) {
        net_write(conn, "%s", (dom->cluster != NULL) ? dom->cluster : "");
        return;
    }

    if (strcmp(args->argv[2], "remove") == 0)
        cluster = NULL;
    else if (strcmp(args->argv[2], "set") == 0) {
        if (args->argc < 4) {
            net_write(conn, "failed: missing cluster name");
            return;
        }

        cluster = args->argv[2] + 4 /* strlen("set ")*/;
        line_parse_undo(args);
    }
    else {
        net_write(conn, "failed: unknown cluster command: %s", args->argv[2]);
        return;
    }

    if (STATUS_IS_ERROR(sc = domains_cluster(dom, cluster))) {
        log_msg(LOG_ERR, 0, "Cannot set cluster for domain %s: %s",
                dom->fqdn, status_decode(sc));
        net_write(conn, "failed: %s", status_decode(sc));
        return;
    }

    domains_state_save();
    net_write(conn, "ok");
}/*}}}*/

