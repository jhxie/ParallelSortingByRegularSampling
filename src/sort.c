#include "psrs/generator.h"
#include "psrs/timing.h"

#include <stdlib.h>
#include <stdio.h>

int long_compare(const void *left, const void *right)
{
        const long left_long = *((const long *)left);
        const long right_long = *((const long *)right);

        return (left_long < right_long ? -1 : left_long > right_long ? 1 : 0);
}


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
int sort_launch()
{
        long *array = NULL;
        struct timespec start;
        double elapsed = .0;
        size_t length = 9999999UL;

        if (-1 == array_generate(&array, length, 1)) {
                return EXIT_FAILURE;
        }
        puts("Before Sorting");

        timing_start(&start);
        qsort(array, length, sizeof(long), long_compare);
        timing_stop(&elapsed, &start);
        puts("\nAfter Sorting");

        array_destroy(&array);

        printf("It takes %f seconds to sort.\n", elapsed);
        if (NULL == array) {
                puts("Success");
        }
        return 0;
}
