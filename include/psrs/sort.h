#ifndef SORT_H
#define SORT_H

#include "macro.h"
#include "list.h"
#include "psrs.h"

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

struct partition {
        long *start;
        size_t size;
};

struct thread_arg {
        bool master;
        unsigned int id;
        pthread_t tid;
        long *head; /* Starting address of the individual array. */
        size_t size; /* Size of the individual array to be sorted. */
        struct list *samples;
        struct partition *part;
        struct partition *part_copy;
        long *result;
        size_t result_size;
};

int sort_launch(const struct cli_arg *const arg);

#ifdef PSRS_SORT_ONLY
static int thread_spawn(double *average, const struct cli_arg *const arg);
static int sequential_sort(double *average, const struct cli_arg *const arg);
static void *parallel_sort(void *argument);
static int long_compare(const void *left, const void *right);
static int array_merge(long output[const],
                       long left[const],
                       const size_t lsize,
                       long right[const],
                       const size_t rsize);
#endif

#endif /* SORT_H */
