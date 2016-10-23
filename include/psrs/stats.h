#ifndef STATS_H
#define STATS_H

#include "psrs/ring.h"

int moving_average_push(struct ring *window, const double value);
int moving_average_calc(const struct ring *window, double *average);

#endif /* STATS_H */
