#ifndef PSRS_H
#define PSRS_H

#include "macro.h"

#include <mpi.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Ensure all the members are of builtin types so MPI can transmit them
 * easily without worrying about custom defined types.
 */
struct cli_arg {
        /*
         * Whether output the sorting time(s) (moving average)
         * in binary format.
         *
         * NOTE: This is a boolean variable; however its actual type is
         * not 'bool' because there is no 'MPI_BOOL' data type built-in
         * for MPICH.
         */
        unsigned int binary;
        /*
         * Contrary to common practice these days, the 'count' formal parameter
         * of 'MPI_Send' is of 'int' type instead of 'size_t', so here a
         * similar convention needs to be maintained.
         */
        int length;
        /*
         * Whether output the sorting time(s) (moving average)
         * in a per-phase format.
         *
         * NOTE: This is a boolean variable; however its actual type is
         * not 'bool' because there is no 'MPI_BOOL' data type built-in
         * for MPICH.
         */
        unsigned int phase;
        unsigned int run;
        unsigned int seed;
        /*
         * 'process' is not a command line parameter directly supplied to the
         * program itself, but 'mpiexec' instead.
         */
        int process;
        unsigned int window;
};

#ifdef PSRS_PSRS_ONLY
static int argument_parse(struct cli_arg *result, int argc, char *argv[]);
static void argument_bcast(struct cli_arg *arg);
static void usage_show(const char *name, int status, const char *msg);
#endif

#endif /* PSRS_H */
