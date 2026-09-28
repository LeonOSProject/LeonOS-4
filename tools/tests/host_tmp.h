#ifndef RELIEFOS_HOST_TMP_H
#define RELIEFOS_HOST_TMP_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Host fixtures keep all temporary files on the caller-selected filesystem. */
static inline void host_tmp_path(char *path, size_t capacity, const char *name)
{
    const char *directory = getenv("TMPDIR");
    if (!directory || !*directory) directory = ".";
    int size = snprintf(path, capacity, "%s/%s", directory, name);
    assert(size >= 0 && (size_t)size < capacity);
}
#endif
