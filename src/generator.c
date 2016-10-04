#include "psrs/generator.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

int array_generate(long **const array, const size_t length, unsigned int seed)
{
        long *temp_ptr = NULL;

        /*
         * According to 29.2 (chapter 29 section 2) of
         * "The Linux Programming Interface":
         *
         * On Linux, a thread-specific errno is achieved in a similar manner
         * to most other UNIX implementations:
         *
         * errno is defined as a macro that expands into a function call
         * returning a modifiable lvalue that is distinct for each thread.
         *
         * (Since the lvalue is modifiable, it is still possible to write
         * assignment statements of the form errno = value in threaded
         * programs.)
         */
        if (NULL == array || 0U == length) {
                errno = EINVAL;
                return -1;
        }
        if ((SIZE_MAX / sizeof(long)) < length) {
                errno = EOVERFLOW;
                return -1;
        }

        temp_ptr = (long *)malloc(sizeof(long) * length);

        if (NULL == temp_ptr) {
                return -1;
        }

        srandom(seed);

        for (size_t i = 0; i < length; ++i) {
                temp_ptr[i] = random();
        }

        *array = temp_ptr;
        return 0;
}

int array_destroy(long **const array)
{
        if (NULL == array) {
                errno = EINVAL;
                return -1;
        }

        free(*array);
        *array = NULL;
        return 0;
}
