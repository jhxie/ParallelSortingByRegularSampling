#ifndef PSRS_H
#define PSRS_H

#include "macro.h"

#include <inttypes.h>
#include <stddef.h>

struct cli_arg {
        size_t length;
        size_t run;
        unsigned int seed;
        unsigned int thread;
};

#ifdef PSRS_PSRS_ONLY
static int argument_parse(struct cli_arg *result, int argc, char *argv[]);
static int sizet_convert(size_t *size, const char *const candidate);
static int unsigned_convert(unsigned int *number, const char *const candidate);
static void usage_show(const char *name, int status, const char *msg);
#endif

#endif /* PSRS_H */
