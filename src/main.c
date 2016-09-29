#include "psrs/generator.h"

#include <stdio.h>
#include <sys/sysinfo.h>

int main(int argc, char *argv[])
{
        long *array = NULL;
        array_generate(&array, 10, 1);
        for (size_t i = 0; i < 10; ++i) {
                printf("%ld\n", array[i]);
        }

        array_destroy(&array);

        printf("You have %d processors available.\n", get_nprocs());
        if (NULL == array) {
                puts("Success");
        }
        return 0;
}
