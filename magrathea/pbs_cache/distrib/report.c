/*
 *      report.c
 *
 *      Copyright 2009 Jaroslav Tomeček
 *
 *
 * 		Reporting module of paxos
 */
#include "main.h"
#include "../log.h"
#include "../pbs_cache.h"
#include <stdarg.h>


FILE * inf;

FILE * get_report_desc(){
    return inf;
}

int pax_report_init() {
    #if PAXOS_PBS_LOG > 0
    if ((inf = fopen(PAXOS_LOG, "a+")) == NULL) {
        perror("Error while opening the paxos report log:");
        fprintf(stderr, "The paxos report %s will not be available\n", PAXOS_LOG);
        exit (EX_OSERR);
    }
    fprintf(inf, "\n\nNew paxos instance started\n");
    fflush(inf);
    #endif
    return 0;
}

void pax_log(int level, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (_PAXOS_DEBUG<8)
        if (level == LOG_ERR) {
            fprintf(stderr, "[%ld]", (long int)time(NULL));
            vfprintf(stderr, fmt, ap);
        } else {
            printf("[%ld]", (long int)time(NULL));
            vprintf(fmt, ap);
        }
    else {
        if (inf == NULL) {
            fprintf(stderr, "report file not opened!\n");
            return;
        }
        fprintf(inf, "[%ld]", (long int)time(NULL));
        vfprintf(inf, fmt, ap);
        fflush(inf);
    }

    va_end(ap);
}


void pax_log_close(){
	fclose(inf);
}
