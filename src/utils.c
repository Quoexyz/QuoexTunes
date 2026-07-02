#include "utils.h"
#include <ctype.h>
#include <stdio.h>

const char *stristr(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n) return haystack;
    }
    return NULL;
}

void format_time(int seconds, char *buf, size_t bufsz) {
    if (seconds < 0) seconds = 0;
    int mins = seconds / 60;
    int secs = seconds % 60;
    snprintf(buf, bufsz, "%d:%02d", mins, secs);
}
