#ifndef UTILITY_H_
#define UTILITY_H_

#ifndef NULL
#define NULL ((void*)0L)
#endif

#define retnull_on_null(X) do { if ((X) == NULL) { return NULL; } } while(0)

#endif /* UTILITY_H_ */
