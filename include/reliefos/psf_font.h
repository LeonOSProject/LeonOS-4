#ifndef RELIEFOS_PSF_FONT_H
#define RELIEFOS_PSF_FONT_H

#include <stdint.h>

#include <reliefos/layout.h>

#include <reliefos/lat15_vga16_psf.inc>

#define RELIEFOS_FONT_W 8u
#define RELIEFOS_FONT_H 16u
#define RELIEFOS_SYSTEM_FONT_PATH RELIEFOS_PATH_SYSTEM_FONT

struct reliefos_psf_view {
    const uint8_t *glyphs;
    uint32_t glyph_count;
    uint32_t char_size;
};

static inline int reliefos_psf_view_from_memory(const uint8_t *font, uint32_t len,
                                              struct reliefos_psf_view *out)
{
    uint32_t glyph_count = 256;
    uint32_t char_size = 16;
    uint32_t header_size = 0;
    if (!font || !out) {
        return 0;
    }
    if (len >= 4 && font[0] == 0x36 && font[1] == 0x04) {
        char_size = font[3];
        glyph_count = (font[2] & 0x01u) ? 512u : 256u;
        header_size = 4;
    } else if (len >= 32 &&
               font[0] == 0x72 && font[1] == 0xb5 && font[2] == 0x4a && font[3] == 0x86) {
        header_size = (uint32_t)font[8] |
                      ((uint32_t)font[9] << 8) |
                      ((uint32_t)font[10] << 16) |
                      ((uint32_t)font[11] << 24);
        glyph_count = (uint32_t)font[16] |
                      ((uint32_t)font[17] << 8) |
                      ((uint32_t)font[18] << 16) |
                      ((uint32_t)font[19] << 24);
        char_size = (uint32_t)font[20] |
                    ((uint32_t)font[21] << 8) |
                    ((uint32_t)font[22] << 16) |
                    ((uint32_t)font[23] << 24);
    } else {
        return 0;
    }
    if (char_size == 0 || glyph_count == 0 ||
        header_size > len || (uint64_t)char_size * glyph_count > len - header_size) {
        return 0;
    }
    out->glyphs = font + header_size;
    out->glyph_count = glyph_count;
    out->char_size = char_size;
    return 1;
}

static inline const uint8_t *reliefos_psf_view_glyph(const struct reliefos_psf_view *view,
                                                   char ch)
{
    uint32_t glyph = (uint8_t)ch;
    if (!view || !view->glyphs || view->char_size == 0 || view->glyph_count == 0) {
        return reliefos_lat15_vga16_psf + 4 + (uint32_t)'?' * 16u;
    }
    if (glyph < 32 || glyph >= view->glyph_count) {
        glyph = '?';
    }
    return view->glyphs + glyph * view->char_size;
}

static inline const uint8_t *reliefos_psf_glyph(char ch)
{
    struct reliefos_psf_view view;
    if (!reliefos_psf_view_from_memory(reliefos_lat15_vga16_psf,
                                     reliefos_lat15_vga16_psf_len, &view)) {
        return reliefos_lat15_vga16_psf + 4 + (uint32_t)'?' * 16u;
    }
    return reliefos_psf_view_glyph(&view, ch);
}

#endif
