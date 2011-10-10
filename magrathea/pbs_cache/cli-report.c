/*
 *      report.c
 *
 *      Copyright 2009 Jaroslav Tomeček
 *
 *
 * 		Reporting module of paxos
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

FILE * inf;

int report_init(const char * file) {
    if ((inf = fopen(file, "a+")) == NULL) {
        perror("Error while opening the paxos report log:");
        fprintf(stderr, "The paxos report will not be available\n");
        exit (-15);
    }
    fprintf(inf, "\n\nNew instance started\n");
    fflush(inf);
    return 0;
}

void _log(int way, int level, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (way)
        if (level == -2) {
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
        vfprintf(inf, fmt, ap);
        fflush(inf);
    }

    va_end(ap);
}

void log_close(){
	fclose(inf);
}
