#ifndef SORT_H
#define SORT_H

#include "macro.h"
#include "list.h"
#include "psrs.h"

#include <stdbool.h>
#include <stddef.h>

struct partition {
        long *head;
        int size; /* Number of elements. */
};

struct part_blk {
        bool clean; /* Whether 'free' needs to be called for each 'part'. */
        int size; /* Number of partitions. */
        struct partition part[]; /* Flexible array of 'partition's. */
};

struct process_arg {
        unsigned int root;
        int id; /* Rank of the process. */
        int process; /* Total number of processes. */
        long *head; /* Starting address of the individual array. */
        int size; /* Size of the individual array to be sorted. */
        int max_sample_size;
        /*
         * Total size of the array to be sorted;
         * 'total_size' should be equal to the sum of the 'size' member
         * for each process.
         */
        int total_size;
};

enum sort_stat {
        MEAN,
        STDEV,
        SORT_STAT_SIZE
};
/*
 * Used for indexing the array passed as a parameter to 'psort_launch'
 * and 'parallel_sort'.
 *
 * NOTE:
 * Both 'psort_launch' and 'parallel_sort' calculate per-phase sorting time
 * UN-CONDITIONALLY, in order to simplifly the implementation; 'sort_launch'
 * would make the correct output based on the command line flags given.
 */
enum psrs_phase {
        PHASE1,
        PHASE2,
        PHASE3,
        PHASE4,
        PHASE_COUNT
};

void
sort_launch(const struct cli_arg *const arg);

int
part_blk_init(struct part_blk **self, bool clean, int size);

int
part_blk_destroy(struct part_blk **self);

#ifdef PSRS_SORT_ONLY
static void
output_write(double data[const], const struct cli_arg *const arg);

static int
sequential_sort(double ssort_stats[const], const struct cli_arg *const arg);

static void
parallel_sort(double psort_stats[const], const struct cli_arg *const arg);

static void
psort_launch(double elapsed[const], const struct cli_arg *const arg);

static void
psort_start(double elapsed[const],
            long array[const],
            struct process_arg *const arg);

/* Phase 1.1 */
static void
local_scatter(long array[const], struct process_arg *const arg);

/* Phase 1.2 */
static void
local_sort(struct partition *const local_samples,
           const struct process_arg *const arg);

/* Phase 2.1 - 2.2 */
static void
pivots_bcast(struct partition *const pivots,
             struct partition *const local_samples,
             const struct process_arg *const arg);

/* Phase 2.3 */
static void
partition_form(struct part_blk *const blk,
               struct partition *const pivots,
               const struct process_arg *const arg);

/* Phase 3 */
static void
partition_exchange(struct part_blk *const blk_copy,
                   struct part_blk *blk,
                   const struct process_arg *const arg);

static void
partition_send(struct part_blk *const blk_copy,
               struct part_blk *const blk,
               const int sid,
               int *const pindex,
               const struct process_arg *const arg);
/* Phase 4 */
static void
partition_merge(struct partition *const result,
                struct part_blk *blk_copy,
                const struct process_arg *const arg);

static int
long_compare(const void *left, const void *right);

static int
array_merge(long output[const],
            const long left[const],
            const size_t lsize,
            const long right[const],
            const size_t rsize);

static int
bin_search(int *const index,
           const long value,
           const long array[const],
           const int size);

static inline void
mpi_recv_check(const MPI_Status *const status,
               MPI_Datatype datatype,
               const int count);
#endif

#endif /* SORT_H */
