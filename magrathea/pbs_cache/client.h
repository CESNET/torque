#define  _LOG_DEBUG 0

void _log(int way, int level, char *fmt, ...);
void log_close();
int report_init(const char *file);
