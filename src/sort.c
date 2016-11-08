#include "psrs/macro.h"
#define PSRS_SORT_ONLY
#include "psrs/sort.h"
#undef PSRS_SORT_ONLY

#include "psrs/generator.h"
#include "psrs/list.h"
#include "psrs/psrs.h"
#include "psrs/stats.h"
#include "psrs/timing.h"

#include <errno.h>
#include <math.h>    /* ceil() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
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

/*
 * This variable is set in the thread_spawn function and read from
 * parallel_sort function.
 */
static size_t g_max_sample_size;

/*
 * Phase 1:
 * This variable is written by multiple threads each time a sample is found;
 * after a barrier, it is read by the master.
 *
 * A mutex is needed to achieve mutual exclusion.
 */
static volatile size_t g_total_samples;

/*
 * NOTE:
 * At first in the author's opinion the previous variable does not require
 * a mutex to protect since an arithmetic addition operation is considered
 * as an atomic operation; and even though the addition operation is not,
 * the final result should also be the same.
 *
 * However, turns out the missing of a mutex for protecting against write
 * access for the g_total_samples results in a long march of bug hunt that
 * lasts for days as both GDB and Valgrind points to other source location for
 * segmentation violation that does not seem to have bug after inspection.
 */
static pthread_mutex_t g_total_samples_mtx = PTHREAD_MUTEX_INITIALIZER;
/*
 * Phase 2:
 * The selected pivots are pushed to this linked list to be shared by all the
 * threads in the next phase.
 */
static struct list *g_pivot_list;
#endif

void sort_launch(const struct cli_arg *const arg)
{
        int rank = 0;
        double average = .0;
        double n_process_time = .0;
        struct moving_window *window = NULL;

        if (NULL == arg || 0 == arg->process) {
                errno = EINVAL;
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (0 > moving_window_init(&window, arg->window)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (1 == arg->process) {
                if (0 > sequential_sort(&average, arg)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        } else if (1 < arg->process) {
                for (size_t i = 0; i < arg->run; ++i) {
                        psort_launch(&n_process_time, arg);

                        if (0 > moving_window_push(window, n_process_time)) {
                                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                        }
                        MPI_Barrier(MPI_COMM_WORLD);
                }

                if (0 > moving_average_calc(window, &average)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        }

        if (0 == rank) {
                if (arg->binary) {
                        fwrite(&average, sizeof(average), 1U, stdout);
                } else {
                        printf("%f\n", average);
                }
        }

        moving_window_destroy(&window);
        MPI_Barrier(MPI_COMM_WORLD);
}

static void psort_launch(double *elapsed, const struct cli_arg *const arg)
{
        long *array = NULL;
        /* Number of elements to be processed per process. */
        int chunk_size = (int)ceil((double)arg->length / arg->process);
        struct process_arg process_info;
        double one_time_elapsed = 0;

        if (NULL == elapsed || NULL == arg) {
                errno = EINVAL;
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* Initialize the 'process_info' for each process. */
        memset(&process_info, 0, sizeof(struct process_arg));
        MPI_Comm_rank(MPI_COMM_WORLD, &(process_info.id));
        process_info.total_size = arg->length;
        process_info.process = arg->process;

        if (0 == process_info.id) {
                process_info.root = true;
        } else {
                process_info.root = false;
        }

        /* Only the root process needs to generate the array. */
        if (process_info.root) {
                if (0 > array_generate(&array, arg->length, arg->seed)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
#if 0
                puts("The array to be sorted is:");
                for (int i = 0; i < arg->length; ++i) {
                        printf("%ld\t", array[i]);
                }
                puts("\n--------------------");
#endif
        }

        /*
         * Every other process waits for the root until array generation is
         * complete.
         */
        MPI_Barrier(MPI_COMM_WORLD);

        /*
         * For all the processes other than the last, the size of the array
         * it needs to sort is just 'chunk_size'.
         */
        if (arg->process != (process_info.id + 1)) {
                process_info.size = chunk_size;
        } else {
                /*
                 * For the last process, 2 scenarios need to be considered:
                 * whether the total length of array ('arg->length') is fully
                 * divisible by 'chunk_size'; if not the last process only
                 * needs to sort the remainder; otherwise the size is the same
                 * as every other process.
                 */
                if (0 != (arg->length % chunk_size)) {
                        process_info.size = arg->length % chunk_size;
                        process_info.max_sample_size = process_info.size;
                } else {
                        process_info.size = chunk_size;
                        process_info.max_sample_size = arg->process;
                }
        }

        /*
         * Each process allocate the memory needed to store the sub-array.
         *
         * NOTE: Ownership is preserved in this function.
         */
        process_info.head = (long *)calloc(process_info.size, sizeof(long));

        MPI_Barrier(MPI_COMM_WORLD);

        /*
         * Last process broadcast the 'max_sample_size' member to every
         * other process.
         */
        MPI_Bcast(&(process_info.max_sample_size),
                  1,
                  MPI_INT,
                  arg->process - 1,
                  MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);

        /* Scatter the sub-array to each process. */
        MPI_Scatter(array,
                    chunk_size,
                    MPI_LONG,
                    process_info.head,
                    chunk_size,
                    MPI_LONG,
                    0,
                    MPI_COMM_WORLD);

#if 0
        for (int i = 0; i < arg->process; ++i) {
                if (process_info.id == i) {
                        printf("Sub-array of process #%d\n", process_info.id);
                        for (int j = 0; j < process_info.size; ++j) {
                                printf("%ld\t", process_info.head[j]);
                        }
                        puts("\n--------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif

        parallel_sort(&one_time_elapsed, &process_info);
        free(process_info.head);

        *elapsed = one_time_elapsed;

        if (process_info.root) {
                array_destroy(&array);
        }
}

static int sequential_sort(double *average, const struct cli_arg *const arg)
{
        double elapsed = .0, one_thread_average = .0;
        long *array = NULL;
        struct timespec start;
        struct moving_window *window = NULL;

        if (NULL == average || NULL == arg) {
                errno = EINVAL;
                return -1;
        }

        if (0 > moving_window_init(&window, arg->window)) {
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
        for (size_t iteration = 0U; iteration < arg->run; ++iteration) {
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
                moving_window_push(window, elapsed);
                elapsed = .0;
        }

        if (0 > moving_average_calc(window, &one_thread_average)) {
                return -1;
        }

        array_destroy(&array);
        moving_window_destroy(&window);

        *average = one_thread_average;
        return 0;
}

static void parallel_sort(double *elapsed, const struct process_arg *const arg)
{
        struct timespec start;

        /* Phase 1 Result */
        struct partition local_samples;

        /* Phase 2.1 - 2.2 Result */
        struct partition pivots;

        /* Phase 2.3 Result */
        /* An array of 'partition's. */
        struct part_blk *blk = NULL;

        /* Phase 3 Result */
        struct part_blk *blk_copy = NULL;

        int part_size = 0;

        struct partition running_result;
        struct partition merge_dump;
        /*
         * NOTE:
         * If any single process fails, the whole progress group identified
         * by 'MPI_COMM_WORLD' communicator would abort.
         *
         * This implementaion does not fully handle the case where the
         * 'arg->total_size' of the array is not fully divisible by
         * 'arg->process' since 'MPI_Gather' would access out of bound
         * memory region for the last process with maximum rank.
         */

        timing_reset(&start);
        MPI_Barrier(MPI_COMM_WORLD);

        if (arg->root) {
                timing_start(&start);
        }

        /*
         * Phase 1
         *
         * Local regular samples of all process are written into
         * 'local_samples' structure.
         *
         * NOTE: Ownership of 'local_samples.head' is transferred back to
         * this function first, then to 'pivots_bcast' later.
         */
        local_sort(&local_samples, arg);

        /*
         * Phase 2 - Find Pivots then Partition.
         */

        /*
         * Phase 2.1 - 2.2
         *
         * Given 'local_samples' from each process, forms 'total_samples'
         * and picks, broadcasts 'pivots' to all processes.
         *
         * NOTE: Ownership of 'pivots.head' is transferred back to
         * this function.
         */
        pivots_bcast(&pivots, &local_samples, arg);

        /*
         * Abort if the total number of pivots is not 1 less than the total
         * number of processes.
         */
        if (arg->process != pivots.size + 1) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* Phase 2.3
         *
         * Each process receives a copy of the 'pivots' and forms p partitions
         * from their sorted local blocks.
         *
         * NOTE: 'blk' temporarily lends to 'partition_form', the ownership
         * is not transferred; but the ownership of 'pivots.head' is
         * transferred because 'pivots.head' is not needed after this phase.
         */
        if (0 > part_blk_init(&blk, false, pivots.size + 1)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        partition_form(blk, &pivots, arg);

        /*
         * Phase 3 - Exchange Partitions
         *
         * Each processor i keeps the i-th partition for itself and assigns
         * the j-th partition to the j-th processor.
         *
         * In this implementation each thread does not distinguish between
         * partitions from others and the partition belong to itself:
         * the 'part' member is filled by the last step of previous phase,
         * which merely records the beginning addresses and size for each
         * partition and no copy is involved.
         */
#if 0
        for (int i = 0; i < arg->process; ++i) {
                for (int j = 0; j < pivots.size + 1; ++j) {
                        if (i != j) {
                                if (i == arg->id) {
                                } else if (j == arg->id) {
                                }
                        }
                        MPI_Barrier(MPI_COMM_WORLD);
                }
        }

        arg->part_copy = (struct partition *)calloc(g_pivot_list->size + 1U,
                                                    sizeof(struct partition));
        arg->result_size = 0U;
        for (ssize_t i = -(ssize_t)arg->id, j = 0;
             i < g_total_threads - arg->id &&
             (size_t)j < g_pivot_list->size + 1U;
             ++i, ++j) {
                part_size = (*(arg + i)).part[arg->id].size;
                arg->result_size += part_size;

                arg->part_copy[j].start = (long *)calloc(part_size,
                                                         sizeof(long));
                arg->part_copy[j].size = part_size;
                memcpy(arg->part_copy[j].start,
                       (*(arg + i)).part[arg->id].start,
                       part_size * sizeof(long));
        }
        pthread_barrier_wait(&g_barrier);
        free(arg->part);

        /*
         * Phase 4 - Merge Partitions
         */

        /*
         * Assume the 'g_pivot_list->size' is at least 1U.
         * Perform a shallow copy of the 1st partition.
         */
        running_result = arg->part_copy[0U];
        for (size_t i = 1U; i < g_pivot_list->size + 1U; ++i) {
                merge_dump.size = running_result.size + arg->part_copy[i].size;
                merge_dump.start = calloc(merge_dump.size, sizeof(long));
                if (NULL == merge_dump.start) {
                        return NULL;
                }
                array_merge(merge_dump.start,
                            running_result.start,
                            running_result.size,
                            arg->part_copy[i].start,
                            arg->part_copy[i].size);
                free(running_result.start);
                free(arg->part_copy[i].start);
                running_result = merge_dump;
        }
        arg->result = running_result.start;
        pthread_barrier_wait(&g_barrier);

        /* 4.2 The concatenation of all the lists is the final sorted list. */
        if (arg->master) {
                for (size_t i = 0U, last_size = 0U; i < g_total_threads; ++i) {
                        memcpy(arg->head + last_size,
                               arg[i].result,
                               arg[i].result_size * sizeof(long));
                        last_size += arg[i].result_size;
                }
        }
        pthread_barrier_wait(&g_barrier);
        free(arg->result);

        /* End */
        if (arg->master) {
                timing_stop(elapsed, &start);
#ifdef PRINT_DEBUG_INFO
                size_t verify_sum = 0U;
#endif
                for (size_t i = 0U; i < g_total_threads; ++i) {
#ifdef PRINT_DEBUG_INFO
                        printf("Result Size of Thread #%zu: %zu\n",
                               i, arg[i].result_size);
                        verify_sum += arg[i].result_size;
#endif
                        free(arg[i].part_copy);
                }
                /*
                 * NOTE: The result size is completely wrong.
                 * To be fixed.
                 */
#ifdef PRINT_DEBUG_INFO
                printf("Result Size: %zu\n", verify_sum);
                printf("Pivots: %zu\n", g_pivot_list->size);
                long *cmp = calloc(g_total_length, sizeof(long));
                memcpy(cmp, arg->head, g_total_length * sizeof(long));
                qsort(cmp, g_total_length, sizeof(long), long_compare);
                if (0 !=
                    memcmp(cmp, arg->head, g_total_length * sizeof(long))) {
                        puts("The Result is Wrong!");
                        puts("------------------------------");

#if 0
                        puts("Correct Sorted List:");
                        for (size_t i = 0; i < g_total_length; ++i) {
                                printf("%ld\t", cmp[i]);
                        }
                        puts("");
                        puts("Actual Sorted List:");
                        for (size_t i = 0; i < g_total_length; ++i) {
                                printf("%ld\t", arg->head[i]);
                        }
                        puts("");
#endif
                }
                free(cmp);
#endif
                list_destroy(&g_pivot_list);
                return elapsed;
        } else {
                return NULL;
        }
#endif
}

static void
local_sort(struct partition *const local_samples,
           const struct process_arg *const arg)
{
        /* w = n / p^2 */
        int window = arg->total_size / (arg->process * arg->process);
        struct list *local_sample_list = NULL;

        memset(local_samples, 0, sizeof(struct partition));

        /* 1.1 Sort disjoint local data. */
        qsort(arg->head, arg->size, sizeof(long), long_compare);
        /* 1.2 Begin regular sampling load balancing heuristic. */
        if (0 > list_init(&local_sample_list)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        for (int idx = 0, picked = 0;
             idx < arg->size && picked < arg->max_sample_size;
             idx += window, ++picked) {
                if (0 > list_add(local_sample_list, arg->head[idx])) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                if (0U == window) {
                        break;
                }
        }
        local_samples->size = (int)local_sample_list->size;
        /* Ownership is transferred back to caller. */
        local_samples->head = (long *)calloc(local_samples->size,
                                             sizeof(long));

        if (NULL == local_samples) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (0 > list_copy(local_sample_list, local_samples->head)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (0 > list_destroy(&local_sample_list)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* Wait until all processes finish writing their own samples. */
        MPI_Barrier(MPI_COMM_WORLD);

#ifdef PRINT_DEBUG_INFO
        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        printf("Local samples from Process #%d\n", arg->id);
                        for (int j = 0; j < local_samples->size; ++j) {
                                printf("%ld\t", local_samples->head[j]);
                        }
                        puts("\n-----------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
}

static void
pivots_bcast(struct partition *const pivots,
             struct partition *const local_samples,
             const struct process_arg *const arg)
{
        /* ρ (rho) = floor(p / 2) */
        int pivot_step = arg->process / 2;
        struct list *pivot_list = NULL;
        struct partition total_samples;

        memset(pivots, 0, sizeof(struct partition));
        memset(&total_samples, 0, sizeof(struct partition));

        MPI_Reduce(&(local_samples->size),
                   &(total_samples.size),
                   1,
                   MPI_INT,
                   MPI_SUM,
                   0,
                   MPI_COMM_WORLD);

        if (arg->root) {
                /*
                 * Use calloc instead of malloc to silence the "conditional
                 * jump based on uninitialized heap variable" warning
                 * coming from valgrind.
                 */
                total_samples.head = (long *)calloc(total_samples.size,
                                                    sizeof(long));
        }

        MPI_Barrier(MPI_COMM_WORLD);
        /* Gather local samples into the root process. */
        MPI_Gather(local_samples->head,
                   local_samples->size,
                   MPI_LONG,
                   total_samples.head,
                   local_samples->size,
                   MPI_LONG,
                   0,
                   MPI_COMM_WORLD);
        /* Samples from each individual process are no longer needed. */
        free(local_samples->head);
        local_samples->head = NULL;

#ifdef PRINT_DEBUG_INFO
        if (arg->root) {
                puts("Gathered samples from Root");
                for (int j = 0; j < total_samples.size; ++j) {
                        printf("%ld\t", total_samples.head[j]);
                }
                puts("\n-----------------------");
        }
#endif

        /* 2.1 Sort the collected samples. */
        if (arg->root) {
                if (0 > list_init(&pivot_list)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                qsort(total_samples.head,
                      total_samples.size,
                      sizeof(long),
                      long_compare);
                /* 2.2 p - 1 pivots are selected from the regular sample. */
                for (int i = arg->process + pivot_step, count = 0;
                     i < total_samples.size && count < arg->process - 1;
                     i += arg->process, ++count) {
                        if (0 > list_add(pivot_list, total_samples.head[i])) {
                                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                        }
                }
                free(total_samples.head);
                total_samples.head = NULL;
                pivots->size = (int)pivot_list->size;
                pivots->head = (long *)calloc(pivots->size, sizeof(long));

                if (NULL == pivots->head) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }

                if (0 > list_copy(pivot_list, pivots->head)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }

                if (0 > list_destroy(&pivot_list)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        }
        /*
         * Wait until root process finishes sorting the gathered samples
         * and finds all the pivots.
         */
        MPI_Barrier(MPI_COMM_WORLD);

        /*
         * Every process other than root needs to know the size of the
         * 'pivots' array before allocate memory space for it.
         */
        MPI_Bcast(&(pivots->size),
                  1,
                  MPI_INT,
                  0,
                  MPI_COMM_WORLD);

        if (false == arg->root) {
                pivots->head = (long *)calloc(pivots->size, sizeof(long));

                if (NULL == pivots->head) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(pivots->head,
                  pivots->size,
                  MPI_LONG,
                  0,
                  MPI_COMM_WORLD);

#ifdef PRINT_DEBUG_INFO
        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        printf("Pivots from Process #%d\n", arg->id);
                        for (int j = 0; j < pivots->size; ++j) {
                                printf("%ld\t", pivots->head[j]);
                        }
                        puts("\n-----------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
}

static void
partition_form(struct part_blk *const blk,
               struct partition *const pivots,
               const struct process_arg *const arg)
{
        int part_idx = 0;
        int sub_idx = 0;
        int prev_part_size = 0;
        long pivot = 0;

        blk->part[part_idx].head = arg->head;
        for (int pivot_idx = 0; pivot_idx < pivots->size; ++pivot_idx) {
                pivot = pivots->head[pivot_idx];
                for (sub_idx = 0;
                     blk->part[part_idx].head[sub_idx] <= pivot;
                     ++sub_idx);
                if (0 == sub_idx) {
                        ++sub_idx;
                }
                blk->part[part_idx].size = sub_idx;
                prev_part_size += sub_idx;
                blk->part[part_idx + 1].head = blk->part[part_idx].head +\
                                               blk->part[part_idx].size;
                ++part_idx;
        }
        blk->part[part_idx].size = arg->size - prev_part_size;

        free(pivots->head);
        MPI_Barrier(MPI_COMM_WORLD);
#ifdef PRINT_DEBUG_INFO
        int per_process_size = 0, total_size = 0;

        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        printf("Partition Size for Process #%d\n",
                               i);
                        per_process_size = 0;
                        /* Sum the size for all the partitions of a process. */
                        for (int j = 0; j < blk->size; ++j) {
                                per_process_size += blk->part[j].size;
                        }
                        printf("Partition Size is: %d\n", per_process_size);
                        puts("\n-----------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }

        MPI_Reduce(&per_process_size,
                   &total_size,
                   1,
                   MPI_INT,
                   MPI_SUM,
                   0,
                   MPI_COMM_WORLD);
        if (arg->root) {
                printf("Total Partition Size: %d\n", total_size);
        }
#endif
}

static void
partition_exchange(struct part_blk *const blk_copy,
                   struct part_blk *const blk,
                   const struct process_arg *const arg)
{
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

int part_blk_init(struct part_blk **self, bool clean, int size)
{
        struct part_blk *blk = NULL;
        size_t total_size = sizeof(struct part_blk) +\
                            size * sizeof(struct partition);

        if (NULL == self || 0 >= size) {
                errno = EINVAL;
                return -1;
        }

        blk = (struct part_blk *)malloc(total_size);

        if (NULL == blk) {
                return -1;
        }

        memset(blk, 0, total_size);
        blk->clean = clean;
        blk->size = size;

        *self = blk;
        return 0;
}

int part_blk_destroy(struct part_blk **self)
{
        struct part_blk *blk = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        blk = *self;

        if (blk->clean) {
                for (int i = 0; i < blk->size; ++i) {
                        free(blk->part[i].head);
                }
        }

        free(blk);
        *self = NULL;

        return 0;
}
