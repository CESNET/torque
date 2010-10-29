/* $Id: cfg.c,v 1.2 2008/05/21 15:56:16 xdenemar Exp $ */

/** @file
 * General configuration file parsing for Magrathea.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.2 $ $Date: 2008/05/21 15:56:16 $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <regex.h>

#include "utils.h"
#include "cfg.h"

/** Regular expression matching a comment or an empty line. */
#define RE_COMMENT  \
    "^[[:space:]]*(#.*)?$"

/** Regular expression matching a line with option name and its value. */
#define RE_OPTION   \
    "^[[:space:]]*([[:alpha:]][-_[:alnum:]]*)[[:space:]]*=[[:space:]]?(.*)$"


struct cfg_file *cfg_parse(const char *filename)
{/*{{{*/
    FILE *file;
    char line[1024];
    int line_number = 0;
    int err;
    regex_t re_option;
    regex_t re_comment;
    regmatch_t matches[3];
    struct cfg_file *cfg;
    struct cfg_option opt;

    log_msg(LOG_INFO, 0, "Reading configuration from %s", filename);

    if ((file = fopen(filename, "r")) == NULL) {
        log_msg(LOG_ERR, errno,
                "Cannot parse configuration file %s", filename);
        return NULL;
    }

    err = regcomp(&re_option, RE_OPTION, REG_EXTENDED);

    if (!err) {
        err = regcomp(&re_comment, RE_COMMENT, REG_EXTENDED | REG_NOSUB);
        if (err) {
            regerror(err, &re_comment, line, 1023);
            regfree(&re_option);
        }
    }
    else
        regerror(err, &re_option, line, 1023);

    if (err) {
        log_msg(LOG_ERR, 0,
                "Cannot compile regular expression "
                "for parsing configuration file: %s", line);
        fclose(file);
        return NULL;
    }

    if ((cfg = malloc(sizeof(struct cfg_file))) == NULL) {
        log_msg(LOG_ERR, errno,
                "Cannot parse configuration file %s", filename);
        fclose(file);
        regfree(&re_option);
        regfree(&re_comment);
        return NULL;
    }

    cfg->cursor = 0;
    cfg->count = 0;
    cfg->options = NULL;

    while (fgets(line, 1023, file) != NULL) {
        line_number++;

        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] ='\0';

        if (regexec(&re_comment, line, 0, NULL, 0) == 0)
            continue;
        else if (regexec(&re_option, line, 3, matches, 0) == 0
                 && matches[1].rm_so >= 0
                 && matches[2].rm_so >= 0) {
            struct cfg_option *newopts;

            opt.name = malloc(matches[1].rm_eo - matches[1].rm_so + 1);
            opt.value = malloc(matches[2].rm_eo - matches[2].rm_so + 1);
            newopts = realloc(cfg->options,
                              sizeof(struct cfg_option) * (cfg->count + 1));

            if (newopts != NULL)
                cfg->options = newopts;

            if (opt.name == NULL || opt.value == NULL || newopts == NULL) {
                if (opt.name != NULL)
                    free(opt.name);
                if (opt.value != NULL)
                    free(opt.value);
                log_msg(LOG_WARNING, errno,
                        "Cannot parse configuration file %s", filename);
            }
            else {
                memcpy(opt.name, line + matches[1].rm_so,
                       matches[1].rm_eo - matches[1].rm_so);
                opt.name[matches[1].rm_eo - matches[1].rm_so] = '\0';

                memcpy(opt.value, line + matches[2].rm_so,
                       matches[2].rm_eo - matches[2].rm_so);
                opt.value[matches[2].rm_eo - matches[2].rm_so] = '\0';

                cfg->options[cfg->count] = opt;
                cfg->count++;
            }
        }
        else {
            log_msg(LOG_WARNING, 0, "Syntax error in %s line %d: %s",
                    filename, line_number, line);
        }
    }

    regfree(&re_option);
    regfree(&re_comment);
    fclose(file);

    return cfg;
}/*}}}*/


void cfg_free(struct cfg_file *cfg)
{/*{{{*/
    if (cfg->options != NULL) {
        int i;
        for (i = 0; i < cfg->count; i++) {
            free(cfg->options[i].name);
            free(cfg->options[i].value);
        }
    }

    free(cfg->options);
    free(cfg);
}/*}}}*/


void cfg_reset(struct cfg_file *cfg)
{/*{{{*/
    cfg->cursor = 0;
}/*}}}*/


const char *cfg_get_string(struct cfg_file *cfg,
                           enum cfg_search search,
                           const char *name,
                           const char *default_value)
{/*{{{*/
    int start;
    int i;
    const char *value = NULL;
    int pos = -1;

    if (search == CFG_NEXT)
        start = cfg->cursor;
    else
        start = 0;

    for (i = start; i < cfg->count; i++) {
        if (strcmp(cfg->options[i].name, name) == 0) {
            value = cfg->options[i].value;
            pos = i;
            if (search != CFG_LAST)
                break;
        }
    }

    cfg->cursor = pos + 1;

    if (value == NULL)
        return default_value;
    else
        return value;

}/*}}}*/


long cfg_get_long(struct cfg_file *cfg,
                  enum cfg_search search,
                  const char *name,
                  long default_value)
{/*{{{*/
    const char *str;

    str = cfg_get_string(cfg, search, name, NULL);

    if (str == NULL)
        return default_value;
    else
        return atol(str);
}/*}}}*/


struct args *cfg_get_list(struct cfg_file *cfg,
                          enum cfg_search search,
                          const char *name)
{/*{{{*/
    const char *str;
    char *line;
    struct args *args;

    str = cfg_get_string(cfg, search, name, NULL);
    if (str == NULL)
        return NULL;

    for (; *str == ' '; str++);

    if ((line = strdup(str)) == NULL
        || (args = line_parse(line)) == NULL) {
        log_msg(LOG_ERR, errno, "Cannot parse option %s", name);
        return NULL;
    }

    return args;
}/*}}}*/


void cfg_free_list(struct args *args)
{/*{{{*/
    line_parse_undo(args);
    free(args->argv[0]);
    free(args);
}/*}}}*/

