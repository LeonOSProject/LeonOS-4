/*
 * Growable byte buffer shared by the host build tools.
 *
 * Ownership: the caller owns the struct; the buffer owns `data` once
 * buffer_reserve() succeeds. buffer_destroy() releases it. A failed reserve
 * leaves the previous contents and capacity untouched and usable.
 */
#ifndef LEONOS_HOST_COMMON_BUFFER_H
#define LEONOS_HOST_COMMON_BUFFER_H

#include <stddef.h>

struct byte_buffer {
    unsigned char *data;
    size_t len;
    size_t capacity;
};

/**
 * @brief Ensure `data` can hold at least `required` bytes, preserving what was
 *        already staged.
 * @param buf Buffer to grow; must be zero-initialised or previously reserved.
 * @param required Bytes the caller intends to write; SIZE_MAX-scale requests are
 *                 rejected rather than wrapped.
 * @return 0 on success, -1 on failure with errno set (ENOMEM for allocation or
 *         overflow requests). On failure the old buffer is still valid.
 */
int buffer_reserve(struct byte_buffer *buf, size_t required);

/**
 * @brief Release the buffer and reset the struct to its empty state.
 * @param buf Buffer to free; safe to call on a zero-initialised struct.
 */
void buffer_destroy(struct byte_buffer *buf);

#endif /* LEONOS_HOST_COMMON_BUFFER_H */
