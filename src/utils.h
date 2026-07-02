#ifndef UTILS_H
#define UTILS_H
#include <stddef.h>

const char *stristr(const char *haystack, const char *needle);
void format_time(int seconds, char *buf, size_t bufsz);

#endif
