/*
 * Growable byte buffer implementation.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "buffer.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Rounding allocations to this multiple keeps repeated small appends from
 * reallocating on every byte. */
#define BUFFER_ALIGNMENT 64u

/* Refuse requests this close to SIZE_MAX so capacity arithmetic below cannot
 * wrap. */
#define BUFFER_SIZE_LIMIT (SIZE_MAX - BUFFER_ALIGNMENT)

/**
 * @brief Round a requested size up to the allocation granularity.
 * @param required Bytes the caller needs.
 * @param out_rounded Receives the aligned capacity.
 * @return 0 when the rounded value is representable, -1 with errno=ENOMEM when
 *         the request would overflow.
 */
static int aligned_capacity(size_t required, size_t *out_rounded)
{
    size_t slack = BUFFER_ALIGNMENT - 1u;

    if (required > BUFFER_SIZE_LIMIT) {
        return -1;
    }
    *out_rounded = (required + slack) & ~(size_t)slack;
    return 0;
}

int buffer_reserve(struct byte_buffer *buf, size_t required)
{
    size_t target;
    unsigned char *grown;

    if (buf == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (required > BUFFER_SIZE_LIMIT) {
        errno = ENOMEM;
        return -1;
    }
    if (buf->capacity >= required) {
        return 0;
    }

    target = required;
    if (aligned_capacity(required, &target) != 0) {
        errno = ENOMEM;
        return -1;
    }
    /* Never shrink: this function only ever grows. */
    if (target < buf->capacity) {
        target = buf->capacity;
    }

    grown = realloc(buf->data, target);
    if (grown == NULL) {
        /* realloc() leaves the original block allocated and intact. */
        errno = ENOMEM;
        return -1;
    }
    buf->data = grown;
    buf->capacity = target;
    return 0;
}

void buffer_destroy(struct byte_buffer *buf)
{
    if (buf == NULL) {
        return;
    }
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;
}
