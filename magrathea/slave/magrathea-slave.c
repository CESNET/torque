/** $Id: magrathea-slave.c,v 1.22 2008/10/01 14:48:20 xdenemar Exp $ */

/** @file
 * Slave daemon for magrathea.
 * It is running within each VM managed by magrathea.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.22 $ $Date: 2008/10/01 14:48:20 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <regex.h>

#include "../utils.h"
#include "../net.h"
#include "magrathea-slave.h"
#include "versions-magrathea-slave.h"


/** Callback for executing commands on requests of magrathea-master daemon.
 *
 * The callback is executed with the following parameters:
 *  @li command
 *  @li list of jobs running in the VM
 */
#define CALLBACK            CONF_DIR "/master-cmd"

/** Default PID file. */
#define PID_FILE            "/var/run/magrathea-slave.pid"

/** Regular expression each command send from master has to match. */
#define RE_MASTER_COMMAND   "^[-[:alnum:]]+( [^;&(){}]*)?$"


/** Number of seconds to wait between two register commands. */
#define REGISTER_TIMEOUT    30


struct state {
    char *master;
    net_addr_t *master_addr;
    int master_count;
    int master_port;
    int port;
    sem_t listener;
    int registered;
    const char *fqdn;
    int rebooting;
};


static struct message *message_queue = NULL;
static int message_count = 0;


static void send_queue(struct state *state);


int master_connect(struct state *state, char *details, int size)
{/*{{{*/
    int sock;

    sock = net_connect_name(state->master, state->master_port, 1);
    if (sock < 0) {
        log_msg(LOG_ERR, 0, "Cannot connect to magrathea-master daemon");

        if (details != NULL)
            strncpy(details, "cannot connect to magrathea-master", size);

        return -1;
    }
    else
        return sock;
}/*}}}*/


int request_send(struct state *state,
                 const char *request,
                 char *details,
                 int size)
{/*{{{*/
    struct connection conn;
    int pos = 0;
    int len = 0;
    char buf[MAX_BUF];
    char *line;

    if (state->rebooting) {
        strncpy(details, "domain is going down for reboot", size);
        return -1;
    }

    if (connection_init(&conn, master_connect(state, details, size)))
        return -1;

    if (connection_gss(&conn, CONN_CLIENT, "orkuz.ics.muni.cz", "host")) {
        connection_close(&conn);
        return -1;
    }

    if (details != NULL && *details != '\0')
        net_write(&conn, "%s %s", request, details);
    else
        net_write(&conn, "%s", request);

    line = net_read(&conn, buf, &pos, &len, MAX_BUF, 0);

    connection_close(&conn);

    if (line == NULL) {
        strncpy(details, "connection reset by peer", size);
        return -1;
    }

    if (strncmp(line, "failed: ", 8) == 0) {
        line += 8;

        if (strstr(line, "unregistered") != NULL)
            state->registered = 0;
        else if (details != NULL) {
            strncpy(details, line, size);
            details[size - 1] = '\0';
        }

        return -1;
    }
    else {
        if (details != NULL) {
            strncpy(details, line, size);
            details[size - 1] = '\0';
        }

        if (strncmp(request, "register", 8) == 0)
            state->registered = 1;

        return 0;
    }
}/*}}}*/


int master_register(struct state *state, char *response, int size)
{/*{{{*/
    char req[1024];

    snprintf(req, 1024, "register %s %u", state->fqdn, state->port);
    req[1023] = '\0';

    if (response != NULL)
        *response = '\0';

    if (request_send(state, req, response, size)) {
        int details = (response != NULL && *response != '\0');
        log_msg(LOG_ERR, 0,
                "Cannot register domain at magrathea-master daemon%s%s",
                (details) ? ": " : "",
                (details) ? response : "");

        return -1;
    }

    return 0;
}/*}}}*/


int master_request(struct state *state,
                   struct message *msg,
                   int process_queue)
{/*{{{*/
    int queued = 0;

    if (process_queue) {
        send_queue(state);

        if (msg->cmd == CMD_JOB_STOP) {
            void *p;
            p = realloc(message_queue,
                        sizeof(struct message) * (message_count + 1));
            if (p == NULL) {
                strcpy(msg->details, "failed: internal error");
                return 1;
            }

            message_queue = (struct message *) p;
            message_queue[message_count] = *msg;
            message_count++;
            queued = 1;
        }

        if (message_count > 1 && queued) {
            strcpy(msg->details, "queued");
            return 0;
        }
    }

    if (request_send(state, command_string[msg->cmd],
                     msg->details, MAX_DETAILS)) {
        if (!state->registered) {
            if (master_register(state, NULL, 0)) {
                if (queued) {
                    strcpy(msg->details, "queued");
                    return 0;
                }
                else
                    return 1;
            }

            if (!request_send(state, command_string[msg->cmd],
                              msg->details, MAX_DETAILS)) {
                if (queued)
                    message_count--;

                return 0;
            }
        }

        log_msg(LOG_ERR, 0, "Request '%s' failed%s%s",
                command_string[msg->cmd],
                (*msg->details != '\0') ? ": " : "",
                msg->details);

        if (queued) {
            strcpy(msg->details, "queued");
            return 0;
        }
        else
            return 1;
    }

    if (queued)
        message_count--;

    return 0;
}/*}}}*/


static void send_queue(struct state *state)
{/*{{{*/
    struct message msg;

    if (message_count > 0) {
        log_msg(LOG_INFO, 0,
                "Sending queued requests to magrathea-master daemon");
    }

    while (message_count > 0) {
        msg = *message_queue;
        if (master_request(state, &msg, 0))
            break;
        else {
            if (message_count > 1)
                memmove(message_queue, message_queue + 1,
                        sizeof(struct message) * (message_count - 1));
            message_count--;
        }
    }
}/*}}}*/


static void callback(const char *command)
{/*{{{*/
    char *cmd;
    int max;
    int len;
    int ret;

    max = strlen(CALLBACK) + 1 + strlen(command) + 1;
    if ((cmd = (char *) malloc(max)) == NULL) {
        log_msg(LOG_ERR, errno, "Cannot execute callback");
        return;
    }

    len = snprintf(cmd, max, "%s %s", CALLBACK, command);
    cmd[max - 1] = '\0';

    log_msg(LOG_INFO, 0, "Running callback \"%s\"", cmd);

    ret = system_command(cmd);
    free(cmd);

    if (ret == -1)
        log_msg(LOG_ERR, errno, "Callback execution failed");
    else if (WIFSIGNALED(ret))
        log_msg(LOG_ERR, 0, "Callback killed with signal %d", WTERMSIG(ret));
    else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
        log_msg(LOG_ERR, 0, "Callback exited with status %d", WEXITSTATUS(ret));
}/*}}}*/


void *listener(void *arg)
{/*{{{*/
    struct state *state = (struct state *) arg;
    net_addr_t ip = net_inaddr_any;
    int port = 0;
    net_sockaddr_t addr;
    socklen_t addrlen;
    int sock;
    struct connection conn;
    int pos = 0;
    int len = 0;
    char buf[MAX_BUF];
    char *line;
    regex_t re;
    int err;

    err = regcomp(&re, RE_MASTER_COMMAND, REG_EXTENDED | REG_NOSUB);
    if (err != 0) {
        char str[100];
        regerror(err, &re, str, 100);
        log_msg(LOG_ERR, 0, "Cannot compile regular expression: %s", str);
        exit(1);
    }

    if ((sock = net_socket(ip, &port, 0, 1)) == -1
        || listen(sock, 1)) {
        log_msg(LOG_ERR, errno, "Socket initialization failed");
        exit(1);
    }

    state->port = port;

    log_msg(LOG_NOTICE, 0, "Listening on %s port %u",
            net_addr_str(ip), state->port);

    sem_post(&state->listener);

    addrlen = sizeof(addr);
    while (connection_init(&conn,
                accept(sock, (struct sockaddr *) &addr, &addrlen))) {
        int reject = 1;

        port = net_sockaddr_get_port(&addr);
        ip = net_sockaddr_get_ip(&addr);

        if (port < 1024) {
            int i;

            /*TODO replace with GSS-API check for client name!! */
            for (i = 0; i < state->master_count; i++) {
                if (net_addr_equal(ip, state->master_addr[i])) {
                    reject = 0;
                    break;
                }
            }
        }

        if (!reject) {
            log_msg(LOG_DEBUG, 0,
                    "Accepted connection from magrathea-master %s:%u",
                    net_addr_str(ip), port);
        }
        else {
            log_msg(LOG_INFO, 0, "Rejected connection from %s:%u",
                    net_addr_str(ip), port);
            connection_close(&conn);
            continue;
        }

        if ((line = net_read(&conn, buf, &pos, &len, MAX_BUF, 0)) != NULL) {
            if (regexec(&re, line, 0, NULL, 0) == 0) {
                if (strcmp(line, "reboot") == 0) {
                    log_msg(LOG_INFO, 0, "Received 'reboot' command; "
                            "the domain is going down for reboot");
                    state->rebooting = 1;
                }
                else
                    callback(line);

                net_write(&conn, "ok");
            }
            else {
                net_write(&conn, "failed: invalid command, must match \"%s\"",
                          RE_MASTER_COMMAND);
            }
        }

        connection_close(&conn);
    }

    regfree(&re);

    return NULL;
}/*}}}*/


#define USAGE   \
    "Usage:   %s [-hv46] [-d|-p pid_file] <fqdn> <master-fqdn> [master-port]\n" \
    "Options:\n" \
    "   -h  print usage\n" \
    "   -v  print version information\n" \
    "   -4  force IPv4\n" \
    "   -6  prefer IPv6\n" \
    "   -d  print debug information (printed to stdout instead of syslog)\n" \
    "       the process will remain running in foreground\n" \
    "   -p  pid_file\n" \
    "       PID file (defaults to " PID_FILE ")\n"

static void usage(int error, char *argv0)
{/*{{{*/
    if (error)
        log_msg(LOG_ERR, 0, USAGE, argv0);
    else
        printf(USAGE "\n", argv0);

    exit(error);
}/*}}}*/


int main(int argc, char **argv)
{
    int opt;
    struct state state;
    int sock;
    int fd;
    struct message msg;
    struct msghdr hdr;
    struct iovec iov;
    int size;
    pthread_t thread;
    int detach = 1;
    char *pidfile = PID_FILE;
    enum net_protocol protocol = NPROTO_AUTO;

    log_init("magrathea-slave");

    memset(&state, 0, sizeof(state));

    while ((opt = getopt(argc, argv, "dhv46p:")) != -1) {
        switch (opt) {
        case 'd':
            log_stderr();
            log_debug(1);
            detach = 0;
            break;
        case 'p':
            pidfile = optarg;
            break;
        case 'h':
            usage(0, argv[0]);
            break;
        case 'v':
            printf("magrathea-slave daemon\n%s", VERSIONS);
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

    if (optind + 1 >= argc)
        usage(2, argv[0]);

    log_msg(LOG_NOTICE, 0, "Starting magrathea version " VERSION_TAG);

    net_proto(protocol);

    state.fqdn = argv[optind];

    state.master = argv[optind + 1];
    state.master_addr = net_addr_resolve(state.master, &state.master_count);

    if (state.master_addr == NULL) {
        log_msg(LOG_ERR, 0, "Cannot resolve master's address");
        usage(2, argv[0]);
    }

    if (optind + 2 < argc) {
        state.master_port = atoi(argv[optind + 2]);

        if (state.master_port <= 0 || state.master_port > 0xffff)
            usage(2, argv[0]);
    }
    else
        state.master_port = DEFAULT_PORT;


    if (detach && daemonize(pidfile, &terminate_handler)) {
        log_msg(LOG_ERR, errno, "Daemonization failed");
        return 1;
    }

    /* ignore broken pipe signal which is sent when we try to write to
     * closed connection */
    signal(SIGPIPE, SIG_IGN);

    state.port = 0;
    sem_init(&state.listener, 0, 0);

    if (pthread_create(&thread, NULL, listener, (void *) &state)) {
        log_msg(LOG_ERR, errno,
                "Cannot start thread for listening to "
                "massages from magrathea-master daemon");
        return 1;
    }

    sem_wait(&state.listener);
    if (state.port == 0)
        return 1;

    if ((sock = net_socket_un(SLAVE_SOCKET)) == -1)
        return 1;

    master_register(&state, NULL, 0);

    while (1) {
        fd_set fds;
        struct timeval tv;
        int ret;
        int err;

        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = REGISTER_TIMEOUT;
        tv.tv_usec = 0;

        ret = select(sock + 1, &fds, NULL, NULL, &tv);
        err = errno;

        if (!state.registered)
            master_register(&state, NULL, 0);

        send_queue(&state);

        if (ret == -1) {
            if (err != EINTR) {
                log_msg(LOG_ERR, err,
                        "Error while listening on socket %s", SLAVE_SOCKET);
                break;
            }
        }
        else if (ret != 0 && (fd = accept(sock, NULL, NULL)) != -1) {
            memset(&hdr, 0, sizeof(hdr));
            iov.iov_base = &msg;
            iov.iov_len = sizeof(msg);
            hdr.msg_iov = &iov;
            hdr.msg_iovlen = 1;

            if ((size = recvmsg(fd, &hdr, 0)) < 0)
                log_msg(LOG_ERR, errno,
                        "Cannot read request from %s", SLAVE_SOCKET);
            else if (size != sizeof(msg)
                     || msg.cmd >= CMD_RESPONSE
                     || msg.cmd < 0)
                log_msg(LOG_ERR, 0, "Invalid request from %s", SLAVE_SOCKET);
            else {
                msg.ok = !master_request(&state, &msg, 1);
                msg.cmd = CMD_RESPONSE;

                if (sendmsg(fd, &hdr, 0) != sizeof(msg)) {
                    log_msg(LOG_ERR, errno, "Cannot send response to %s",
                            SLAVE_SOCKET);
                }
            }

            close(fd);
        }
    }

    close(sock);

    return 0;
}

