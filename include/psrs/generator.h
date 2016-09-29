#ifndef GENERATOR_H
#define GENERATOR_H

#define _BSD_SOURCE /* Definition for random() is exposed when defined */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>

int array_generate(long **array, size_t length, unsigned int seed);
int array_destroy(long **array);

#endif /* GENERATOR_H */
