
typedef struct address_list {
     char *addr;
     char *pattern;
     struct address_list *next;} address_list;

char* check_address_in_pattern(const char *what,address_list *addr_list);
void print_pattern_list(address_list *addr_list);
address_list *set_pattern_list(register char *val);
