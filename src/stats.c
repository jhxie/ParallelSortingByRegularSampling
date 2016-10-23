#include "psrs/stats.h"

#include <errno.h>

int moving_average_push(struct ring *window, const double value)
{
        if (NULL == window) {
                errno = EINVAL;
                return -1;
        }

        if (0 > ring_add(window, &value, sizeof(double))) {
                return -1;
        }
        return 0;
}

int moving_average_calc(const struct ring *window, double *average)
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
        ring_length(window, &window_size);

        if (0 > ring_iter_init(&iter, window)) {
                return -1;
        }

        for (size_t i = 0U; i < window_size; ++i) {
                ring_iter_walk(iter, (void **)(&current));
                result += *current;
        }

        result /= window_size;

        if (0 > ring_iter_destroy(&iter)) {
                return -1;
        }

        *average = result;
        return 0;
}
