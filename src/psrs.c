#include "psrs/macro.h"
#define PSRS_PSRS_ONLY
#include "psrs/psrs.h"
#undef PSRS_PSRS_ONLY

#include "psrs/sort.h"
#include "psrs/timing.h"

#include <err.h>
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
        struct cli_arg arg = { .binary = false };

        if (0 > argument_parse(&arg, argc, argv)) {
                err(EXIT_FAILURE, NULL);
        }
        if (0 > sort_launch(&arg)) {
                err(EXIT_FAILURE, NULL);
        }
        /*
         * According to 29.4 (chapter 29 section 4) of
         * "The Linux Programming Interface":
         *
         * Any of the threads calls exit(), or the main thread performs a
         * return (in the main() function), which causes all threads in the
         * process to terminate immediately.
         *
         * If the main thread calls pthread_exit() instead of calling exit()
         * or performing a return , then the other threads continue to execute.
         */
#if 0
        printf("-l %zu -r %zu -s %u -t %u\n",
               arg.length, arg.run, arg.seed, arg.thread);
#endif
        return EXIT_SUCCESS;
}

static int argument_parse(struct cli_arg *result, int argc, char *argv[])
{
        /* NOTE: All the flags followed by an extra colon require arguments. */
        const static char *const OPT_STR = ":bhl:r:s:t:w:";
        const static struct option OPTS[] = {
                {"binary",   no_argument,       NULL, 'b'},
                {"help",     no_argument,       NULL, 'h'},
                {"length",   required_argument, NULL, 'l'},
                {"run",      required_argument, NULL, 'r'},
                {"seed",     required_argument, NULL, 's'},
                {"thread",   required_argument, NULL, 't'},
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
                THREAD,
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
                        if (0 > sizet_convert(&result->length, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Length is too large or not valid");
                        }
                        check[LENGTH] = true;
                        break;
                }
                case 'r': {
                        if (0 > sizet_convert(&result->run, optarg)) {
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
                case 't': {
                        if (0 > unsigned_convert(&result->thread, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Thread is too large or not valid");
                        }
                        check[THREAD] = true;
                        break;
                }
                case 'w': {
                        if (0 > sizet_convert(&result->window, optarg)) {
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
                           "Length, run, seed, thread, window arguments "
                           "must be all supplied");
        }

        if (0U == result->length || 0U == result->run || \
            0U == result->seed || 0U == result->thread || \
            0U == result->window) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length, run, seed, thread, window arguments "
                           "must be all positive");
        }

        if (result->run < result->window) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Window must be less than or equal to Run");
        }

        /* length can not be fit into a dynamically allocated array. */
        if ((SIZE_MAX / sizeof(long)) < result->length) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length is larger than (SIZE_MAX / sizeof(long))");
        }
        free(program_name);
        return 0;
}

static int sizet_convert(size_t *size, const char *const candidate)
{
        char *endptr = NULL;
        errno = 0;
        size_t temp = 0U;
        uintmax_t result = strtoumax(candidate, &endptr, 10);

        /* Check overflow */
        if (UINTMAX_MAX == result && ERANGE == errno) {
                return -1;
        /*
         * From the manual page of strtoumax(),
         * "
         * if there were no digits at all, strtoumax() stores the original
         * value of nptr in endptr (and returns 0)
         * "
         * so an argument with some numbers mixed-in would work in this case.
         */
        } else if (0U == result && endptr == candidate) {
                return -1;
        }

        temp = (size_t)result;

        if ((uintmax_t)temp == result) {
                *size = temp;
        } else {
                errno = ERANGE;
                return -1;
        }

        return 0;
}

static int unsigned_convert(unsigned int *number, const char *const candidate)
{
        char *endptr = NULL;
        errno = 0;
        unsigned int temp = 0U;
        unsigned long result = strtoul(candidate, &endptr, 10);

        /* Check overflow */
        if (UINTMAX_MAX == result && ERANGE == errno) {
                return -1;
        /*
         * From the manual page of strtoul(),
         * "
         * if there were no digits at all, strtoul() stores the original value
         * of nptr in endptr (and returns 0)
         * "
         * so an argument with some numbers mixed-in would work in this case.
         */
        } else if (0U == result && endptr == candidate) {
                return -1;
        }

        temp = (unsigned int)result;

        if ((unsigned long)temp == result) {
                *number = temp;
        } else {
                errno = ERANGE;
                return -1;
        }

        return 0;
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
                "[-t NUMBER_OF_THREADS]\n"
                "[-w MOVING_WINDOW_SIZE]\n\n"

                "[" ANSI_COLOR_BLUE "Optional Arguments" ANSI_COLOR_RESET "]\n"
                "-b, --binary\tgive binary output instead of text\n"
                "-h, --help\tshow this help message and exit\n\n"

                "[" ANSI_COLOR_BLUE "Required Arguments" ANSI_COLOR_RESET "]\n"
                "-l, --length\tlength of the array to be sorted\n"
                "-r, --run\tnumber of runs\n"
                "-s, --seed\tseed for PRNG of srandom()\n"
                "-t, --thread\tnumber of threads to launch\n"
                "-w, --window\twindow size of moving average\n"

                "\n[" ANSI_COLOR_BLUE "NOTE" ANSI_COLOR_RESET "]\n"
                "1. The moving average is calculated based on both number of\n"
                "   runs and window size: window size <= number of runs\n"
                "2. To calculate the speedup relative to a single thread,\n"
                "   remember to set the "
                ANSI_COLOR_MAGENTA "SEED" ANSI_COLOR_RESET
                " to the same value used for single thread.\n"
                "3. "
                ANSI_COLOR_MAGENTA "%d " ANSI_COLOR_RESET
                "is the optimal number of threads to be chosen.\n",
                NULL == name ? "" : name, get_nprocs());
        exit(status);
}
