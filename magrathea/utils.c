/* $Id: utils.c,v 1.22 2008/10/01 14:48:20 xdenemar Exp $ */

/** @file
 * Utility functions for all magrathea components.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.22 $ $Date: 2008/10/01 14:48:20 $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <arpa/inet.h>

#ifndef NET_NO_IO
# include "net.h"
#endif

#include "utils.h"

#define LINE_SIZE 1024

static int max_level = LOG_INFO;
static int old_level = LOG_INFO;
static int out_stderr = 0;

static void sig_debug(int sig);
static void set_signal(void);


static int connection_read(struct connection *conn,
                           void *buf,
                           size_t count,
                           long timeout);


static int connection_write(struct connection *conn,
                            void *buf,
                            size_t count);

#if GSSAPI
#define GSSAPI_BUFFER   16384

static int gssapi_init_client(struct connection *conn,
                              char *host,
                              char *service);

static int gssapi_init_server(struct connection *conn);

static void gssapi_error(OM_uint32 status_major,
                         OM_uint32 status_minor,
                         const char *function);

static int gssapi_read(struct connection *conn,
                       gss_buffer_t token,
                       int timeout);

static int gssapi_write(struct connection *conn, gss_buffer_t token);
#endif


static void sig_debug(int sig)
{/*{{{*/
    if (max_level == LOG_DEBUG)
        log_level(old_level);
    else {
        old_level = max_level;
        log_level(LOG_DEBUG);
    }
}/*}}}*/


static void set_signal(void)
{/*{{{*/
    signal(SIGUSR1, sig_debug);
}/*}}}*/


void log_init(const char *name)
{/*{{{*/
    openlog(name, LOG_PID, LOG_DAEMON);
    set_signal();
}/*}}}*/


void log_level(int level)
{/*{{{*/
    if (max_level == level)
        return;

    if (max_level == LOG_DEBUG)
        log_msg(LOG_INFO, 0, "Debug messages disabled");
    else if (level == LOG_DEBUG)
        log_msg(LOG_INFO, 0, "Debug messages enabled");

    max_level = level;
}/*}}}*/


void log_stderr(void)
{/*{{{*/
    out_stderr = 1;
}/*}}}*/


void log_msg(int level, int err, char *fmt, ...)
{/*{{{*/
    char msg[256];
    char *error;
    va_list ap;

    if (level > max_level)
        return;

    msg[0] = '\0';

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (err)
        error = strerror(err);
    else
        error = "";

    if (out_stderr) {
        char timestr[50];
        time_t t;
        struct tm tm;

        t = time(NULL);
        if (localtime_r(&t, &tm) == &tm
            && strftime(timestr, sizeof(timestr), "%F %T", &tm) > 0) {
            fprintf(stderr, "%s %s%s%s\n",
                    timestr, msg, (err == 0) ? "" : ": ", error);
        }
        else
            fprintf(stderr, "%s%s%s\n", msg, (err == 0) ? "" : ": ", error);

        fflush(stderr);
    }
    else
        syslog(level, "%s%s%s", msg, (err == 0) ? "" : ": ", error);
}/*}}}*/


int system_command(const char *command)
{/*{{{*/
    int stat;
    pid_t pid;
    struct sigaction sa, saveint, savequit;
    sigset_t saveblock;
    int pipe_fds[2];

    if (command == NULL)
        return 1;

    if (pipe(pipe_fds) == -1) {
        log_msg(LOG_ERR, errno, "Cannot create pipe");
        return 1;
    }

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigemptyset(&saveint.sa_mask);
    sigemptyset(&savequit.sa_mask);
    sigaction(SIGINT, &sa, &saveint);
    sigaction(SIGQUIT, &sa, &savequit);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sa.sa_mask, &saveblock);

    if ((pid = fork()) == 0) {
        /* child */
        int fd;

        sigaction(SIGINT, &saveint, (struct sigaction *) NULL);
        sigaction(SIGQUIT, &savequit, (struct sigaction *) NULL);
        sigprocmask(SIG_SETMASK, &saveblock, (sigset_t *) NULL);

        /* close read end of the pipes and map the other end */
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);

        /* close all file descriptors except STDOUT_FILENO and STDERR_FILENO */
        for (fd = getdtablesize(); fd >= 0; fd--) {
            if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
                close(fd);
        }

        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        exit(127);
    }
    else if (pid < 0)
        stat = -1;
    else {
        /* parent */
        char line[LINE_SIZE];
        FILE *stream;

        close(pipe_fds[1]);

        if ((stream = fdopen(pipe_fds[0], "r")) != NULL) {
            while (fgets(line, LINE_SIZE, stream) ) {
                int len = strlen(line);
                if (line[len - 1] == '\n')
                    line[len - 1] = '\0';
                log_msg(LOG_NOTICE, 0, "Command[%lu]: %s",
                        pid, line);
            }
        }
        fclose(stream);
        close(pipe_fds[0]);

        while (waitpid(pid, &stat, 0) == -1) {
            if (errno != EINTR) {
                stat = -1;
                break;
            }
        }
    }

    sigaction(SIGINT, &saveint, (struct sigaction *) NULL);
    sigaction(SIGQUIT, &savequit, (struct sigaction *) NULL);
    sigprocmask(SIG_SETMASK, &saveblock, (sigset_t *) NULL);

    return stat;
}/*}}}*/


int read_tmout(int fd, void *buf, size_t count, long timeout)
{/*{{{*/
    struct timeval tv = { timeout , 0 };
    fd_set rdfs;
    int ret;
    int size;

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    if ((ret = select(fd + 1, &rdfs, NULL, NULL,
                      ((timeout > 0) ? &tv : NULL))) == -1) {
        log_msg(LOG_ERR, errno, "Cannot read data");
        return 0;
    }
    else if (ret == 0) {
        log_msg(LOG_ERR, 0, "No data within %d seconds", timeout);
        return 0;
    }

    if ((size = read(fd, buf, count)) <= 0) {
        if (size < 0)
            log_msg(LOG_ERR, errno, "Cannot read data");
        else
            log_msg(LOG_ERR, 0, "Cannot read data: Connection closed");
        return 0;
    }
    else
        return size;
}/*}}}*/


char *line_read(struct connection *conn,
                char *buf,
                int *pos,
                int *len,
                int size,
                long timeout)
{/*{{{*/
    char *p = NULL;
    int i;
    int r;

    if (*pos > 0) {
        memmove(buf, buf + *pos, *len);
        *pos = 0;
    }

    do {
        for (p = buf + *pos, i = *pos; *p != '\n' && i < *len; i++, p++)
            ;

        if (*p != '\n') {
            if (!(r = connection_read(conn, buf + *len, size - *len, timeout))) {
                *pos = 0;
                return NULL;
            }
            *pos += *len;
            *len += r;
        }
    } while (*p != '\n');

    *pos = i + 1;
    *len -= *pos;
    *p = '\0';

    return buf;
}/*}}}*/


int line_write(struct connection *conn, char *buf, long size)
{/*{{{*/
    return connection_write(conn, buf, size);
}/*}}}*/


int daemonize(const char *pidfile, sig_t terminate)
{/*{{{*/
    long pid, parent;
    int fd;
    int pidfd = -1;

    if (getppid() == 1) {
        log_msg(LOG_INFO, 0, "Already running as a daemon; PPID == 1");
        return 0;
    }

    /* open and lock PID file */
    if (pidfile != NULL) {
        if ((pidfd = open(pidfile, O_CREAT | O_RDWR | O_SYNC, 0640)) == -1) {
            log_msg(LOG_ERR, 0, "Cannot create PID file");
            return -1;
        }

        if (lockf(pidfd, F_TLOCK, 0)) {
            log_msg(LOG_ERR, 0, "Another instance is already running");
            return -1;
        }

        ftruncate(pidfd, 0);
    }

    parent = getpid();
    pid = fork();
    if (pid < 0)
        return -1;

    if (pid > 0) {
        /* write child's PID to the PID file */
        if (pidfd != -1) {
            char strpid[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

            snprintf(strpid, 10, "%ld\n", pid);
            if (write(pidfd, strpid, strlen(strpid)) != strlen(strpid)) {
                log_msg(LOG_ERR, 0, "Cannot write child's PID into %s",
                        pidfile);
                return -1;
            }
        }

        exit(0);
    }

    /* CHILD */

    /* create a new session (~ process group) */
    setsid();

    /* close all open filedescriptors except for PID file */
    for (fd = getdtablesize(); fd >= 0; fd--) {
        if (fd != pidfd)
            close(fd);
    }

    /* open /dev/null as stdin, stdout, and stderr */
    if ((fd = open("/dev/null", O_RDWR)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }

    umask(0027);
    chdir("/");

    /* check the PID file is ours */
    if (pidfd != -1) {
        FILE *file;

        /* wait until the PID file is unlocked */
        if (lockf(pidfd, F_LOCK, 0)) {
            log_msg(LOG_ERR, 0, "Cannot lock PID file");
            return -1;
        }

        if ((file = fdopen(pidfd, "r")) == NULL) {
            log_msg(LOG_ERR, 0, "Cannot open PID file");
            return -1;
        }
        rewind(file);

        if (fscanf(file, "%ld", &pid) != 1 || pid != getpid()) {
            log_msg(LOG_ERR, 0,
                    "Ooops, the PID files is for %ld and not for me (%ld)",
                    pid, getpid());
            return -1;
        }
    }

    if (terminate != NULL) {
        signal(SIGINT, terminate);
        signal(SIGTERM, terminate);
    }

    /* ignore some other signals */
    signal(SIGHUP, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    log_msg(LOG_INFO, 0, "Running in background as PID %d, child of %d",
            getpid(), parent);

    return 0;
}/*}}}*/


void terminate_handler(int sig)
{/*{{{*/
    log_msg(LOG_WARNING, 0, "Terminated by signal %d", sig);
    _exit(0);
}/*}}}*/


struct args *line_parse(char *line)
{/*{{{*/
    struct args *args;
    int count;
    int last_space;
    int i;
    char *p;

    if (line == NULL || *line == '\0')
        return NULL;

    count = 0;
    last_space = 1;
    for (p = line; *p != '\0'; p++) {
        if (*p == ' ') {
            if (!last_space)
                count++;
            last_space = 1;
        }
        else
            last_space = 0;
    }
    if (!last_space)
        count++;

    if (count == 0)
        return NULL;

    args = (struct args *)
                malloc(sizeof(struct args) + count * sizeof(char *));
    if (args == NULL)
        return NULL;

    args->argc = count;

    i = 0;
    last_space = 1;
    for (p = line; *p != '\0'; p++) {
        if (*p == ' ') {
            if (!last_space)
                *p = '\0';
            last_space = 1;
        }
        else if (last_space) {
            args->argv[i++] = p;
            last_space = 0;
        }
    }

    return args;
}/*}}}*/


void line_parse_undo(struct args *args)
{/*{{{*/
    int i;

    if (args == NULL)
        return;

    for (i = 0; i < args->argc - 1; i++)
        args->argv[i][strlen(args->argv[i])] = ' ';

    args->argc = 1;
}/*}}}*/


int writeall(int fd, const char *buf, long size)
{/*{{{*/
    long w;
    long rest = size;

    while (rest > 0) {
        if ((w = write(fd, buf, rest)) < 0)
            return -1;

        rest -= w;
        buf += w;
    }

    return 0;
}/*}}}*/


static int connection_read(struct connection *conn,
                           void *buf,
                           size_t count,
                           long timeout)
{/*{{{*/
    int len = 0;

    if (!conn->gss)
        len = read_tmout(conn->sock, buf, count, timeout);
#if GSSAPI
    else {
        OM_uint32 major, minor;
        gss_buffer_desc input;
        gss_buffer_desc output;
        char *buffer[GSSAPI_BUFFER];

        input.length = GSSAPI_BUFFER;
        input.value = buffer;
        if (gssapi_read(conn, &input, timeout))
            return 0;

        major = gss_unwrap(&minor, conn->context, &input, &output, NULL, NULL);
        if (GSS_ERROR(major)) {
            gssapi_error(major, minor, "gss_unwrap");
            return 0;
        }

        if (output.length > count) {
            log_msg(LOG_ERR, 0, "GSS-API: input data too large: %d",
                    output.length);
            gss_release_buffer(&minor, &output);
            return 0;
        }

        len = output.length;
        memcpy(buf, output.value, len);
        gss_release_buffer(&minor, &output);
    }
#endif

    return len;
}/*}}}*/


static int connection_write(struct connection *conn,
                            void *buf,
                            size_t count)
{/*{{{*/
    if (!conn->gss)
        return writeall(conn->sock, buf, count);
#if GSSAPI
    else {
        OM_uint32 major, minor;
        gss_buffer_desc input;
        gss_buffer_desc output;
        int ret;

        input.length = count;
        input.value = buf;

        major = gss_wrap(&minor, conn->context, 1, GSS_C_QOP_DEFAULT,
                         &input, NULL, &output);
        if (GSS_ERROR(major)) {
            gssapi_error(major, minor, "gss_wrap");
            return -1;
        }

        ret = gssapi_write(conn, &output);
        gss_release_buffer(&minor, &output);

        return ret;
    }
#endif

    return -1;
}/*}}}*/


#if GSSAPI
static int gssapi_init_client(struct connection *conn,
                              char *host,
                              char *service)
{/*{{{*/
    gss_name_t server;
    gss_buffer_desc name;
    OM_uint32 major, minor;
    gss_buffer_desc input;
    gss_buffer_desc output;
    int done = 0;
    char buf[GSSAPI_BUFFER];

    name.length = strlen(service) + 1 + strlen(host);
    name.value = malloc(name.length + 1);
    if (name.value == NULL) {
        log_msg(LOG_ERR, errno, "GSS-API: initialization failed");
        return -1;
    }

    sprintf(name.value, "%s@%s", service, host);
    major = gss_import_name(&minor, &name, GSS_C_NT_HOSTBASED_SERVICE,
                            &server);
    conn->server = name.value;

    if (GSS_ERROR(major)) {
        gssapi_error(major, minor, "gss_import_name");
        return -1;
    }

    memset(&input, 0, sizeof(gss_buffer_desc));
    memset(&output, 0, sizeof(gss_buffer_desc));

    while (!done) {
        major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL,
                    &conn->context, server, GSS_C_NO_OID,
                    GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG
                    | GSS_C_SEQUENCE_FLAG | GSS_C_INTEG_FLAG, 0,
                    GSS_C_NO_CHANNEL_BINDINGS, &input, NULL, &output,
                    NULL, NULL);
        if (GSS_ERROR(major)) {
            gssapi_error(major, minor, "gss_init_sec_context");
            goto err;
        }

        if (output.length != 0) {
            int ret = gssapi_write(conn, &output);
            gss_release_buffer(&minor, &output);
            if (ret)
                goto err;
        }

        if (major & GSS_S_CONTINUE_NEEDED) {
            input.length = GSSAPI_BUFFER;
            input.value = buf;
            if (gssapi_read(conn, &input, 0))
                goto err;
        }
        else
            done = 1;
    }

    gss_release_name(&minor, &server);

    conn->gss = 1;

    return 0;

err:
    gss_release_name(&minor, &server);

    if (conn->context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &conn->context, GSS_C_NO_BUFFER);

    return -1;
}/*}}}*/


static int gssapi_init_server(struct connection *conn)
{/*{{{*/
    OM_uint32 major, minor;
    gss_buffer_desc input;
    gss_buffer_desc output;
    gss_name_t client = GSS_C_NO_NAME;
    OM_uint32 flags, req_flags;
    char buf[GSSAPI_BUFFER];

    do {
        input.length = GSSAPI_BUFFER;
        input.value = buf;
        if (gssapi_read(conn, &input, 0))
            return -1;

        major = gss_accept_sec_context(&minor, &conn->context,
                    GSS_C_NO_CREDENTIAL, &input, NULL, &client, NULL, &output,
                    &flags, NULL, NULL);
        if (GSS_ERROR(major)) {
            gssapi_error(major, minor, "gss_accept_sec_context");
            goto err;
        }

        if (output.length != 0) {
            int ret = gssapi_write(conn, &output);
            gss_release_buffer(&minor, &output);
            if (ret)
                goto err;
        }

        req_flags = GSS_C_REPLAY_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG;
        if ((flags & req_flags) != req_flags
            || (flags & GSS_C_ANON_FLAG)) {
            log_msg(LOG_ERR, 0, "GSS-API: client requested incompatible flags");
            goto err;
        }
    } while (major & GSS_S_CONTINUE_NEEDED);

    major = gss_display_name(&minor, client, &output, NULL);
    if (GSS_ERROR(major)) {
        gssapi_error(major, minor, "gss_display_name");
        goto err;
    }

    if (output.length == 0
        || (conn->client = malloc(output.length + 1)) == NULL) {
        gss_release_buffer(&minor, &output);
        log_msg(LOG_ERR, errno, "GSS-API: cannot decode client's name");
        goto err;
    }

    gss_release_name(&minor, &client);
    client = GSS_C_NO_NAME;
    memcpy(conn->client, output.value, output.length);
    conn->client[output.length] = '\0';
    gss_release_buffer(&minor, &output);

    conn->gss = 1;

    return 0;

err:
    if (conn->context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &conn->context, GSS_C_NO_BUFFER);

    if (client != GSS_C_NO_NAME)
        gss_release_name(&minor, &client);

    return -1;
}/*}}}*/


static void gssapi_error(OM_uint32 status_major,
                         OM_uint32 status_minor,
                         const char *function)
{/*{{{*/
    OM_uint32 major, minor;
    OM_uint32 context = 0;
    gss_buffer_desc string;
    const char *error;
    OM_uint32 status;
    int details;

    if ((status = GSS_CALLING_ERROR(status_major))) {
#define CE "calling error: "
        switch (status) {
        case GSS_S_CALL_INACCESSIBLE_READ:
            error = CE "inaccessible read parameter";
            break;
        case GSS_S_CALL_INACCESSIBLE_WRITE:
            error = CE "inaccessible write parameter";
            break;
        case GSS_S_CALL_BAD_STRUCTURE:
            error = CE "malformed parameter";
            break;
        default:
            error = CE "unknown error";
        }
#undef CE
    }
    else if ((status = GSS_ROUTINE_ERROR(status_major))) {
        switch (status) {
        case GSS_S_BAD_MECH:
            error = "unsupported mechanism";
            break;
        case GSS_S_BAD_NAME:
            error = "invalid name";
            break;
        case GSS_S_BAD_NAMETYPE:
            error = "unsupported name type";
            break;
        case GSS_S_BAD_BINDINGS:
            error = "incorrect channel bindings";
            break;
        case GSS_S_BAD_STATUS:
            error = "invalid status code";
            break;
        case GSS_S_BAD_MIC:
            error = "invalid token MIC";
            break;
        case GSS_S_NO_CRED:
            error = "no credentials";
            break;
        case GSS_S_NO_CONTEXT:
            error = "no context";
            break;
        case GSS_S_DEFECTIVE_TOKEN:
            error = "invalid token";
            break;
        case GSS_S_DEFECTIVE_CREDENTIAL:
            error = "invalid credential";
            break;
        case GSS_S_CREDENTIALS_EXPIRED:
            error = "expired credentials";
            break;
        case GSS_S_CONTEXT_EXPIRED:
            error = "expired context";
            break;
        case GSS_S_FAILURE:
            error = "general failure";
            break;
        case GSS_S_BAD_QOP:
            error = "unsupported quality of protection";
            break;
        case GSS_S_UNAUTHORIZED:
            error = "forbidden by local security policy";
            break;
        case GSS_S_UNAVAILABLE:
            error = "unavailable option or operation";
            break;
        case GSS_S_DUPLICATE_ELEMENT:
            error = "credential element already exists";
            break;
        case GSS_S_NAME_NOT_MN:
            error = "name is not a mechanism name";
            break;
        default:
            error = "unknown runtime error";
        }
    }
    else
        error = "unknown error";

    details = status_minor != 0
              || (GSS_SUPPLEMENTARY_INFO(status_major)
                  & (GSS_S_DUPLICATE_TOKEN | GSS_S_OLD_TOKEN
                     | GSS_S_UNSEQ_TOKEN | GSS_S_GAP_TOKEN));

    log_msg(LOG_ERR, 0, "GSS-API: %s() failed: %s%s",
            function, error,
            (details != 0) ? "; details follow" : "");

    if ((status = GSS_SUPPLEMENTARY_INFO(status_major))
        & (GSS_S_DUPLICATE_TOKEN | GSS_S_OLD_TOKEN
           | GSS_S_UNSEQ_TOKEN | GSS_S_GAP_TOKEN)) {
        switch (status) {
        case GSS_S_DUPLICATE_TOKEN:
            error = "duplicate token";
            break;
        case GSS_S_OLD_TOKEN:
            error = "old token";
            break;
        case GSS_S_UNSEQ_TOKEN:
            error = "future token";
            break;
        case GSS_S_GAP_TOKEN:
            error = "missing token";
            break;
        default:
            error = "";
        }

        log_msg(LOG_ERR, 0, "GSS-API: %s", error);
    }

    if (status_minor != 0) {
        do {
            major = gss_display_status(&minor, status_minor, GSS_C_MECH_CODE,
                                       GSS_C_NO_OID, &context, &string);
            log_msg(LOG_ERR, 0, "GSS-API: %.*s",
                    string.length, (char *) string.value);
            gss_release_buffer(&minor, &string);
        } while (!GSS_ERROR(major) && context != 0);
    }
}/*}}}*/


static int gssapi_read(struct connection *conn,
                       gss_buffer_t token,
                       int timeout)
{/*{{{*/
    uint32_t len;

    if (read_tmout(conn->sock, (void *) &len, 4, timeout) != 4) {
        log_msg(LOG_ERR, 0, "GSS-API: cannot read data");
        return -1;
    }

    len = ntohl(len);
    if (len > token->length) {
        log_msg(LOG_ERR, 0,
                "GSS-API: input data too large; "
                "perhaps the client does not use GSS");
        return -1;
    }

    if (read_tmout(conn->sock, token->value, len, timeout) != len) {
        log_msg(LOG_ERR, 0, "GSS-API: cannot read data");
        return -1;
    }

    token->length = len;

    return 0;
}/*}}}*/


static int gssapi_write(struct connection *conn, gss_buffer_t token)
{/*{{{*/
    uint32_t len = htonl(token->length);

    return (writeall(conn->sock, (char *) &len, 4)
            || writeall(conn->sock, (char *) token->value, token->length));
}/*}}}*/
#endif


int connection_init(struct connection *conn, int fd)
{/*{{{*/
    if (fd < 0)
        return -1;

    conn->sock = fd;
    conn->gss = 0;
#if GSSAPI
    conn->context = GSS_C_NO_CONTEXT;
    conn->client = NULL;
    conn->server = NULL;
#endif

    return 0;
}/*}}}*/


int connection_gss(struct connection *conn,
                   enum conn_role role,
                   char *host,
                   char *service)
{/*{{{*/
#if GSSAPI
    switch (role) {
    case CONN_CLIENT:
        return gssapi_init_client(conn, host, service);

    case CONN_SERVER:
        return gssapi_init_server(conn);
    }

    return -1;

#else
    log_msg(LOG_WARNING, 0, "GSS-API support was disabled");
    return 0;
#endif
}/*}}}*/


void connection_close(struct connection *conn)
{/*{{{*/
#ifndef NET_NO_IO
    char *peer;

    if ((peer = net_peer(conn)) != NULL)
        log_msg(LOG_DEBUG, 0, "Connection to %s closed", peer);
    free(peer);
#endif

    close(conn->sock);

#if GSSAPI
    if (conn->gss) {
        OM_uint32 minor;

        if (conn->context != GSS_C_NO_CONTEXT)
            gss_delete_sec_context(&minor, &conn->context, GSS_C_NO_BUFFER);
        free(conn->client);
        free(conn->server);
    }
#endif

    memset(conn, '\0', sizeof(struct connection));
}/*}}}*/

