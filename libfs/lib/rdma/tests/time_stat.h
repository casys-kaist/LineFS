#ifndef TIME_STAT_H
#define TIME_STAT_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

struct time_stats {
	struct timeval start;
	//struct timespec start;
	int n, count;
	double* time_v;
};

void time_stats_init(struct time_stats*, int);
void time_stats_start(struct time_stats*);
void time_stats_stop(struct time_stats*);
void time_stats_print(struct time_stats*, char*);
double time_stats_get_avg(struct time_stats*);

#endif

