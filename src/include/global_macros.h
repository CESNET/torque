/* Copyright (c) 2011 Mgr. Simon Toth (CESNET)
 *
 * Licensed under MIT License
 * http://www.opensource.org/licenses/mit-license.php
 */

#ifndef GLOBAL_MACROS_H_
#define GLOBAL_MACROS_H_

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#elif defined(__cplusplus)
# define UNUSED(x)
#else
# define UNUSED(x) x
#endif

#endif /* GLOBAL_MACROS_H_ */
