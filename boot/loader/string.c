/*
 * Freestanding memory primitives for the loader. The loader links without a
 * C library, and at -O0 (PROFILE=debug) the compiler lowers aggregate copies
 * to memset/memcpy libcalls instead of inlining byte loops, so the symbols
 * must exist in the image.
 */
#include <stddef.h>

/**
 * @brief Fill len bytes at dst with the low byte of value and return dst.
 * @param dst writable buffer of at least len bytes.
 * @param value fill pattern; only the low byte is used.
 * @param len byte count; 0 writes nothing.
 * @return dst.
 */
void *memset(void *dst, int value, size_t len)
{
    unsigned char *p = (unsigned char *)dst;
    while (len--) {
        *p++ = (unsigned char)value;
    }
    return dst;
}

/**
 * @brief Copy len bytes from src to dst and return dst.
 * @param dst writable buffer of at least len bytes.
 * @param src readable buffer of at least len bytes; must not overlap dst.
 * @param len byte count; 0 copies nothing.
 * @return dst.
 */
void *memcpy(void *dst, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (len--) {
        *d++ = *s++;
    }
    return dst;
}
