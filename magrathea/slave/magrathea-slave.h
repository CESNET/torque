/* $Id: magrathea-slave.h,v 1.9 2009/06/04 11:12:30 ruda Exp $ */

/** @file
 * Common declarations for magrathea slave daemon and command line tool.
 * @author Jiri Denemark
 * @date 2006
 * @version $Revision: 1.9 $ $Date: 2009/06/04 11:12:30 $
 */

#ifndef MAGRATHEA_SLAVE_H
#define MAGRATHEA_SLAVE_H

#include "../utils.h"


/** Socket name. */
#define SLAVE_SOCKET  RUN_DIR "/slave.socket"


/** Commands sent between daemon and command line tool. */
enum command {
    /** Job start request. */
    CMD_JOB_START,
    /** Preemptible job start request. */
    CMD_JOB_START_PREEMPTIBLE,
    /** Stop a job. */
    CMD_JOB_STOP,
    /** Stop all jobs. */
    CMD_JOB_STOP_ALL,
    /** Cluster start request. */
    CMD_CLUSTER_START,
    /** Stop a cluster. */
    CMD_CLUSTER_STOP,
    /** Domain status request. */
    CMD_STATUS,
    /** Request for preempted time. */
    CMD_PREEMPTED,
    /** Request for freezing the VM. */
    CMD_FREEZE,
    /** Request for rebooting the VM. */
    CMD_REBOOT,
    /** End of list of commands.
     * This is not a real command. It is used to indicate response message. */
    CMD_RESPONSE
};

/** String representation of commands. */
static const char *command_string[] = {
    "startjob",
    "startjob-preemptible",
    "stopjob",
    "stopalljobs",
    "startcluster",
    "stopcluster",
    "status",
    "preempted",
    "freeze",
    "reboot",
    NULL
};


/** Maximum length of message details (including '\0'). */
#define MAX_DETAILS     MAX_BUF

/** Message (either request or response to it). */
struct message {
    /** Request command or CMD_RESULT. */
    enum command cmd;
    /** Result in case message::cmd == CMD_RESULT. */
    uint8_t ok;
    /** Message details. */
    char details[MAX_DETAILS];
} __attribute__((packed));

#endif

