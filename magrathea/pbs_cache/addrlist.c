
#include "addrlist.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

char* check_address_in_pattern(what, addr_list)
        register const char *what;
        register address_list *addr_list;
{
        while (addr_list != NULL) {
            if (regexec((regex_t *) addr_list->pattern,
                  what, 0, NULL, 0) == 0)
                        break;
            addr_list = addr_list->next;
        }
        if (addr_list != NULL)
            return addr_list->addr;
        else
            return NULL;
}

void print_pattern_list(address_list *addr_list)
{ address_list *addr;

  addr=addr_list;
  while(addr!=NULL) {
      printf("%s\n",addr->addr);
      addr=addr->next;
  }
}

address_list *
set_pattern_list(val)
        register char *val;
{
        address_list *pattern_list = NULL;
        address_list *addr;
        char *pattern;
        int err;

        while (*val != '\0')
        {
                char *c;

                while (*val != '\0' && isspace(*val))
                        val++;
                c = val;
                while (*val != '\0' && !isspace(*val))
                        val++;
                if ( val != c )
                {
                        if (*val != '\0')
                                *val++ = '\0';
                        addr = (address_list *)malloc(sizeof *addr);
                        if (strcmp(c, ".*@") == 0)
                                c = "[a-z0-9_\\+\\-\\.]*";
                        pattern = malloc(strlen(c) + 3);
                        strcpy(pattern, "^");
                        strcat(pattern, c);
                        strcat(pattern, "$");
                        addr->pattern = (char *)malloc(sizeof(regex_t));
                        if ((err = regcomp((regex_t *)addr->pattern,
                                pattern, REG_ICASE | REG_EXTENDED | REG_NOSUB))
                                != 0)
                        {
                                char errbuf[80];

                                regerror(err, (regex_t *)addr->pattern,
                                        errbuf, sizeof errbuf);
                                perror(errbuf);
                                free(addr->pattern);
                                free(addr);
} else {
                                addr->next = pattern_list;
                                pattern_list = addr;
                        }
                        free(pattern);
                        addr->addr=strdup(c);
                }
        }
        return pattern_list;
}

