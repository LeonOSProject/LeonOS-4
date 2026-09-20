/*
 * LeonOS kernel text transcoding implementation.
 * Decodes and encodes UTF-8 and UTF-16LE without libc, allocation or shared state.
 */
#include <ntclks/text_utf16.h>

#define UTF8_REPLACEMENT NTCLKS_TEXT_REPLACEMENT_CHAR

/** @brief True when `byte` is a UTF-8 continuation byte (0b10xxxxxx). */
static int utf8_is_continuation(uint8_t byte)
{
    return (byte & 0xc0u) == 0x80u;
}

/**
 * @brief Decode the UTF-8 sequence starting at `offset`, replacing invalid input.
 * @param bytes Input buffer.
 * @param length Size of `bytes`.
 * @param offset Index of the sequence to decode; must be below `length`.
 * @param out_byte_len Output; bytes consumed, always at least one so callers advance.
 * @return The decoded code point, or U+FFFD for a malformed or truncated sequence.
 */
static uint32_t utf8_decode(const uint8_t *bytes, uint32_t length, uint32_t offset,
                            uint32_t *out_byte_len)
{
    uint8_t lead;
    *out_byte_len = 1u;
    lead = bytes[offset];
    if (lead < 0x80u) {
        return lead;
    }
    if (lead < 0xc2u) {
        return UTF8_REPLACEMENT;
    }
    if (lead < 0xe0u) {
        if (offset + 1u >= length || !utf8_is_continuation(bytes[offset + 1u])) {
            return UTF8_REPLACEMENT;
        }
        *out_byte_len = 2u;
        return ((uint32_t)(lead & 0x1fu) << 6) |
               (uint32_t)(bytes[offset + 1u] & 0x3fu);
    }
    if (lead < 0xf0u) {
        uint8_t second;
        if (offset + 2u >= length) {
            return UTF8_REPLACEMENT;
        }
        second = bytes[offset + 1u];
        if (!utf8_is_continuation(second) ||
            !utf8_is_continuation(bytes[offset + 2u]) ||
            (lead == 0xe0u && second < 0xa0u) ||
            (lead == 0xedu && second >= 0xa0u)) {
            return UTF8_REPLACEMENT;
        }
        *out_byte_len = 3u;
        return ((uint32_t)(lead & 0x0fu) << 12) |
               ((uint32_t)(second & 0x3fu) << 6) |
               (uint32_t)(bytes[offset + 2u] & 0x3fu);
    }
    if (lead < 0xf5u) {
        uint8_t second;
        uint8_t third;
        if (offset + 3u >= length) {
            return UTF8_REPLACEMENT;
        }
        second = bytes[offset + 1u];
        third = bytes[offset + 2u];
        if (!utf8_is_continuation(second) || !utf8_is_continuation(third) ||
            !utf8_is_continuation(bytes[offset + 3u]) ||
            (lead == 0xf0u && second < 0x90u) ||
            (lead == 0xf4u && second >= 0x90u)) {
            return UTF8_REPLACEMENT;
        }
        *out_byte_len = 4u;
        return ((uint32_t)(lead & 0x07u) << 18) |
               ((uint32_t)(second & 0x3fu) << 12) |
               ((uint32_t)(third & 0x3fu) << 6) |
               (uint32_t)(bytes[offset + 3u] & 0x3fu);
    }
    return UTF8_REPLACEMENT;
}

/**
 * @brief Encode one code point as UTF-8 at `position`, bounded by `capacity`.
 * @param codepoint Code point to encode; values above U+10FFFF are replaced first.
 * @param out Destination bytes, or NULL when capacity is zero.
 * @param capacity Bytes available at `out`.
 * @param position Offset to write at; stays below capacity for every stored byte.
 * @return Bytes the encoding needs (1-4), regardless of how many were stored.
 */
static uint32_t utf8_encode(uint32_t codepoint, char *out, uint32_t capacity,
                            uint32_t position)
{
    uint8_t *destination = (uint8_t *)out;
    uint8_t lead_mask = 0u;
    uint32_t width;
    uint32_t index;
    if (codepoint > 0x10ffffu) {
        codepoint = UTF8_REPLACEMENT;
    }
    if (codepoint < 0x80u) {
        width = 1u;
    } else if (codepoint < 0x800u) {
        width = 2u;
        lead_mask = 0xc0u;
    } else if (codepoint < 0x10000u) {
        width = 3u;
        lead_mask = 0xe0u;
    } else {
        width = 4u;
        lead_mask = 0xf0u;
    }
    for (index = 0u; index < width; ++index) {
        uint32_t shift = (width - 1u - index) * 6u;
        uint8_t byte;
        if (index == 0u) {
            byte = width == 1u ? (uint8_t)codepoint
                               : (uint8_t)(lead_mask | (codepoint >> shift));
        } else {
            byte = (uint8_t)(0x80u | ((codepoint >> shift) & 0x3fu));
        }
        if (position + index < capacity) {
            destination[position + index] = byte;
        }
    }
    return width;
}

int text_utf8_to_utf16le(const char *utf8, uint32_t utf8_len, uint16_t *utf16,
                         uint32_t utf16_capacity, uint32_t *out_utf16_len)
{
    uint32_t offset = 0u;
    uint32_t produced = 0u;
    if (!out_utf16_len) {
        return -22;
    }
    *out_utf16_len = 0u;
    if (!utf8 || (utf16_capacity != 0u && !utf16)) {
        return -22;
    }
    while (offset < utf8_len) {
        uint32_t byte_len = 1u;
        uint32_t codepoint = utf8_decode((const uint8_t *)utf8, utf8_len, offset,
                                         &byte_len);
        if (codepoint <= 0xffffu) {
            if (produced < utf16_capacity) {
                utf16[produced] = (uint16_t)codepoint;
            }
            ++produced;
        } else {
            uint32_t value = codepoint - 0x10000u;
            if (produced < utf16_capacity) {
                utf16[produced] = (uint16_t)(0xd800u | (value >> 10));
            }
            ++produced;
            if (produced < utf16_capacity) {
                utf16[produced] = (uint16_t)(0xdc00u | (value & 0x3ffu));
            }
            ++produced;
        }
        offset += byte_len;
    }
    *out_utf16_len = produced;
    return 0;
}

int text_utf16le_to_utf8(const uint16_t *utf16, uint32_t utf16_len, char *utf8,
                         uint32_t utf8_capacity, uint32_t *out_utf8_len)
{
    uint32_t index = 0u;
    uint32_t produced = 0u;
    if (!out_utf8_len) {
        return -22;
    }
    *out_utf8_len = 0u;
    if (!utf16 || (utf8_capacity != 0u && !utf8)) {
        return -22;
    }
    while (index < utf16_len) {
        uint16_t unit = utf16[index];
        uint32_t codepoint;
        if (unit >= 0xd800u && unit <= 0xdbffu) {
            if (index + 1u < utf16_len && utf16[index + 1u] >= 0xdc00u &&
                utf16[index + 1u] <= 0xdfffu) {
                codepoint = 0x10000u +
                    ((((uint32_t)unit - 0xd800u) << 10) |
                     ((uint32_t)utf16[index + 1u] - 0xdc00u));
                ++index;
            } else {
                codepoint = UTF8_REPLACEMENT;
            }
        } else if (unit >= 0xdc00u && unit <= 0xdfffu) {
            codepoint = UTF8_REPLACEMENT;
        } else {
            codepoint = unit;
        }
        produced += utf8_encode(codepoint, utf8, utf8_capacity, produced);
        ++index;
    }
    if (produced < utf8_capacity) {
        utf8[produced] = 0;
    }
    *out_utf8_len = produced;
    return 0;
}
