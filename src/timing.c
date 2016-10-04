#include "psrs/timing.h"

#include <errno.h>

int timing_start(struct timespec *start)
{
        if (NULL == start) {
                errno = EINVAL;
                return -1;
        }

        if (-1 == clock_gettime(CLOCK_REALTIME, start)) {
                return -1;
        }
        return 0;
}

int timing_stop(double *elapsed, const struct timespec *start)
{
        struct timespec result, end;
        const static long NS_PER_SEC = 1000000000;

        if (NULL == elapsed || NULL == start) {
                errno = EINVAL;
                return -1;
        }

        if (-1 == clock_gettime(CLOCK_REALTIME, &end)) {
                return -1;
        }

        /*
         * On a POSIX system, time_t is an arithmetic type.
         * Subtraction between 2 time_t values is allowed.
         */
        if (start->tv_nsec > end.tv_nsec) {
                result.tv_sec = end.tv_sec - start->tv_sec - 1;
                result.tv_nsec = NS_PER_SEC + end.tv_nsec - start->tv_nsec;
        } else {
                result.tv_sec = end.tv_sec - start->tv_sec;
                result.tv_nsec = end.tv_nsec - start->tv_nsec;
        }
        *elapsed = (double)result.tv_nsec / NS_PER_SEC + (double)result.tv_sec;
        return 0;
}
