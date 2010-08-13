/* $Id: paths.h,v 1.2 2008/05/21 15:56:16 xdenemar Exp $ */

/** @file
 * Default paths for magrathea master daemon.
 * @author Jiri Denemark
 * @date 2007
 * @version $Revision: 1.2 $ $Date: 2008/05/21 15:56:16 $
 */

#ifndef PATHS_H
#define PATHS_H

/** Default configuration file. */
#define CONFIG_FILE         CONF_DIR "/master"


/** Default PID file. */
#define PID_FILE            "/var/run/magrathea-master.pid"


/** Domain switch callback.
 *
 * The callback is executed as follows
 *  <tt>"switch" domain1 domain2 ...</tt>
 * where each domain is described by its ID, state and a number of CPUs
 * assigned to it:
 *  <tt>domain_id:state:CPUs</tt>
 * where @c state is one of the following:
 *  @li @c f   free
 *  @li @c o   occupied or occupied-would-preempt
 *  @li @c r   running or running-preemptible
 *  @li @c p   preempted
 * If CPU counting is disabled, the value of CPUs is undefined.
 */
#define CALLBACK_SWITCH     CONF_DIR "/switch"


/** CPU switch callback.
 *
 * The callback is executed in the same way as the domain switch callback above.
 */
#define CALLBACK_SWITCH_CPU CONF_DIR "/switch-cpu"


/** Default administrative socket. */
#define ADMIN_SOCKET        RUN_DIR "/master.sock"


/** Basename for a persistent state storage file. */
#define STORAGE_FILE        RUN_DIR "/master.state"


#endif

