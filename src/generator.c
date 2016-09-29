#include "psrs/generator.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

int array_generate(long **array, size_t length, unsigned int seed)
{
        long *temp_ptr = NULL;

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

int array_destroy(long **array)
{
        if (NULL == array) {
                errno = EINVAL;
                return -1;
        }

        free(*array);
        *array = NULL;
        return 0;
}
