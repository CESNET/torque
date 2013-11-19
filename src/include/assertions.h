#include "log.h"
#include "pbs_error.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if 0

#ifndef ASSERTIONS_H_
#define ASSERTIONS_H_

#ifndef NDEBUG



/* TODO provide better solution, this is ugly */
static char whereami[1024];

#define dbg_precondition(expr, comment) \
  { if (!(expr)) { \
  sprintf(whereami,"%s() %s:%d",__func__, __FILE__, __LINE__);\
  sprintf(log_buffer,"%s is false -> \"%s\"",#expr,comment);\
  log_err(PBSE_PRECONDITION,whereami,log_buffer); abort(); } }

#define dbg_postcondition(expr, comment) \
  { if (!(expr)) { \
  sprintf(whereami,"%s() %s:%d",__func__, __FILE__, __LINE__);\
  sprintf(log_buffer,"%s is false -> \"%s\"",#expr,comment);\
  log_err(PBSE_POSTCONDITION,whereami,log_buffer); abort(); } }

#define dbg_consistency(expr, comment) \
  { if (!(expr)) { \
  sprintf(whereami,"%s() %s:%d",__func__, __FILE__, __LINE__);\
  sprintf(log_buffer,"%s is false -> \"%s\"",#expr,comment);\
  log_err(PBSE_CONSISTENCY,whereami,log_buffer); abort(); } }

#else

#define dbg_precondition(expr, comment) {}
#define dbg_postcondition(expr, comment) {}
#define dbg_consistency(expr, comment) {}

#endif

#endif

#endif
