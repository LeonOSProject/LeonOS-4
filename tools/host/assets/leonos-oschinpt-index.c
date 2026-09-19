#define _POSIX_C_SOURCE 200809L
/* Compact dictionary index: little-endian OSCI v1, unchanged guest ABI. */
#include "tools/host/common/io.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct entry {
    unsigned char code[8];
    uint32_t start, end;
};
static int compare(const void *left, const void *right) {
    const struct entry *a = left, *b = right;
    int n = memcmp(a->code, b->code, 8);
    if (n)
        return n;
    if (a->start != b->start)
        return a->start > b->start ? 1 : -1;
    return (a->end > b->end) - (a->end < b->end);
}
static void le32(unsigned char *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (unsigned char)(v >> (i * 8));
}
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: leonos-oschinpt-index DICTIONARY OUTPUT\n");
        return 2;
    }
    struct byte_buffer in = {0}, out = {0};
    struct entry *entries = NULL;
    size_t count = 0, capacity = 0;
    int status = 1;
    if (read_file_all(argv[1], &in) || in.len > UINT32_MAX)
        goto done;
    for (size_t offset = 0; offset < in.len;) {
        size_t end = offset;
        while (end < in.len && in.data[end] != '\n' && in.data[end] != '\r')
            ++end;
        size_t content = end;
        if (end < in.len && in.data[end++] == '\r' && end < in.len &&
            in.data[end] == '\n')
            ++end;
        const unsigned char *tab =
            memchr(in.data + offset, '\t', content - offset);
        if (tab && tab != in.data + offset && in.data[offset] != '#') {
            size_t first = (size_t)(tab - in.data) + 1, stop = first;
            while (stop < content && in.data[stop] != ' ' &&
                   in.data[stop] != '\t')
                ++stop;
            size_t length = stop - first;
            int valid = length > 0 && length < 8;
            for (size_t i = first; i < stop; ++i)
                if (in.data[i] < 'a' || in.data[i] > 'z')
                    valid = 0;
            if (valid) {
                unsigned char code[8] = {0};
                memcpy(code, in.data + first, length);
                if (count && !memcmp(entries[count - 1].code, code, 8))
                    entries[count - 1].end = (uint32_t)end;
                else {
                    if (count == capacity) {
                        capacity = capacity ? capacity * 2 : 256;
                        struct entry *p =
                            realloc(entries, capacity * sizeof(*p));
                        if (!p)
                            goto done;
                        entries = p;
                    }
                    memcpy(entries[count].code, code, 8);
                    entries[count].start = (uint32_t)offset;
                    entries[count].end = (uint32_t)end;
                    ++count;
                }
            }
        }
        offset = end;
    }
    if (!count || count > UINT32_MAX || count > (SIZE_MAX - 16) / 16)
        goto done;
    qsort(entries, count, sizeof(*entries), compare);
    out.len = 16 + count * 16;
    if (buffer_reserve(&out, out.len))
        goto done;
    memcpy(out.data, "OSCI", 4);
    le32(out.data + 4, 1);
    le32(out.data + 8, (uint32_t)count);
    le32(out.data + 12, (uint32_t)in.len);
    for (size_t i = 0; i < count; ++i) {
        unsigned char *p = out.data + 16 + i * 16;
        memcpy(p, entries[i].code, 8);
        le32(p + 8, entries[i].start);
        le32(p + 12, entries[i].end);
    }
    if (write_file_if_changed(argv[2], out.data, out.len, 0644))
        goto done;
    status = 0;
done:
    if (status)
        fprintf(stderr,
                "dictionary index: invalid input, allocation or I/O failure\n");
    free(entries);
    buffer_destroy(&in);
    buffer_destroy(&out);
    return status;
}
