#ifndef SORT_H
#define SORT_H

#include "macro.h"

#include <stddef.h>

struct thread_start_arg {
        long *head;
        size_t size;
};

int sort_launch();

#ifdef PSRS_SORT_ONLY
static int long_compare(const void *left, const void *right);
static int array_merge(long output[const],
                       long left[const],
                       const size_t lsize,
                       long right[const],
                       const size_t rsize);
#endif

#endif /* SORT_H */
