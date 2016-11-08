#include "psrs/macro.h"
#define PSRS_PSRS_ONLY
#include "psrs/psrs.h"
#undef PSRS_PSRS_ONLY

#include "psrs/convert.h"
#include "psrs/sort.h"

#include <errno.h>
#include <getopt.h>      /* getopt_long() */
#include <inttypes.h>    /* uintmax_t */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* strdup() */
#include <sys/sysinfo.h> /* get_nprocs() */


int main(int argc, char *argv[])
{
        /*
         * According to the manual page of MPI:
         * "
         * All MPI routines (except MPI_Wtime and MPI_Wtick ) return an error
         * value; C routines as the value of the function and Fortran routines
         * in the last argument.
         * Before the value is returned, the current MPI error handler is
         * called.
         * By default, this error handler aborts the MPI job.
         * "
         * For simplicity (incompetence of the author), this implementation
         * does not change the default behavior of error handler.
         *
         * From 3.11 (Chapter 3 section 11) of
         * Using MPI:
         * Portable Parallel Programming with the Message-Passing Interface
         * "
         * The arguments to MPI_Init in C are &argc and &argv.
         * This feature allows the MPI implementation to fill these in on all
         * processes, but the MPI standard does not require it.
         * Some implementaions propagate argc and argv to all processes;
         * some don't.
         * "
         * For portability only the root process calls the argument_parse
         * function and passes the parsed results to all the rest process(es).
         */
        MPI_Init(&argc, &argv);
        int rank;
        struct cli_arg arg = { .binary = false };

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (0 == rank) {
                if (0 > argument_parse(&arg, argc, argv)) {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
        }

        /* MPI would not deadlock even if there is only one process. */
        argument_bcast(&arg);
        /* All the processes receive a copy of 'arg' at this point. */

        /*
         * DEBUG:
         * Check whether all the arguments are passed properly.
         */
#if 0
        for (int i = 0; i < arg.process; ++i) {
                if (rank == i) {
                        printf("Process #%d\n", rank);
                        printf("Binary: %u\n"
                               "Length: %d\n"
                               "Run: %u\n"
                               "Seed: %u\n"
                               "Process: %d\n"
                               "Window: %u\n",
                               arg.binary,
                               arg.length,
                               arg.run,
                               arg.seed,
                               arg.process,
                               arg.window);
                        puts("-----------------------");
                }
                MPI_Barrier(MPI_COMM_WORLD);
        }
#endif
        sort_launch(&arg);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
        /*
         * From 3.11 (Chapter 3 section 11) of
         * Using MPI:
         * Portable Parallel Programming with the Message-Passing Interface
         * "
         * The MPI standard guarantees that at least the process with rank zero
         * in MPI_COMM_WORLD will continue executing after MPI_Finalize
         * returns.
         * This permits an application to perform any non-MPI cleanup and, in a
         * POSIX environment, provide an exit code for the program.
         * If you use this feature, make sure to test that the process is the
         * one with rank zero in MPI_COMM_WORLD - after MPI_Finalize, it's
         * possible that all MPI processes are still running.
         * "
         * In this case no extra cleanup is required, so an exit code is
         * supplied directly.
         */
        return EXIT_SUCCESS;
}

static int argument_parse(struct cli_arg *result, int argc, char *argv[])
{
        /* NOTE: All the flags followed by an extra colon require arguments. */
        static const char *const OPT_STR = ":bhl:r:s:t:w:";
        static const struct option OPTS[] = {
                {"binary",   no_argument,       NULL, 'b'},
                {"help",     no_argument,       NULL, 'h'},
                {"length",   required_argument, NULL, 'l'},
                {"run",      required_argument, NULL, 'r'},
                {"seed",     required_argument, NULL, 's'},
                {"window",   required_argument, NULL, 'w'},
                {
                        .name    = NULL,
                        .has_arg = 0,
                        .flag    = NULL,
                        .val     = 0
                }
        };
        enum {
                LENGTH,
                RUN,
                SEED,
                WINDOW,
                NUM_OF_CMD_ARGS
        };
        bool all_arguments_present = true;
        bool check[NUM_OF_CMD_ARGS] = { false };
        /*
         * NOTE:
         * This variable is not freed when this function exits by calling
         * usage_show().
         * Since usage_show() would direcly terminate the whole program,
         * so this may not count as a memory leak.
         */
        char *program_name = strdup(argv[0]);
        int opt = 0;

        if (NULL == program_name) {
                return -1;
        }
        if (NULL == result || 0 == argc || NULL == argv) {
                errno = EINVAL;
                return -1;
        }

        /* By default, output average value in human readable form. */
        result->binary = false;

        while (-1 != (opt = getopt_long(argc, argv, OPT_STR, OPTS, NULL))) {
                /*
                 * NOTE:
                 * 'sscanf' is an unsafe function since it can cause unwanted
                 * overflow and memory corruption, which has a negative impact
                 * on the robustness of the program.
                 *
                 * Reference:
                 * https://www.akkadia.org/drepper/defprogramming.pdf
                 */
                switch (opt) {
                /*
                 * 'case 0:' only triggered when any one of the flag
                 * field in the 'struct option' is set to non-NULL value;
                 * in this program this branch is never taken.
                 */
                case 0:
                        break;
                case 'b':
                        result->binary = true;
                        break;
                case 'l': {
                        if (0 > int_convert(&result->length, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Length is too large or not valid");
                        }
                        check[LENGTH] = true;
                        break;
                }
                case 'r': {
                        if (0 > unsigned_convert(&result->run, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Run is too large or not valid");
                        }
                        check[RUN] = true;
                        break;
                }
                case 's': {
                        if (0 > unsigned_convert(&result->seed, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Seed is too large or not valid");
                        }
                        check[SEED] = true;
                        break;
                }
                case 'w': {
                        if (0 > unsigned_convert(&result->window, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Window is too large or not valid");
                        }
                        check[WINDOW] = true;
                        break;
                }
                case '?':
                        usage_show(program_name,
                                   EXIT_FAILURE,
                                   "There is no such option");
                case ':':
                        usage_show(program_name,
                                   EXIT_FAILURE,
                                   "Missing argument");
                case 'h':
                default:
                        usage_show(program_name, EXIT_FAILURE, NULL);
                }
        }

        for (size_t i = 0; i < (size_t)NUM_OF_CMD_ARGS; ++i) {
                all_arguments_present &= check[i];
        }

        if (false == all_arguments_present) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length, run, seed, window arguments "
                           "must be all supplied");
        }

        if (0 >= result->length || 0U == result->run || \
            0U == result->seed || 0U == result->window) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length, run, seed, window arguments "
                           "must be all positive");
        }

        if (result->run < result->window) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Window must be less than or equal to Run");
        }

        /* length can not be fit into a dynamically allocated array. */
        if ((SIZE_MAX / sizeof(long)) < (size_t)result->length) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length is larger than (SIZE_MAX / sizeof(long))");
        }
        free(program_name);

        MPI_Comm_size(MPI_COMM_WORLD, &(result->process));
        return 0;
}

static void argument_bcast(struct cli_arg *arg)
{
        /*
         * Depends on how the processes are actually scheduled,
         * the first barrier is mainly waiting for root process.
         * */
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->binary), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->length), 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->run), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->seed), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->process), 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&(arg->window), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
}

static void usage_show(const char *name, int status, const char *msg)
{
        if (NULL != msg) {
                fprintf(stderr,
                        "[" ANSI_COLOR_BLUE "Error" ANSI_COLOR_RESET "]\n"
                        "%s\n\n",
                        msg);
        }
        fprintf(stderr,
                "[" ANSI_COLOR_BLUE "Usage" ANSI_COLOR_RESET "]\n"
                "%s [-h]\n"
                "[-b]\n"
                "[-l LENGTH_OF_ARRAY]\n"
                "[-r NUMBER_OF_RUNS]\n"
                "[-s SEED]\n"
                "[-w MOVING_WINDOW_SIZE]\n\n"

                "[" ANSI_COLOR_BLUE "Optional Arguments" ANSI_COLOR_RESET "]\n"
                "-b, --binary\tgive binary output instead of text\n"
                "-h, --help\tshow this help message and exit\n\n"

                "[" ANSI_COLOR_BLUE "Required Arguments" ANSI_COLOR_RESET "]\n"
                "-l, --length\tlength of the array to be sorted\n"
                "-r, --run\tnumber of runs\n"
                "-s, --seed\tseed for PRNG of srandom()\n"
                "-w, --window\twindow size of moving average\n"

                "\n[" ANSI_COLOR_BLUE "NOTE" ANSI_COLOR_RESET "]\n"
                "1. The moving average is calculated based on both number of\n"
                "   runs and window size: window size <= number of runs\n"
                "2. To calculate the speedup relative to a single process,\n"
                "   remember to set the "
                ANSI_COLOR_MAGENTA "SEED" ANSI_COLOR_RESET
                " to the same value used for single process.\n"
                "3. "
                ANSI_COLOR_MAGENTA "%d " ANSI_COLOR_RESET
                "is the optimal number of processes to be chosen.\n",
                NULL == name ? "" : name, get_nprocs());
        MPI_Abort(MPI_COMM_WORLD, status);
}
