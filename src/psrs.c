#define PSRS_ONLY
#include "psrs/psrs.h"
#undef PSRS_ONLY

#include "psrs/timing.h"
#include "psrs/macro.h"

#include <errno.h>
#include <getopt.h>      /* getopt_long() */
#include <inttypes.h>    /* uintmax_t */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* strdup() */
#include <sys/sysinfo.h> /* get_nprocs() */


int main(int argc, char *argv[])
{
        struct argument arg;

        if (0 > argument_parse(&arg, argc, argv)) {
                return EXIT_FAILURE;
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
        printf("-l %zu -t %u -r %zu\n", arg.length, arg.thread, arg.run);
        return EXIT_SUCCESS;
}

static int argument_parse(struct argument *result, int argc, char *argv[])
{
        /* NOTE: All the flags followed by an extra colon require arguments. */
        const static char *const OPT_STR = ":hl:r:t:";
        const static struct option OPTS[] = {
                {"help",     no_argument,       NULL, 'h'},
                {"length",   required_argument, NULL, 'l'},
                {"run",      required_argument, NULL, 'r'},
                {"thread",   required_argument, NULL, 't'},
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
                THREAD,
                NUM_OF_CMD_ARGS
        };
        bool all_argument_present = true;
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

        while (-1 != (opt = getopt_long(argc, argv, OPT_STR, OPTS, NULL))) {
                switch (opt) {
                /*
                 * 'case 0:' only triggered when any one of the flag
                 * field in the 'struct option' is set to non-NULL value;
                 * in this program this branch is never taken.
                 */
                case 0:
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
                case 't': {
                        if (0 > unsigned_convert(&result->thread, optarg)) {
                                usage_show(program_name,
                                           EXIT_FAILURE,
                                           "Thread is too large or not valid");
                        }
                        check[THREAD] = true;
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
                all_argument_present &= check[i];
        }

        if (false == all_argument_present) {
                usage_show(program_name,
                           EXIT_FAILURE,
                           "Length, run, thread argument must be supplied");
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
                "[-l LENGTH_OF_ARRAY]\n"
                "[-r NUMBER_OF_RUNS]\n"
                "[-t MAX_NUMBER_OF_THREADS]\n\n"

                "[" ANSI_COLOR_BLUE "Optional Arguments" ANSI_COLOR_RESET "]\n"
                "-h, --help\tshow this help message and exit\n"
                "-l, --length\tlength of the array to be sorted\n"
                "-r, --run\tnumber of runs\n"
                "-t, --thread\tmaximum number of threads to launch\n"

                "\n[" ANSI_COLOR_BLUE "NOTE" ANSI_COLOR_RESET "]\n"
                "1. The average is calculated based on the "
                "number of runs specified.\n"
                "2. The program only tests for the case(s) where "
                "   the number of threads is a power of 2;\n"
                "   for example, if "
                ANSI_COLOR_MAGENTA "MAX_NUMBER_OF_THREADS" ANSI_COLOR_RESET
                " is set to 10, then the cases where number of threads\n"
                "   is 1, 2, 4, 8 would be tested.\n"
                "3. "
                ANSI_COLOR_MAGENTA "%d " ANSI_COLOR_RESET
                "is the optimal number of threads to be chosen.\n",
                NULL == name ? "" : name, get_nprocs());
        exit(status);
}
