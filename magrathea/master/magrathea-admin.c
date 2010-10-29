/** $Id: magrathea-admin.c,v 1.7 2008/09/11 09:27:04 xdenemar Exp $ */

/** @file
 * Administration tool for magrathea-master daemon.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.7 $ $Date: 2008/09/11 09:27:04 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>
#include <signal.h>

#include "../utils.h"
#include "../net.h"
#include "../cfg.h"
#include "paths.h"


static void usage(char *me)
{
    fprintf(stderr,
    "Usage:   %s [options] <command> [<FQDN> arguments...]\n"
    "Options:   -c config_file\n"
    "           -s socket\n"
    "           -t timeout\n",
    me);
}


static int master_connect(const char *socket_name)
{
    struct sockaddr_un addr;
    int sock;

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_name, UNIX_PATH_MAX);
    addr.sun_path[UNIX_PATH_MAX - 1] = '\0';

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0
        || connect(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        perror("Cannot open socket");
        return -1;
    }

    return sock;
}


static void sig_alarm(int sig)
{
    log_msg(LOG_ERR, 0, "Timeout");
    exit(1);
}


int main(int argc, char **argv)
{
    struct connection conn;
    int i;
    int pos;
    int len;
    char buf[MAX_BUF];
    struct cfg_file *cfg = NULL;
    const char *sock_name = NULL;
    int opt;
    char *config_file = CONFIG_FILE;
    char *line;
    unsigned int timeout = 0;

    log_init("magrathea-admin");
    log_stderr();
    log_level(LOG_ERR);

    while ((opt = getopt(argc, argv, "c:s:t:")) != -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;
        case 's':
            sock_name = optarg;
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        }
    }

    if (argc < optind + 1) {
        usage(argv[0]);
        return 2;
    }

    if (sock_name == NULL) {
        if ((cfg = cfg_parse(config_file)) == NULL) {
            fprintf(stderr, "Cannot parse configuration file\n");
            return 2;
        }

        sock_name = cfg_get_string(cfg, CFG_FIRST,
                                   "admin-socket", ADMIN_SOCKET);
    }

    if (timeout > 0) {
        signal(SIGALRM, sig_alarm);
        alarm(timeout);
    }

    if (connection_init(&conn, master_connect(sock_name))) {
        fprintf(stderr, "Cannot connect to magrathea-master daemon\n");
        return 2;
    }

    memset(buf, '\0', MAX_BUF);
    pos = 0;

    for (i = optind; i < argc; i++) {
        len = snprintf(buf + pos, MAX_BUF - pos, "%s ", argv[i]);
        if (len + pos > MAX_BUF)
            break;
        pos += len;
    }
    buf[pos - 1] = '\n';

    if (line_write(&conn, buf, pos)) {
        perror("Cannot send request to magrathea-master daemon");
        return 2;
    }

    len = 0;
    pos = 0;
    buf[0] = '\0';
    if ((line = line_read(&conn, buf, &pos, &len, MAX_BUF, 0)) == NULL)
        return 2;

    if (strncmp(line, "failed: ", 8) == 0) {
        printf("%s\n", line + 8);
        return 1;
    }
    else {
        if (strcmp(line, "ok LIST") == 0) {
            while ((line = line_read(&conn, buf, &pos, &len, MAX_BUF, 0))
                    != NULL) {
                if (strcmp(line, "/LIST") == 0)
                    break;
                else
                    printf("%s\n", line);
            }
        }
        else if (strcmp(line, "ok") != 0)
            printf("%s\n", line);

        return 0;
    }
}

