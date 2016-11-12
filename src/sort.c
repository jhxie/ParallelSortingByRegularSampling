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

void sort_launch(const struct cli_arg *const arg)
{
        int rank = 0;
        /*
         * For one-process sequential sort and non-phased version of PSRS,
         * the outputs are the same: an array with the member 'AVERAGE'
         * along with 'STDEV' are stored.
         *
         * NOTE:
         * Refer to the definition of 'enum sort_stat' in 'include/psrs/sort.h'
         * for details.
         */
        double ssort_data[SORT_STAT_SIZE];
        double psort_data[SORT_STAT_SIZE];

        /*
         * For a per-phase sorting time, the output is simply the time taken
         * for each separate phase.
         * The array is indexed with 'PHASE1' up to 'PHASE4'.
         *
         * NOTE:
         * Refer to the definition of 'enum psrs_phase' in
         * 'include/psrs/sort.h' for details.
         */
        double psort_per_phase_data[PHASE_COUNT];

        if (NULL == arg || 0 == arg->process) {
                errno = EINVAL;
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        /*
         * If only 1 process is involved, use standard quick sort.
         *
         * NOTE: Average and standard deviation is calculated by
         * 'sequential_sort' directly, so simply gather the result.
         */
        if (1 == arg->process) {
                if (0 > sequential_sort(ssort_data, arg)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }

        } else if (1 < arg->process) {
                /*
                 * NOTE:
                 * The array size is different since the outputs
                 * are different.
                 */
                if (arg->phase) {
                        parallel_sort(psort_per_phase_data, arg);
                } else {
                        parallel_sort(psort_data, arg);
                }
        }

        if (0 == rank) {
                if (1 == arg->process) {
                        output_write(ssort_data, arg);
                } else if (1 < arg->process) {
                        if (arg->phase) {
                                output_write(psort_per_phase_data, arg);
                        } else {
                                output_write(psort_data, arg);
                        }
                }
        }

        MPI_Barrier(MPI_COMM_WORLD);
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

static void
output_write(double data[const], const struct cli_arg *const arg)
{
        if (NULL == data || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (1 == arg->process) {
                if (arg->binary) {
                        fwrite(data, sizeof(data[0]), SORT_STAT_SIZE, stdout);
                } else {
                        puts("Mean Sorting Time, Standard Deviation");
                        printf("%f, %f\n", data[MEAN], data[STDEV]);
                }
        } else if (1 < arg->process) {
                if (arg->binary) {
                        /* There are 4 elements for the per-phased data. */
                        if (arg->phase) {
                                fwrite(data,
                                       sizeof(data[0]),
                                       PHASE_COUNT,
                                       stdout);
                        } else {
                                fwrite(data,
                                       sizeof(data[0]),
                                       SORT_STAT_SIZE,
                                       stdout);
                        }
                } else {
                        /* There are 4 elements for the per-phased data. */
                        if (arg->phase) {
                                puts("Phase 1, Phase 2, Phase 3, Phase 4");
                                printf("%f, %f, %f, %f\n",
                                       data[PHASE1],
                                       data[PHASE2],
                                       data[PHASE3],
                                       data[PHASE4]);
                        } else {
                                puts("Mean Sorting Time, Standard Deviation");
                                printf("%f, %f\n", data[MEAN], data[STDEV]);
                        }
                }
        }
}

static int
sequential_sort(double ssort_stats[const], const struct cli_arg *const arg)
{
        double elapsed = 0, one_process_avg = 0, one_process_stdev = 0;
        long *array = NULL;
        struct timespec start;
        struct moving_window *window = NULL;

        if (NULL == ssort_stats || NULL == arg) {
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

        if (0 > moving_average_calc(window, &one_process_avg)) {
                return -1;
        }

        if (0 > moving_stdev_calc(window, &one_process_stdev)) {
                return -1;
        }

        array_destroy(&array);
        moving_window_destroy(&window);

        ssort_stats[MEAN] = one_process_avg;
        ssort_stats[STDEV] = one_process_stdev;
        return 0;
}

static void
parallel_sort(double psort_stats[const], const struct cli_arg *const arg)
{
        /*
         * Sorting time per-phase; all the fields are filled regardless
         * of the value of 'arg->phase'.
         */
        double sort_time[PHASE_COUNT];
        double total_sort_time;
        struct moving_window *phase_wdw[PHASE_COUNT];
        struct moving_window *total_wdw = NULL;

        memset(sort_time, 0, sizeof sort_time);

        if (arg->phase) {
                memset(phase_wdw, 0, sizeof phase_wdw);

                /*
                 * Initialize 4 parallel windows in order to calculate the
                 * moving average for each phase.
                 */
                for (int j = PHASE1; j < PHASE_COUNT; ++j) {
                        if (0 > moving_window_init(&(phase_wdw[j]),
                                                   arg->window)) {
                                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                        }
                }
        } else {
                if (0 > moving_window_init(&total_wdw, arg->window)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        }

        for (unsigned int i = 0; i < arg->run; ++i) {
                /*
                 * 'sort_time' always records sorting times per phase per run.
                 */
                psort_launch(sort_time, arg);

                if (arg->phase) {
                        for (int j = PHASE1; j < PHASE_COUNT; ++j) {
                                if (0 > moving_window_push(phase_wdw[j],
                                                           sort_time[j])) {
                                        MPI_Abort(MPI_COMM_WORLD,
                                                  EXIT_FAILURE);
                                }
                        }
                } else {
                        total_sort_time = 0;
                        for (int j = 0; j < PHASE_COUNT; ++j) {
                                total_sort_time += sort_time[j];
                        }
                        /*
                         * If 'arg->phase' is 'false', a moving average based
                         * on a series of total sorting time can be calculated.
                         */
                        if (0 > moving_window_push(total_wdw,
                                                   total_sort_time)) {
                                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                        }
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }

        /*
         * NOTE:
         * The size of 'psort_stats' differs depends on whether 'arg->phase'
         * is set to 'true'.
         */
        if (arg->phase) {
                for (int j = PHASE1; j < PHASE_COUNT; ++j) {
                        if (0 > moving_average_calc(phase_wdw[j],
                                                    &(psort_stats[j]))) {
                                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                        }
                        moving_window_destroy(&(phase_wdw[j]));
                }
        } else {
                if (0 > moving_average_calc(total_wdw, &(psort_stats[MEAN]))) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                if (0 > moving_stdev_calc(total_wdw, &(psort_stats[STDEV]))) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                moving_window_destroy(&total_wdw);
        }
}

static void
psort_launch(double elapsed[const], const struct cli_arg *const arg)
{
        long *array = NULL;
        /* Number of elements to be processed per process. */
        int chunk_size = (int)ceil((double)arg->length / arg->process);
        struct process_arg process_info;

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
                puts("\n------------------------------");
                puts("Phase 0: Array Generation from Root");
                puts("\n------------------------------");
                puts("The array to be sorted is:");
                for (int i = 0; i < arg->length; ++i) {
                        printf("%ld\t", array[i]);
                }
                puts("\n------------------------------");
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

        psort_start(elapsed, array, &process_info);

        if (process_info.root) {
                array_destroy(&array);
        }
}

static void
psort_start(double elapsed[const],
            long array[const],
            struct process_arg *const arg)
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

        /* Phase 4 Result */
        struct partition result;
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
         * Phase 1.1
         *
         * Scatter the generated 'array' to each process from root.
         */
        local_scatter(array, arg);
        /*
         * Phase 1.2
         *
         * Local regular samples of all process are written into
         * 'local_samples' structure.
         *
         * NOTE: Ownership of 'local_samples.head' is transferred back to
         * this function first, then to 'pivots_bcast' later.
         */
        local_sort(&local_samples, arg);

        MPI_Barrier(MPI_COMM_WORLD);
        if (arg->root) {
                timing_stop(&(elapsed[PHASE1]), &start);
                timing_reset(&start);
                timing_start(&start);
        }

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

        MPI_Barrier(MPI_COMM_WORLD);
        if (arg->root) {
                timing_stop(&(elapsed[PHASE2]), &start);
                timing_reset(&start);
                timing_start(&start);
        }
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
         *
         * NOTE:
         * Ownership of 'blk' is transferred to 'partition_exchange'
         * function; the partition copies would be written into 'blk_copy'
         * structure after the function returns; the initial sorted local
         * array stored in 'arg->head' is no longer needed for each process
         * after the partition exchange is done, so its ownership is hand
         * over to 'partition_exchange' as well.
         */
        if (0 > part_blk_init(&blk_copy, true, pivots.size + 1)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        partition_exchange(blk_copy, blk, arg);

        MPI_Barrier(MPI_COMM_WORLD);
        if (arg->root) {
                timing_stop(&(elapsed[PHASE3]), &start);
                timing_reset(&start);
                timing_start(&start);
        }
        /*
         * Phase 4 - Merge Partitions
         *
         * NOTE: Ownership of 'blk_copy' is transferred to 'partition_merge'
         * function while ownership of 'result.head' is transferred back
         * from it for the ROOT process ONLY.
         */
        partition_merge(&result, blk_copy, arg);

        MPI_Barrier(MPI_COMM_WORLD);
        /* End */
        if (arg->root) {
                timing_stop(&(elapsed[PHASE4]), &start);
#ifdef PRINT_DEBUG_INFO
                puts("\n------------------------------");
                puts("Phase 5: Result Verification");
                puts("\n------------------------------");
                long *cmp = calloc(arg->total_size, sizeof(long));
                memcpy(cmp, result.head, arg->total_size * sizeof(long));
                qsort(cmp, arg->total_size, sizeof(long), long_compare);
#if 0
                puts("The sorted array is:");
                for (int i = 0; i < arg->total_size; ++i) {
                        printf("%ld\t", result.head[i]);
                }
#endif
                if (0 !=
                    memcmp(cmp, result.head, arg->total_size * sizeof(long))) {
                        puts("The Result is Wrong!");
                        puts("------------------------------");

#if 0
                        puts("Correct Sorted List:");
                        for (size_t i = 0; i < arg->total_size; ++i) {
                                printf("%ld\t", cmp[i]);
                        }
                        puts("");
                        puts("Actual Sorted List:");
                        for (size_t i = 0; i < arg->total_size; ++i) {
                                printf("%ld\t", arg->head[i]);
                        }
                        puts("");
#endif
                } else {
                        puts("The Result is Right!");
                        puts("------------------------------");
                }
                free(cmp);
#endif
                free(result.head);
        }
        MPI_Barrier(MPI_COMM_WORLD);
}

/* ------------------------------- Phase 1.1 ------------------------------- */
static void
local_scatter(long array[const], struct process_arg *const arg)
{
        /*
         * NOTE:
         * 'array' should be 'NULL' for every process other than root.
         */
        if ((arg->root && NULL == array) || (!arg->root && NULL != array) ||\
            NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        /*
         * Each process allocate the memory needed to store the sub-array.
         *
         * NOTE: Ownership is transferred back to caller.
         */
        arg->head = (long *)calloc(arg->size, sizeof(long));

        MPI_Barrier(MPI_COMM_WORLD);

        /*
         * Last process broadcast the 'max_sample_size' member to every
         * other process.
         */
        MPI_Bcast(&(arg->max_sample_size),
                  1,
                  MPI_INT,
                  arg->process - 1,
                  MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);

        /* Scatter the sub-array to each process. */
        MPI_Scatter(array,
                    arg->size,
                    MPI_LONG,
                    arg->head,
                    arg->size,
                    MPI_LONG,
                    0,
                    MPI_COMM_WORLD);

#if 0
        puts("\n------------------------------");
        puts("Phase 1.1: Scattering Sub-Arrays to Each Process");
        puts("\n------------------------------");
        for (int i = 0; i < arg->process; ++i) {
                if (arg->id == i) {
                        printf("Sub-array of process #%d\n", arg->id);
                        for (int j = 0; j < arg->size; ++j) {
                                printf("%ld\t", arg->head[j]);
                        }
                        puts("\n------------------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
}
/* ------------------------------- Phase 1.1 ------------------------------- */

/* ------------------------------- Phase 1.2 ------------------------------- */
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

#if 0
        puts("\n------------------------------");
        puts("Phase 1.2: Sorting Local Samples");
        puts("\n------------------------------");
        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        printf("Local samples from Process #%d\n", arg->id);
                        for (int j = 0; j < local_samples->size; ++j) {
                                printf("%ld\t", local_samples->head[j]);
                        }
                        puts("\n------------------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
}
/* ------------------------------- Phase 1.2 ------------------------------- */

/* ---------------------------- Phase 2.1 - 2.2 ---------------------------- */
static void
pivots_bcast(struct partition *const pivots,
             struct partition *const local_samples,
             const struct process_arg *const arg)
{
        /* ρ (rho) = floor(p / 2) */
        int pivot_step = 0;
        struct list *pivot_list = NULL;
        struct partition total_samples;

        if (NULL == pivots || NULL == local_samples || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        pivot_step = arg->process / 2;
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

#if 0
        puts("\n------------------------------");
        puts("Phase 2.1: Gathering Samples from Root");
        puts("\n------------------------------");
        if (arg->root) {
                puts("Gathered samples from Root");
                for (int j = 0; j < total_samples.size; ++j) {
                        printf("%ld\t", total_samples.head[j]);
                }
                puts("\n------------------------------");
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

#if 0
        puts("\n------------------------------");
        puts("Phase 2.2: Receiving Pivots");
        puts("\n------------------------------");
        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        printf("Pivots from Process #%d\n", arg->id);
                        for (int j = 0; j < pivots->size; ++j) {
                                printf("%ld\t", pivots->head[j]);
                        }
                        puts("\n------------------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
}
/* ---------------------------- Phase 2.1 - 2.2 ---------------------------- */

/* ------------------------------- Phase 2.3 ------------------------------- */
static void
partition_form(struct part_blk *const blk,
               struct partition *const pivots,
               const struct process_arg *const arg)
{
        int part_idx = 0;
        int sub_idx = 0;
        int prev_part_size = 0;
        long pivot = 0;

        if (NULL == blk || NULL == pivots || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        blk->part[part_idx].head = arg->head;
        for (int pivot_idx = 0; pivot_idx < pivots->size; ++pivot_idx) {
                pivot = pivots->head[pivot_idx];
#if 0
                for (sub_idx = 0;
                     blk->part[part_idx].head[sub_idx] <= pivot;
                     ++sub_idx);
#endif

                if (0 > bin_search(&sub_idx,
                                   pivot,
                                   blk->part[part_idx].head,
                                   arg->size - prev_part_size)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }


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
#if 0
        int per_process_size = 0, total_size = 0;

        puts("\n------------------------------");
        puts("Phase 2.3: Splitting Sub-Array into Partitions");
        puts("\n------------------------------");
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
                        puts("\n------------------------------");
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
/* ------------------------------- Phase 2.3 ------------------------------- */

/* -------------------------------- Phase 3 -------------------------------- */
static void
partition_exchange(struct part_blk *const blk_copy,
                   struct part_blk *blk,
                   const struct process_arg *const arg)
{
        int part_idx = 0;

        if (NULL == blk_copy || NULL == blk || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* i identifies the current sending process. */
        for (int i = 0; i < arg->process; ++i) {
                partition_send(blk_copy, blk, i, &part_idx, arg);
        }

        if (0 > part_blk_destroy(&blk)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        free(arg->head);

#if 0
        int per_process_size = 0, total_size = 0;

        puts("\n------------------------------");
        puts("Phase 3: Exchanging Partitions");
        puts("\n------------------------------");
        for (int i = 0; i < arg->process; ++i) {
                if (i == arg->id) {
                        per_process_size = 0;
                        /* Sum the size for all the partitions of a process. */
                        for (int j = 0; j < blk_copy->size; ++j) {
                                per_process_size += blk_copy->part[j].size;
                        }
                        printf("Partition Size for Process #%d is: %d\n",
                               i, per_process_size);
                        puts("\n------------------------------");
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
        MPI_Barrier(MPI_COMM_WORLD);
#endif
}

static void
partition_send(struct part_blk *const blk_copy,
               struct part_blk *const blk,
               const int sid,
               int *const pindex,
               const struct process_arg *const arg)
{
        MPI_Status recv_status;

        if (NULL == blk_copy || NULL == blk ||\
            0 > sid || NULL == pindex || 0 > *pindex || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        memset(&recv_status, 0, sizeof(MPI_Status));
        /*
         * Copy the corresponding partition to itself if the current sending
         * process is the same as 'arg->id'.
         */
        if (sid == arg->id) {
                blk_copy->part[*pindex].size = blk->part[arg->id].size;
                blk_copy->part[*pindex].head = (long *)\
                                               calloc(blk->part[arg->id].size,
                                                      sizeof(long));
                if (NULL == blk_copy->part[*pindex].head) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                memcpy(blk_copy->part[*pindex].head, blk->part[arg->id].head,
                       blk->part[arg->id].size * sizeof(long));
                ++*pindex;
        }
        /*
         * From 5.7 (Chapter 5 Section 7) of
         * Multicore and GPU Programming
         * "
         * MPI_Send is called a blocking send operation, implying that the
         * sender blocks until the message is delivered. However, this term is
         * misleading because the function may return before the message is
         * delivered!
         *
         * MPI_Send uses the so called standard communication mode. What really
         * happens is that MPI decides, based on the size of the message,
         * whether to block the call until the destination process collects it
         * or to return before a matching receive is issued.
         *
         * The latter is chosen if the message is small enough, making
         * MPI_Send locally blocking, i.e., the function returns as soon as the
         * message is copied to a local MPI buffer, boosting CPU utilization.
         * "
         *
         * In this partition sending case, a globally blocking send operation
         * is needed for the sender to ensure that the destination process has
         * actually retrieved the message.
         */

        /* j identifies the partition to be sent. */
        for (int j = 0; j < arg->process; ++j) {
                /*
                 * There is no need to send anything if the id of the sender
                 * process equals to the id of the partition to be sent.
                 */
                if (sid != j) {
                        if (sid == arg->id) {
                                MPI_Ssend(&(blk->part[j].size),
                                          1,
                                          MPI_INT,
                                          j,
                                          0,
                                          MPI_COMM_WORLD);
                                MPI_Ssend(blk->part[j].head,
                                          blk->part[j].size,
                                          MPI_LONG,
                                          j,
                                          0,
                                          MPI_COMM_WORLD);
                        } else if (j == arg->id) {
                                MPI_Recv(&(blk_copy->part[*pindex].size),
                                         1,
                                         MPI_INT,
                                         sid,
                                         MPI_ANY_TAG,
                                         MPI_COMM_WORLD,
                                         &recv_status);
                                mpi_recv_check(&recv_status,
                                               MPI_INT,
                                               1);
                                blk_copy->part[*pindex].head =(long *)calloc(\
                                                blk_copy->part[*pindex].size,
                                                sizeof(long));
                                if (NULL == blk_copy->part[*pindex].head) {
                                        MPI_Abort(MPI_COMM_WORLD,
                                                  EXIT_FAILURE);
                                }
                                MPI_Recv(blk_copy->part[*pindex].head,
                                         blk_copy->part[*pindex].size,
                                         MPI_LONG,
                                         sid,
                                         MPI_ANY_TAG,
                                         MPI_COMM_WORLD,
                                         &recv_status);
                                mpi_recv_check(&recv_status,
                                               MPI_LONG,
                                               blk_copy->part[*pindex].size);
                                ++*pindex;
                        }
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
}
/* -------------------------------- Phase 3 -------------------------------- */

/* -------------------------------- Phase 4 -------------------------------- */
static void
partition_merge(struct partition *const result,
                struct part_blk *blk_copy,
                const struct process_arg *const arg)
{
        struct partition running_result;
        struct partition merge_dump;
        MPI_Status recv_status;

        if (NULL == result || NULL == blk_copy || NULL == arg) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        memset(result, 0, sizeof(struct partition));
        memset(&recv_status, 0, sizeof(MPI_Status));

        /* Perform a shallow copy of the 1st partition. */
        running_result = blk_copy->part[0];
        for (int i = 1; i < arg->process; ++i) {
                merge_dump.size = running_result.size + blk_copy->part[i].size;
                merge_dump.head = calloc(merge_dump.size, sizeof(long));
                if (NULL == merge_dump.head) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                array_merge(merge_dump.head,
                            running_result.head,
                            running_result.size,
                            blk_copy->part[i].head,
                            blk_copy->part[i].size);
#if 0
                free(running_result.head);
                free(blk_copy->part[i].head);
#endif
                running_result = merge_dump;
        }

        /*
         * The merged result of all partitions is stored in 'running_result'.
         * 'blk_copy' can be safely discarded for all processes.
         */
        if (0 > part_blk_destroy(&blk_copy)) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /*
         * Root process copies 'running_result.head' into the tentative
         * final 'result' since later on there is no need for the root
         * process to send merged partitions to itself.
         */
        if (arg->root) {
                result->size = arg->total_size;
                result->head = (long *)calloc(arg->total_size, sizeof(long));

                if (NULL == result->head) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                memcpy(result->head,
                       running_result.head,
                       running_result.size * sizeof(long));
        }

        MPI_Barrier(MPI_COMM_WORLD);

        /*
         * 4.2
         * The concatenation of all the lists is the final sorted list.
         * 'i' denotes the sending process in each iteration.
         *
         * NOTE: 'last_size' is only meaningful in the root process.
         */
        for (int i = 1, last_size = running_result.size, merged_size = 0;
             i < arg->process;
             ++i) {
                if (arg->root) {
                        MPI_Recv(&merged_size,
                                 1,
                                 MPI_INT,
                                 i,
                                 MPI_ANY_TAG,
                                 MPI_COMM_WORLD,
                                 &recv_status);
                        mpi_recv_check(&recv_status,
                                       MPI_INT,
                                       1);

                        MPI_Recv(result->head + last_size,
                                 merged_size,
                                 MPI_LONG,
                                 i,
                                 MPI_ANY_TAG,
                                 MPI_COMM_WORLD,
                                 &recv_status);
                        mpi_recv_check(&recv_status,
                                       MPI_LONG,
                                       merged_size);
                        last_size += merged_size;
#if 0
                        memcpy(arg->head + last_size,
                               arg[i].result,
                               arg[i].result_size * sizeof(long));
                        last_size += arg[i].result_size;
#endif
                } else if (i == arg->id) {
                        MPI_Ssend(&running_result.size,
                                  1,
                                  MPI_INT,
                                  0,
                                  0,
                                  MPI_COMM_WORLD);

                        MPI_Ssend(running_result.head,
                                  running_result.size,
                                  MPI_LONG,
                                  0,
                                  0,
                                  MPI_COMM_WORLD);
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        free(running_result.head);
}
/* -------------------------------- Phase 4 -------------------------------- */

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
                       const long left[const],
                       const size_t lsize,
                       const long right[const],
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

static int
bin_search(int *const index,
           const long value,
           const long array[const],
           const int size)
{
        int start, middle, end;

        if (NULL == index || NULL == array || 0 >= size) {
                errno = EINVAL;
                return -1;
        }

        start = 0;
        end = size - 1;
        middle = (start + end) / 2;

        while (start <= end) {
                if (array[middle] <= value) {
                        start = middle + 1;
                } else if (array[middle] > value) {
                        end = middle - 1;
                }
                middle = (start + end) / 2;
        }

        *index = start;

        return 0;
}

/*
 * Checks whether the number of elements received actually matches the
 * amount expected.
 */
static inline void
mpi_recv_check(const MPI_Status *const status,
               MPI_Datatype datatype,
               const int count)
{
        int received = 0;

        if (NULL == status || 0 > count) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        MPI_Get_count(status, datatype, &received);

        if (received != count) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
}
