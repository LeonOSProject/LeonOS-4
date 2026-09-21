#ifndef LEONOS_UTF8_STREAM_H
#define LEONOS_UTF8_STREAM_H
#include <stdint.h>

struct leonos_utf8_stream {
    uint32_t value;
    uint32_t minimum;
    unsigned remaining;
};

/* Returns 1 for a scalar, 0 while incomplete, -1 to retry a rejected byte.
 * A caller receiving -1 first emits U+FFFD, then feeds the same byte again. */
static inline int leonos_utf8_feed(struct leonos_utf8_stream *s,
                                  unsigned char byte, uint32_t *scalar)
{
    if (s->remaining) {
        if ((byte & 0xc0U) != 0x80U) {
            s->remaining = 0;
            *scalar = 0xfffdU;
            return -1;
        }
        s->value = (s->value << 6) | (byte & 0x3fU);
        if (--s->remaining) return 0;
        *scalar = s->value;
        if (*scalar < s->minimum || *scalar > 0x10ffffU ||
            (*scalar >= 0xd800U && *scalar <= 0xdfffU)) *scalar = 0xfffdU;
        return 1;
    }
    if (byte < 0x80U) { *scalar = byte; return 1; }
    if (byte >= 0xc2U && byte <= 0xdfU) {
        s->value = byte & 0x1fU; s->minimum = 0x80U; s->remaining = 1;
    } else if (byte >= 0xe0U && byte <= 0xefU) {
        s->value = byte & 0x0fU; s->minimum = 0x800U; s->remaining = 2;
    } else if (byte >= 0xf0U && byte <= 0xf4U) {
        s->value = byte & 7U; s->minimum = 0x10000U; s->remaining = 3;
    } else { *scalar = 0xfffdU; return 1; }
    return 0;
}
#endif
