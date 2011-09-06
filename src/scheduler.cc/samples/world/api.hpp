#ifndef API_HPP_
#define API_HPP_

/* api.h modified for usage in C++ code */

extern "C" {
#include "api.h"
}

inline char *xpbs_cache_get_local(const char *hostname, const char *name)
{ return pbs_cache_get_local((char*)hostname,(char*)name); }

inline int xcache_hash_fill_local(const char *metric, void *hash)
{ return cache_hash_fill_local((char*)metric, hash); }

inline char *xcache_hash_find(void *hash,const char *key)
{ return cache_hash_find(hash,(char*)key); }

inline int xcache_store_local(const char *hostname, const char *name, const char *value)
{ return cache_store_local((char*)hostname,(char*)name, (char*)value); }

#endif /* API_HPP_ */
