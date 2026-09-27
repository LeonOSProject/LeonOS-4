#ifndef RELIEFOS_PNG_H
#define RELIEFOS_PNG_H

#include <stdint.h>

/* PNG decoding stays bounded so a malformed or oversized image cannot turn a
 * document preview into an unbounded allocation. */
#define RELIEFOS_PNG_MAX_PIXELS (1024U * 1024U)
#define RELIEFOS_PNG_MAX_FILE_BYTES (16U * 1024U * 1024U)

/*
 * Decode a PNG file into ReliefOS UI pixels (0x00RRGGBB).  Transparency is
 * composited on white.  On success the caller owns *out_pixels and releases
 * it with reliefos_png_free().
 */
int reliefos_png_decode_file(const char *path, uint32_t **out_pixels,
                           uint32_t *out_width, uint32_t *out_height);
void reliefos_png_free(uint32_t *pixels);

#endif
