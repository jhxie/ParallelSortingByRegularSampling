#include "psrs/macro.h"
#include "psrs/convert.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

int int_convert(int *number, const char *const candidate)
{
        char *endptr = NULL;
        errno = 0;
        int temp = 0;
        long result = strtol(candidate, &endptr, 10);

        /* Check overflow */
        if ((ERANGE == errno) && (LONG_MAX == result || LONG_MIN == result)) {
                return -1;
        /*
         * From the manual page of strtol(),
         * "
         * if there were no digits at all, strtol() stores the original
         * value of nptr in endptr (and returns 0)
         * "
         * so an argument with some numbers mixed-in would work in this case.
         */
        } else if (endptr == candidate) {
                return -1;
        }

        temp = (int)result;

        if ((long)temp == result) {
                *number = temp;
        } else {
                errno = ERANGE;
                return -1;
        }

        return 0;
}

int unsigned_convert(unsigned int *number, const char *const candidate)
{
        char *endptr = NULL;
        errno = 0;
        unsigned int temp = 0U;
        unsigned long result = strtoul(candidate, &endptr, 10);

        /* Check overflow */
        if (ULONG_MAX == result && ERANGE == errno) {
                return -1;
        /*
         * From the manual page of strtoul(),
         * "
         * if there were no digits at all, strtoul() stores the original value
         * of nptr in endptr (and returns 0)
         * "
         * so an argument with some numbers mixed-in would work in this case.
         */
        } else if (endptr == candidate) {
                return -1;
        }

        temp = (unsigned int)result;

        if ((unsigned long)temp == result) {
                *number = temp;
        } else {
                errno = ERANGE;
                return -1;
        }

        return 0;
}

int sizet_convert(size_t *size, const char *const candidate)
{
        char *endptr = NULL;
        errno = 0;
        size_t temp = 0U;
        uintmax_t result = strtoumax(candidate, &endptr, 10);

        /* Check overflow */
        if (UINTMAX_MAX == result && ERANGE == errno) {
                return -1;
        /*
         * From the manual page of strtoumax(),
         * "
         * if there were no digits at all, strtoumax() stores the original
         * value of nptr in endptr (and returns 0)
         * "
         * so an argument with some numbers mixed-in would work in this case.
         */
        } else if (endptr == candidate) {
                return -1;
        }

        temp = (size_t)result;

        if ((uintmax_t)temp == result) {
                *size = temp;
        } else {
                errno = ERANGE;
                return -1;
        }

        return 0;
}

