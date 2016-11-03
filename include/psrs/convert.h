#ifndef CONVERT_H
#define CONVERT_H

#include <stddef.h>

int int_convert(int *number, const char *const candidate);
int unsigned_convert(unsigned int *number, const char *const candidate);
int sizet_convert(size_t *size, const char *const candidate);
#endif /* CONVERT_H */
