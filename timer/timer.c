#include "timer.h"
#include "stdio.h"

struct timeval start, tick, stop;

void timer_start() {
    gettimeofday(&start, NULL);
    gettimeofday(&tick, NULL);
}

void timer_tick(char *msg) {
    gettimeofday(&stop, NULL);
    double secs = (double)((stop.tv_sec * 1000000.0 + stop.tv_usec) -
                    (tick.tv_sec * 1000000.0 + tick.tv_usec)) / 1000.0;
    printf("%-15s: %.3f ms\n", msg, secs);
    gettimeofday(&tick, NULL);
}

void timer_end(char *msg) {
    gettimeofday(&stop, NULL);
    double secs = (double)((stop.tv_sec * 1000000.0 + stop.tv_usec) -
                    (start.tv_sec * 1000000.0 + start.tv_usec)) / 1000.0;
    printf("%-15s: %.3f ms\n", msg, secs);
}