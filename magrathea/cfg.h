/* $Id: cfg.h,v 1.1 2008/02/15 13:29:46 xdenemar Exp $ */

/** @file
 * General configuration file parsing for Magrathea.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.1 $ $Date: 2008/02/15 13:29:46 $
 */

#ifndef CFG_H
#define CFG_H

#include "utils.h"


/** Options from a configuration file. */
struct cfg_file {
    /** Starting position for CFG_NEXT search. */
    int cursor;
    /** Number of options. */
    int count;
    /** List of options. */
    struct cfg_option *options;
};


/** Configuration option. */
struct cfg_option {
    /** Option name. */
    char *name;
    /** Option value. */
    char *value;
};


/** Search type. */
enum cfg_search {
    CFG_FIRST,
    CFG_NEXT,
    CFG_LAST
};


/** Parse a configuration file.
 *
 * @param filename
 *      name of the configuration file.
 *
 * @return
 *      dynamically allocated structure containing a list of options and their
 *      values. NULL on error.
 */
extern struct cfg_file *cfg_parse(const char *filename);


/** Free memory occppied by cfg_parse().
 *
 * @param cfg
 *      the value returned by cfg_parse().
 *
 * @return
 *      nothing.
 */
extern void cfg_free(struct cfg_file *cfg);


/** Reset position for CFG_NEXT search.
 *
 * @param cfg
 *      the value returned by cfg_parse().
 *
 * @return
 *      nothing.
 */
extern void cfg_reset(struct cfg_file *cfg);


/** Find value of a given option.
 *
 * @param cfg
 *      the value returned by cfg_parse().
 *
 * @param search
 *      type of search.
 *
 * @param name
 *      option name.
 *
 * @param default_value
 *      this value is returned when no option with a given name is found.
 *
 * @return
 *      option value.
 */
extern const char *cfg_get_string(struct cfg_file *cfg,
                                  enum cfg_search search,
                                  const char *name,
                                  const char *default_value);


/** Find value of a given option and interpret it as a long integer.
 * @sa cfg_get_string()
 */
extern long cfg_get_long(struct cfg_file *cfg,
                         enum cfg_search search,
                         const char *name,
                         long default_value);


/** Find value of a given option and interpret it as a space-separated list.
 * @sa cfg_get_string()
 *
 * The result should be freed using cfg_free_list();
 *
 * @return
 *      NULL on error or when no such option was found.
 */
extern struct args *cfg_get_list(struct cfg_file *cfg,
                                 enum cfg_search search,
                                 const char *name);


/** Free list returned by cfg_get_list().
 *
 * @param args
 *      list of words returned by cfg_get_list().
 *
 * @return
 *      nothing.
 */
void cfg_free_list(struct args *args);

#endif

