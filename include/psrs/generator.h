#ifndef GENERATOR_H
#define GENERATOR_H

#include "macro.h"

#include <stddef.h>

int array_generate(long **const array, const size_t length, unsigned int seed);
int array_destroy(long **const array);

#endif /* GENERATOR_H */
