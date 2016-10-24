#include "psrs/stats.h"

#include <errno.h>
#include <stdlib.h>

int moving_window_init(struct moving_window **self, const size_t length)
{
        struct moving_window *window = NULL;
        struct ring *ring = NULL;

        if (NULL == self || 0U == length) {
                errno = EINVAL;
                return -1;
        }

        window = (struct moving_window *)malloc(sizeof(struct moving_window));

        if (NULL == window) {
                return -1;
        }

        if (0 > ring_init(&ring, length, NULL, NULL)) {
                free(window);
                return -1;
        }

        window->ring = ring;
        window->written = 0U;

        *self = window;
        return 0;
}

int moving_window_push(struct moving_window *self, const double value)
{
        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        if (0 > ring_add(self->ring, &value, sizeof(double))) {
                return -1;
        }

        /*
         * Let the 'written' field stay at 'SIZE_MAX' if it is about to
         * overflow: this field is only used for testing inequality
         * between itself and the 'length' of ring in 'moving_average_calc'.
         */
        if (0U != ~(self->written)) {
                self->written++;
        }
        return 0;
}

int moving_window_destroy(struct moving_window **self)
{
        struct moving_window *window = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        window = *self;

        if (0 > ring_destroy(&(window->ring))) {
                return -1;
        }

        free(window);

        *self = NULL;
        return 0;
}

int moving_average_calc(const struct moving_window *window, double *average)
{
        double result = 0;
        double *current = NULL;
        size_t window_size = 0U;
        struct ring_iter *iter = NULL;

        if (NULL == window || NULL == average) {
                errno = EINVAL;
                return -1;
        }

        /*
         * Since both 'window' and '&window_size' cannot be NULL at this point,
         * so this function call must succeed.
         */
        ring_length(window->ring, &window_size);

        /*
         * If the number of elements 'written' in total is less than
         * the actual 'window_size', moving average can not be calculated.
         * For example, if the 'window_size' is 4, and there are only 2 numbers
         * available in the window (ring), the rest of 2 buffers in window
         * (ring) would be empty.
         */
        if (window->written < window_size) {
                errno = ENOTSUP;
                return -1;
        }

        if (0 > ring_iter_init(&iter, window->ring)) {
                return -1;
        }

        for (size_t i = 0U; i < window_size; ++i) {
                /* Both arguments can not be NULL; return value unchecked. */
                ring_iter_walk(iter, (void **)(&current));
                result += *current;
        }

        result /= (double)window_size;

        /* 'iter' can not be NULL; return value unchecked. */
        ring_iter_destroy(&iter);

        *average = result;
        return 0;
}
