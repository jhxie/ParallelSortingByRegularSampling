#include "psrs/macro.h"
#define PSRS_SORT_ONLY
#include "psrs/sort.h"
#undef PSRS_SORT_ONLY

#include "psrs/generator.h"
#include "psrs/list.h"
#include "psrs/psrs.h"
#include "psrs/timing.h"

#include <err.h>
#include <errno.h>
#include <math.h>    /* ceil() */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Even though it is a file scope variable, it is initialized and destroyed
 * in thread_spawn function; and can be perceived as it is "owned" by the
 * thread_spawn function.
 *
 * The only other function that have read access to this variable is
 * parallel_sort; which can be perceived as "borrowed" by this function.
 */
static pthread_barrier_t g_barrier;
/*
 * The following two are initialized in thread_spawn function and accessed in
 * parallel_sort function as well.
 */
static unsigned int g_total_threads;
static size_t g_total_length;

static size_t g_max_sample_size;
/*
 * Phase 1:
 * This variable is written by multiple threads each time a sample is found;
 * after a barrier, it is read by the master so there is no race conditions.
 */
static size_t g_total_samples;
static struct list *g_pivot_list;

int sort_launch(const struct cli_arg *const arg)
{
        double one_thread_average = .0;
        double n_thread_average = .0;
        double n_thread_time = .0;

        if (NULL == arg) {
                errno = EINVAL;
                return -1;
        }

        if (1U == arg->thread) {
                if (0 > sequential_sort(&one_thread_average, arg)) {
                        return -1;
                }
                printf("%f\n", one_thread_average);
        } else if (1U < arg->thread) {
                for (size_t i = 0; i < arg->run; ++i) {
                        if (0 > thread_spawn(&n_thread_time, arg)) {
                                return -1;
                        }
                        n_thread_average += n_thread_time;
                }
                n_thread_average /= (double)arg->run;
                printf("%f\n", n_thread_average);
        }

        return 0;
}

static int thread_spawn(double *elapsed, const struct cli_arg *const arg)
{
        long *array = NULL;
        /* Number of elements to be processed per thread. */
        long chunk_size = (long)ceil((double)arg->length / arg->thread);
        struct thread_arg *thread_info = NULL;
        double *one_time_elapsed = NULL;
        g_total_length = arg->length;
        g_total_threads = arg->thread;

        if (NULL == arg) {
                errno = EINVAL;
                return -1;
        }

        if (pthread_barrier_init(&g_barrier, NULL, arg->thread)) {
                return -1;
        }

        thread_info = malloc(sizeof(struct thread_arg) * arg->thread);

        if (NULL == thread_info) {
                return -1;
        }
        if (0 > array_generate(&array, arg->length, arg->seed)) {
                return -1;
        }
        /*
         * According to 29.3 (chapter 29 section 3) of
         * "The Linux Programming Interface":
         *
         * SUSv3 explicitly notes that the implementation need not initialize
         * the buffer pointed to by thread before the new thread starts
         * executing; that is, the new thread may start running before
         * pthread_create() returns to its caller.
         * If the new thread needs to obtain its own ID, then it must do so
         * using pthread_self().
         */

        /* Initialize the first element to be master thread. */
        thread_info[0U].master = true;
        thread_info[0U].id = 0U;
        thread_info[0U].tid = pthread_self();
        thread_info[0U].head = array;
        thread_info[0U].size = chunk_size;
        for (unsigned int i = 1U; i < arg->thread; ++i) {
                thread_info[i].master = false;
                thread_info[i].id = i;
                thread_info[i].head = thread_info[i-1U].head +\
                                      chunk_size;
                /* The last thread get remaining elements */
                if (arg->thread - 1 == i) {
                        if (0U != arg->length % chunk_size) {
                                thread_info[i].size = arg->length % chunk_size;
                                g_max_sample_size = arg->length % chunk_size;
                        } else {
                                thread_info[i].size = chunk_size;
                                g_max_sample_size = g_total_threads;
                        }
                } else {
                        thread_info[i].size = chunk_size;
                }
                if (pthread_create(&thread_info[i].tid,
                                   NULL,
                                   parallel_sort,
                                   &thread_info[i])) {
                        return -1;
                }
        }
        one_time_elapsed = parallel_sort(&thread_info[0U]);

        if (NULL == one_time_elapsed) {
                return -1;
        }

        for (unsigned int i = 1U; i < arg->thread; ++i) {
                if (pthread_join(thread_info[i].tid, NULL)) {
                        return -1;
                }
        }

        *elapsed = *one_time_elapsed;
        pthread_barrier_destroy(&g_barrier);
        array_destroy(&array);
        free(one_time_elapsed);
        free(thread_info);
        return 0;
}

static int sequential_sort(double *average, const struct cli_arg *const arg)
{
        double elapsed = .0, one_thread_average = .0;
        long *array = NULL;
        struct timespec start;

        if (NULL == average || NULL == arg) {
                errno = EINVAL;
                return -1;
        }

        if (0 > array_generate(&array, arg->length, arg->seed)) {
                return -1;
        }

        timing_reset(&start);
        /*
         * If the number of threads needs to be executed is 1, pthread APIs
         * need not to be invoked.
         */
        for (size_t iteration = 0; iteration < arg->run; ++iteration) {
                timing_start(&start);
                qsort(array, arg->length, sizeof(long), long_compare);
                timing_stop(&elapsed, &start);
                timing_reset(&start);
                /*
                 * Revert the unsorted version back into array
                 * using the same seed: no new memory is allocated.
                 */
                if (0 > array_generate(&array, arg->length, arg->seed)) {
                        return -1;
                }
                one_thread_average += elapsed;
                elapsed = .0;
        }
        one_thread_average /= (double)arg->run;

        array_destroy(&array);

        *average = one_thread_average;
        return 0;
}

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
static void *parallel_sort(void *argument)
{
        struct thread_arg *arg = (struct thread_arg *)argument;
        struct timespec start;
        double *elapsed = NULL;
        long *gathered_samples = NULL;
        /* w = n / p^2 */
        size_t window = g_total_length / (g_total_threads * g_total_threads);
        /* ρ (rho) = floor(p / 2) */
        size_t pivot_step = g_total_threads / 2;

        /*
         * NOTE:
         * If any single thread fails, the final result would be wrong.
         * To check for sorting failures, the result obtained from PSRS
         * is compared with the one produced by sequential_sort.
         */

        if (arg->master) {
                elapsed = (double *)malloc(sizeof(double));
                if (NULL == elapsed) {
                        return NULL;
                }
                timing_reset(&start);
        }

        pthread_barrier_wait(&g_barrier);
        if (arg->master) {
                timing_start(&start);
        }

        /* Phase 1 */
        /* 1.1 Sort disjoint local data. */
        qsort(arg->head, arg->size, sizeof(long), long_compare);
        /* 1.2 Begin regular sampling load balancing heuristic. */
        list_init(&arg->samples);

        for (size_t idx = 0, picked = 0;
             idx < arg->size && picked < g_max_sample_size;
             idx += window, ++picked) {
                list_add(arg->samples, arg->head[idx]);
                ++g_total_samples;
                if (0U == window) {
                        break;
                }
        }
        /* Wait until all threads finish writing their own samples. */
        pthread_barrier_wait(&g_barrier);

        /* Phase 2 */
        if (arg->master) {
                /* 2.1 Sort the collected samples. */
                gathered_samples = (long *)malloc(sizeof(long) *\
                                                  g_total_samples);
                list_init(&g_pivot_list);
                for (size_t i = 0, last = 0; i < g_total_threads; ++i) {
                        list_copy(arg[i].samples, gathered_samples + last);
                        last = arg[i].samples->size;
                }
                qsort(gathered_samples,
                      g_total_samples,
                      sizeof(long),
                      long_compare);
                /* 2.2 p - 1 pivots are selected from the regular sample. */
                for (size_t i = g_total_threads + pivot_step, count = 0;
                     i < g_total_samples && count < g_total_threads - 1;
                     i += g_total_threads, ++count) {
                        list_add(g_pivot_list, gathered_samples[i]);
                }
                free(gathered_samples);
                gathered_samples = NULL;
        }
        pthread_barrier_wait(&g_barrier);
        list_destroy(&arg->samples);

        /* Phase 3 */
        pthread_barrier_wait(&g_barrier);

        /* Phase 4 */
        pthread_barrier_wait(&g_barrier);

        if (arg->master) {
                timing_stop(elapsed, &start);
                list_destroy(&g_pivot_list);
                return elapsed;
        } else {
                return NULL;
        }
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
