#ifndef TIMING_H
#define TIMING_H

#include "macro.h"

#include <time.h>

int timing_start(struct timespec *start);
int timing_reset(struct timespec *start);
int timing_stop(double *elapsed, const struct timespec *start);
#endif /* TIMING_H */
