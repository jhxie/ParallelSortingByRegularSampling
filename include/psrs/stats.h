#ifndef STATS_H
#define STATS_H

#include "psrs/ring.h"

/*
 * 'moving_window' is a specialized version of 'ring': it has an extra
 * 'written' field and all the values are of double type.
 */
struct moving_window {
        struct ring *ring;
        /*
         * This field is only meaningful when there is no overflow;
         * once an overflow is about to occur, it would stay at 'SIZE_MAX'
         * for all the following write(s).
         */
        size_t written;
};

int moving_window_init(struct moving_window **self, const size_t length);
int moving_window_push(struct moving_window *self, const double value);
int moving_window_destroy(struct moving_window **self);

int moving_average_calc(const struct moving_window *window, double *average);

#endif /* STATS_H */
