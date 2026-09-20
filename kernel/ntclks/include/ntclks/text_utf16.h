/*
 * LeonOS kernel text transcoding: UTF-8 and UTF-16LE conversion for filesystem
 * name handling. Pure algorithms over caller-owned buffers.
 */
#ifndef NTCLKS_TEXT_UTF16_H
#define NTCLKS_TEXT_UTF16_H
#include <ntclks/types.h>

/* Malformed or unpaired input is replaced, never rejected, because on-disk
 * names must remain round-trippable enough to be repaired by the user. */
#define NTCLKS_TEXT_REPLACEMENT_CHAR 0xfffdu

/**
 * @brief Transcode UTF-8 to UTF-16LE, replacing each malformed byte sequence with U+FFFD.
 * @param utf8 Input bytes; must not be NULL even when utf8_len is zero.
 * @param utf8_len Number of bytes to read from `utf8`.
 * @param utf16 Destination units; may be NULL only when utf16_capacity is zero, in which
 *              case the conversion is measured without being written.
 * @param utf16_capacity Units available at `utf16`.
 * @param out_utf16_len Output; units the conversion needs, which may exceed utf16_capacity.
 *                       Always set, including on error paths. Must not be NULL.
 * @return Zero, or -EINVAL when utf8, utf16 (with nonzero capacity) or out_utf16_len is NULL.
 * @context Any kernel context: no locks, no allocation, no sleep, no shared state.
 */
int text_utf8_to_utf16le(const char *utf8, uint32_t utf8_len, uint16_t *utf16,
                         uint32_t utf16_capacity, uint32_t *out_utf16_len);

/**
 * @brief Transcode UTF-16LE to UTF-8, replacing unpaired surrogates with U+FFFD.
 * @param utf16 Input units; must not be NULL even when utf16_len is zero.
 * @param utf16_len Number of units to read from `utf16`.
 * @param utf8 Destination bytes. A NUL terminator is appended when the converted
 *             text plus terminator fits within utf8_capacity; otherwise the buffer
 *             is filled and the caller must terminate it itself.
 * @param utf8_capacity Bytes available at `utf8`; may be zero with a NULL `utf8`.
 * @param out_utf8_len Output; bytes the conversion needs excluding the terminator,
 *                     which may exceed utf8_capacity. Always set. Must not be NULL.
 * @return Zero, or -EINVAL when utf16, utf8 (with nonzero capacity) or out_utf8_len is NULL.
 * @context Any kernel context: no locks, no allocation, no sleep, no shared state.
 */
int text_utf16le_to_utf8(const uint16_t *utf16, uint32_t utf16_len, char *utf8,
                         uint32_t utf8_capacity, uint32_t *out_utf8_len);

#endif
