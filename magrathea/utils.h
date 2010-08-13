/* $Id: utils.h,v 1.16 2008/10/01 14:48:20 xdenemar Exp $ */

/** @file
 * Utility declarations for all magrathea components.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.16 $ $Date: 2008/10/01 14:48:20 $
 */

#ifndef UTILS_H
#define UTILS_H

#include <syslog.h>
#include <sys/types.h>
#include <signal.h>
#if GSSAPI
# include <gssapi.h>
#endif


/** Number of seconds we are willing to wait for incomming data. */
#define READ_TIMEOUT        30


/** Maximum size of read buffer. */
#define MAX_BUF             8192


struct connection {
    int sock;
    int gss;
# if GSSAPI
    gss_ctx_id_t context;
    char *client;
    char *server;
# endif
};


enum conn_role {
    CONN_CLIENT,
    CONN_SERVER
};


extern void log_init(const char *name);
extern void log_stderr(void);
extern void log_level(int level);
#define log_debug(on)   log_level((on) ? LOG_DEBUG : LOG_INFO)
extern void log_msg(int level, int err, char *fmt, ...);


/** Execute a shell command.
 * This function is inspired by system() function from glibc and system(3P).
 * It is extended to log command's output using log_msg().
 *
 * @param[in] command
 *      command to be executed using /bin/sh -c.
 *
 * @return
 *      see system(3).
 */
extern int system_command(const char *command);


/** Read data with timeout.
 * The function waits at most @c timeout seconds for data to come.
 *
 * @param[in] fd
 *      file descriptor to read data from.
 *
 * @param[out] buf
 *      buffer for data.
 *
 * @param[in] count
 *      maximum number of characters to read.
 *
 * @param[in] timeout
 *      number of seconds to wait for. Zero means wait forever.
 *
 * @return
 *      number of characters read from fd.
 */
extern int read_tmout(int fd, void *buf, size_t count, long timeout);


/** Return the first line from a buffer.
 * In case there is not enough data in the buffer, read_tmout() is used to
 * read more data.
 *
 * @param[in] conn
 *      pointer to a connection to read data from when there is not enough
 *      data in the buffer.
 *
 * @param buf
 *      data buffer. Its content may be changed after the function returns.
 *
 * @param pos
 *      current position in the buffer. The value will point to the begining
 *      of next line.
 *
 * @param len
 *      number of characters in the buffer.
 *
 * @param[in] size
 *      mazimum size of the buffer.
 *
 * @param[in] timeout
 *      number of seconds to wait for. Zero means wait forever.
 *
 * @return
 *      pointer to the first line in the buffer. The pointer points into the
 *      buffer and shall never be free()d or considered valid after another
 *      line_read() call.
 */
extern char *line_read(struct connection *conn,
                       char *buf,
                       int *pos,
                       int *len,
                       int size,
                       long timeout);


/** Write a line to a connection.
 * Despite its name, however, the function does not add a new line at the end
 * of the buffer.
 *
 * @param[in] conn
 *      connection to write data to.
 *
 * @param[in] buf
 *      data buffer.
 *
 * @param[in] size
 *      number of characters in the buffer.
 *
 * @return
 *      zero on success, nonzero otherwise.
 */
extern int line_write(struct connection *conn, char *buf, long size);


/** Detach current process from terminal and run it in background.
 *
 * @param pidfile
 *      file name to write daemon's PID into. PID will not be written
 *      anywhere if pidfile is @c NULL.
 *
 * @param terminate
 *      handler for SIGINT and SIGTERM signals.
 *
 * @return
 *      zero on success, nonzero otherwise (details in errno).
 */
extern int daemonize(const char *pidfile, sig_t terminate);


/** Simple signal handler for daemonize().
 * The function just logs the process is beeing terminated and exits it.
 *
 * @param sig
 *      signal.
 *
 * @return
 *      nothing.
 */
void terminate_handler(int sig);


struct args {
    int argc;
    char *argv[];
};

extern struct args *line_parse(char *line);

extern void line_parse_undo(struct args *args);


extern int writeall(int fd, const char *buf, long size);


extern int connection_init(struct connection *conn, int fd);

extern void connection_close(struct connection *conn);

extern int connection_gss(struct connection *conn,
                          enum conn_role role,
                          char *host,
                          char *service);
#endif

