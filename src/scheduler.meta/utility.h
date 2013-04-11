#ifndef UTILITY_H_
#define UTILITY_H_

#include <string>

#ifndef NULL
#define NULL ((void*)0L)
#endif

#define retnull_on_null(X) do { if ((X) == NULL) { return NULL; } } while(0)

/** Get errno text as string */
std::string get_errno_string();

/** Global handler for out of memory situations */
void global_oom_handle();

#endif /* UTILITY_H_ */
