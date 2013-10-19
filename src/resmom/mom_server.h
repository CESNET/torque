#ifndef MOM_SERVER_H_
#define MOM_SERVER_H_

#include "pbs_ifl.h"
#include "mcom.h"
#include <time.h>

typedef struct mom_server
  {
  int             SStream;                  /* streams to pbs_server daemons */
  char            pbs_servername[PBS_MAXSERVERNAME + 1];
  time_t          next_connect_time;
  int             connect_failure_count;
  int             sent_hello_count;
  char            ReportMomState;
  time_t          MOMLastSendToServerTime;
  time_t          MOMLastRecvFromServerTime;
  char            MOMLastRecvFromServerCmd[MMAX_LINE];
  int             received_hello_count;
  int             received_cluster_address_count;
  char            MOMSendStatFailure[MMAX_LINE];
  } mom_server;

#endif /* MOM_SERVER_H_ */
