#ifndef PSRS_H
#define PSRS_H

#include <inttypes.h>
#include <stddef.h>

#ifdef PSRS_ONLY

struct argument {
        size_t length;
        size_t run;
        unsigned int thread;
};

static int argument_parse(struct argument *result, int argc, char *argv[]);
static int sizet_convert(size_t *size, const char *const candidate);
static int unsigned_convert(unsigned int *number, const char *const candidate);
static void usage_show(const char *name, int status, const char *msg);
#endif


#endif /* PSRS_H */
