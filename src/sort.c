#define PSRS_SORT_ONLY
#include "psrs/sort.h"
#undef PSRS_SORT_ONLY

#include "psrs/generator.h"
#include "psrs/psrs.h"
#include "psrs/timing.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * According to 29.3 (chapter 29 section 3) of
 * "The Linux Programming Interface":
 *
 * SUSv3 explicitly notes that the implementation need not initialize the
 * buffer pointed to by thread before the new thread starts executing; that is,
 * the new thread may start running before pthread_create() returns to its
 * caller.
 * If the new thread needs to obtain its own ID, then it must do so using
 * pthread_self().
 */

/*
 * According to 29.3 (chapter 29 section 3) of
 * "The Linux Programming Interface":
 *
 * Caution is required when using a cast integer as the return value of a
 * thread’s
 * start function. The reason for this is that PTHREAD_CANCELED , the value
 * returned
 * when a thread is canceled (see Chapter 32), is usually some implementation-
 * defined integer value cast to void *. If a thread’s start function returns
 * the
 * same integer value, then, to another thread that is doing a pthread_join(),
 * it will wrongly appear that the thread was canceled. In an application that
 * employs
 * thread cancellation and chooses to return cast integer values from a
 * thread’s
 * start functions, we must ensure that a normally terminating thread does not
 * return an integer whose value matches PTHREAD_CANCELED on that Pthreads
 * implementation. A portable application would need to ensure that normally
 * terminating threads don’t return integer values that match PTHREAD_CANCELED
 * on
 * any of the implementations on which the application is to run.
 */
int sort_launch(const struct cli_arg *const arg)
{
        double elapsed = .0, average = .0;
        long *unsorted_array = NULL, *array = NULL;
        pthread_t *control_block = NULL;
        struct timespec start;
        const size_t array_total_size = sizeof(long) * arg->length;

        unsorted_array = malloc(array_total_size);
        control_block = malloc(sizeof(pthread_t) * arg->thread);

        if (NULL == unsorted_array || NULL == control_block) {
                return -1;
        }
        if (-1 == array_generate(&array, arg->length, arg->seed)) {
                return -1;
        }
        /* Use unsorted_array as a backup to be reverted later. */
        memcpy(unsorted_array, array, array_total_size);
        /*
         * If the number of threads needs to be executed is 1, pthread APIs
         * need not to be invoked.
         */
        if (1U == arg->thread) {
                for (size_t iteration = 0; iteration < arg->run; ++iteration) {
                        timing_start(&start);
                        qsort(array, arg->length, sizeof(long), long_compare);
                        timing_stop(&elapsed, &start);
                        timing_reset(&start);
                        /* Revert the unsorted_array back into array. */
                        memcpy(array, unsorted_array, array_total_size);
                        average += elapsed;
                        elapsed = .0;
                }
                average /= (double)arg->run;
                printf("%f\n", average);
        } else {
        }

        array_destroy(&unsorted_array);
        array_destroy(&array);
        free(control_block);
        return 0;
}

void *parallel_sort(void *arg)
{
        return NULL;
}

static int long_compare(const void *left, const void *right)
{
        const long left_long = *((const long *)left);
        const long right_long = *((const long *)right);

        return (left_long < right_long ? -1 : left_long > right_long ? 1 : 0);
}

/*
 * NOTE:
 * It is callers' responsibility to ensure there are enough memory allcated for
 * the output array.
 *
 * Merge algorithm is from Section 2 Mergesort: Algorithm 2 of the
 * CME 323 lecture note 3.
 * Link:
 * http://stanford.edu/~rezab/dao/notes/Lecture03/cme323_lec3.pdf
 */
static int array_merge(long output[const],
                       long left[const],
                       const size_t lsize,
                       long right[const],
                       const size_t rsize)
{
        size_t lindex = 0U, rindex = 0U, oindex = 0U;

        if (!output || !left || !right || 0U == lsize || 0U == rsize) {
                errno = EINVAL;
                return -1;
        }

        for (; lindex < lsize && rindex < rsize; ++oindex) {
                if (left[lindex] < right[rindex]) {
                        output[oindex] = left[lindex];
                        ++lindex;
                } else {
                        output[oindex] = right[rindex];
                        ++rindex;
                }
        }

        /*
         * If any one of the left or right array is not exhausted, append all
         * the remaining into the output array.
         *
         * Both if conditionals may seem redundant but in general one guideline
         * from "The Zen of Python" is followed throughout the implementation
         * of this program:
         * "Explicit is better than implicit."
         */
        if (lindex < lsize) {
                for (; lindex < lsize; ++lindex, ++oindex) {
                        output[oindex] = left[lindex];
                }
        }

        if (rindex < rsize) {
                for (; rindex < rsize; ++rindex, ++oindex) {
                        output[oindex] = right[rindex];
                }
        }
        return 0;
}
