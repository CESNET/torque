#ifndef UTILITY_H_
#define UTILITY_H_

#include <string>

#ifndef NULL
#define NULL ((void*)0L)
#endif

#define retnull_on_null(X) do { if ((X) == NULL) { return NULL; } } while(0)

/** Verify that the provided FQDN does indeed match a Torque server */
bool verify_fqdn(const std::string& fqdn);

/** Get a FQDN of torque server from a domain name */
std::string get_fqdn(const std::string& host);

/** Get local machine FQDN */
std::string get_local_fqdn();

/** Get errno text as string */
std::string get_errno_string();

/** Global handler for out of memory situations */
void global_oom_handle();

#endif /* UTILITY_H_ */
