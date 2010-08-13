/** $Id: magrathea-cmd.c,v 1.10 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * Command line tool for magrathea slave daemon.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.10 $ $Date: 2009/06/04 11:12:30 $
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../utils.h"
#include "../net.h"
#include "magrathea-slave.h"
#include "versions-magrathea-cmd.h"


void usage(char *me)
{
    enum command cmd;

    fprintf(stderr, "Usage: %s [-v] <command> [arguments...]\n", me);

    fprintf(stderr, "Commands:");
    for (cmd = CMD_JOB_START; cmd < CMD_RESPONSE; cmd++)
        fprintf(stderr, " %s", command_string[cmd]);

    fprintf(stderr, "\n");
}


int slave_connect(void)
{
    struct sockaddr_un addr;
    int sock;

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SLAVE_SOCKET, UNIX_PATH_MAX);
    addr.sun_path[UNIX_PATH_MAX - 1] = '\0';

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0
        || connect(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        perror("Cannot open socket");
        return -1;
    }

    return sock;
}


int main(int argc, char **argv)
{
    enum command cmd;
    struct message msg;
    struct msghdr hdr;
    struct iovec iov;
    int fd;
    int i;
    int pos;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp("-v", argv[1]) == 0) {
        printf("magrathea-cmd CLI to magrathea-slave daemon\n%s", VERSIONS);
        return 0;
    }

    for (cmd = CMD_JOB_START; cmd < CMD_RESPONSE; cmd++) {
        if (strcmp(command_string[cmd], argv[1]) == 0)
            break;
    }

    if (CMD_RESPONSE == cmd) {
        usage(argv[0]);
        return 1;
    }

    if ((fd = slave_connect()) == -1) {
        fprintf(stderr, "Cannot connect to magrathea-slave daemon\n");
        return 1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.cmd = cmd;
    pos = 0;
    for (i = 2; i < argc && pos < MAX_DETAILS; i++) {
        snprintf(msg.details + pos, MAX_DETAILS - pos, "%s ", argv[i]);
        pos += strlen(argv[i]) + 1;
    }
    msg.details[MAX_DETAILS - 1] = '\0';

    memset(&hdr, 0, sizeof(hdr));
    iov.iov_base = &msg;
    iov.iov_len = sizeof(msg);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;

    if (sendmsg(fd, &hdr, 0) != sizeof(msg)) {
        perror("Cannot send request to magrathea-slave daemon");
        return 1;
    }

    if (recvmsg(fd, &hdr, 0) != sizeof(msg)) {
        perror("Cannot read response from magrathea-slave daemon");
        return 1;
    }

    if (CMD_RESPONSE == msg.cmd) {
        fprintf(stderr, "Invalid response received from magrathea-slave daemon\n");
        return 1;
    }

    if (msg.ok) {
        if ('\0' != msg.details[0]) {
            if (strncmp(msg.details, "warning:", 8) == 0)
                fprintf(stderr, "%s\n", msg.details);
            else if (strcmp(msg.details, "ok") != 0)
                fprintf(stdout, "%s\n", msg.details);
        }
        return 0;
    }
    else {
        if ('\0' != msg.details[0])
            fprintf(stderr, "%s\n", msg.details);
        return 1;
    }
}

