/* TrueType face extraction and PSF ASCII outlines for the Win95 UI font.
 * Tables not involved in the ASCII replacement are retained byte-for-byte. */
#include "tools/host/common/io.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIMIT (20u * 1024u * 1024u)
struct table {
  unsigned char tag[4];
  struct byte_buffer bytes;
};
struct font {
  unsigned char header[12];
  struct table *tables;
  size_t count;
};
static void fail(const char *why) {
  fprintf(stderr, "leonos-font: %s\n", why);
  exit(1);
}
static unsigned u16(const unsigned char *p) {
  return (unsigned)p[0] * 256u + p[1];
}
static uint32_t u32(const unsigned char *p) {
  return (uint32_t)u16(p) * 65536u + u16(p + 2);
}
static void p16(unsigned char *p, unsigned v) {
  p[0] = (unsigned char)(v >> 8);
  p[1] = (unsigned char)v;
}
static void p32(unsigned char *p, uint32_t v) {
  p16(p, v >> 16);
  p16(p + 2, v & 65535u);
}
static void append(struct byte_buffer *b, const void *p, size_t n) {
  if (n > LIMIT || b->len > LIMIT - n || buffer_reserve(b, b->len + n))
    fail("font exceeds size limit or allocation failed");
  if (n)
    memcpy(b->data + b->len, p, n);
  b->len += n;
}
static void word(struct byte_buffer *b, unsigned v) {
  unsigned char p[2];
  p16(p, v);
  append(b, p, 2);
}
static void dword(struct byte_buffer *b, uint32_t v) {
  unsigned char p[4];
  p32(p, v);
  append(b, p, 4);
}
static void align4(struct byte_buffer *b) {
  static const char zero[4] = {0};
  append(b, zero, (4 - b->len % 4) % 4);
}
static uint32_t checksum(const struct byte_buffer *b) {
  uint32_t sum = 0;
  for (size_t i = 0; i < b->len; i += 4) {
    uint32_t v = 0;
    for (size_t j = 0; j < 4; ++j)
      v = (v << 8) + (i + j < b->len ? b->data[i + j] : 0);
    sum += v;
  }
  return sum;
}
static struct byte_buffer *table(struct font *f, const char *tag) {
  for (size_t i = 0; i < f->count; ++i)
    if (!memcmp(f->tables[i].tag, tag, 4))
      return &f->tables[i].bytes;
  fail("missing required TrueType table");
  return NULL;
}
static void parse(struct font *f, const struct byte_buffer *input) {
  const unsigned char *d = input->data;
  size_t off = 0;
  if (input->len < 12)
    fail("truncated font");
  if (!memcmp(d, "ttcf", 4)) {
    if (input->len < 16 || !u32(d + 8) || u32(d + 8) > (input->len - 12) / 4)
      fail("invalid TTC header");
    off = u32(d + 12);
  }
  if (off > input->len - 12 || u32(d + off) != 0x10000)
    fail("not a TrueType outline font");
  f->count = u16(d + off + 4);
  if (!f->count || f->count > (input->len - off - 12) / 16)
    fail("truncated font directory");
  memcpy(f->header, d + off, 12);
  f->tables = calloc(f->count, sizeof(*f->tables));
  if (!f->tables)
    fail("allocation failed");
  for (size_t i = 0; i < f->count; ++i) {
    const unsigned char *r = d + off + 12 + i * 16;
    size_t pos = u32(r + 8), n = u32(r + 12);
    if (pos > input->len || n > input->len - pos)
      fail("truncated table");
    for (size_t j = 0; j < i; ++j)
      if (!memcmp(f->tables[j].tag, r, 4))
        fail("duplicate table");
    memcpy(f->tables[i].tag, r, 4);
    append(&f->tables[i].bytes, d + pos, n);
  }
}
static struct byte_buffer serialize(struct font *f) {
  struct byte_buffer out = {0};
  struct byte_buffer *head = table(f, "head");
  if (head->len < 54)
    fail("short head table");
  memset(head->data + 8, 0, 4);
  append(&out, f->header, 12);
  unsigned char zero[16] = {0};
  for (size_t i = 0; i < f->count; ++i)
    append(&out, zero, 16);
  size_t head_offset = 0;
  for (size_t i = 0; i < f->count; ++i) {
    align4(&out);
    size_t pos = out.len;
    struct table *t = &f->tables[i];
    append(&out, t->bytes.data, t->bytes.len);
    unsigned char *r = out.data + 12 + i * 16;
    memcpy(r, t->tag, 4);
    p32(r + 4, checksum(&t->bytes));
    p32(r + 8, (uint32_t)pos);
    p32(r + 12, (uint32_t)t->bytes.len);
    if (!memcmp(t->tag, "head", 4))
      head_offset = pos;
  }
  p32(out.data + head_offset + 8, 0xb1b0afba - checksum(&out));
  return out;
}
static unsigned glyph_id(const struct byte_buffer *cmap, unsigned cp) {
  const unsigned char *d = cmap->data;
  if (cmap->len < 4 || u16(d + 2) > (cmap->len - 4) / 8)
    fail("invalid cmap");
  for (unsigned r = 0; r < u16(d + 2); ++r) {
    const unsigned char *record = d + 4 + r * 8;
    size_t off = u32(record + 4);
    if (u16(record) != 3 || u16(record + 2) != 1 || off > cmap->len - 2 ||
        u16(d + off) != 4)
      continue;
    const unsigned char *s = d + off;
    size_t avail = cmap->len - off;
    if (avail < 16)
      fail("short cmap format 4");
    unsigned len = u16(s + 2), count = u16(s + 6) / 2;
    size_t start = 16 + count * 2, delta = start + count * 2,
           range = delta + count * 2;
    if (len > avail || range + count * 2 > len)
      fail("invalid cmap segments");
    for (unsigned i = 0; i < count; ++i) {
      unsigned end = u16(s + 14 + i * 2), first = u16(s + start + i * 2);
      if (cp > end)
        continue;
      if (cp < first)
        return 0;
      unsigned change = u16(s + delta + i * 2), offset = u16(s + range + i * 2);
      if (!offset)
        return (cp + change) & 65535u;
      size_t p = range + i * 2 + offset + (cp - first) * 2;
      if (p + 2 > len)
        fail("invalid cmap glyph offset");
      unsigned g = u16(s + p);
      return g ? (g + change) & 65535u : 0;
    }
    return 0;
  }
  fail("missing Windows Unicode cmap");
  return 0;
}
static struct byte_buffer pixel(const unsigned char *bitmap, int asc, int desc,
                                int advance) {
  int xs[512], ys[512];
  unsigned points = 0;
  for (int row = 0; row < 16; ++row)
    for (int col = 0; col < 8; ++col)
      if (bitmap[row] & (0x80 >> col)) {
        int x0 = col * advance / 8, x1 = (col + 1) * advance / 8;
        int y0 = desc + (15 - row) * (asc - desc) / 16,
            y1 = desc + (16 - row) * (asc - desc) / 16;
        xs[points] = x0;
        ys[points++] = y0;
        xs[points] = x1;
        ys[points++] = y0;
        xs[points] = x1;
        ys[points++] = y1;
        xs[points] = x0;
        ys[points++] = y1;
      }
  struct byte_buffer b = {0};
  word(&b, points / 4);
  word(&b, 0);
  word(&b, (unsigned)desc);
  word(&b, points ? (unsigned)advance : 0);
  word(&b, (unsigned)asc);
  if (!points)
    return b;
  for (unsigned i = 0; i < points / 4; ++i)
    word(&b, (i + 1) * 4 - 1);
  word(&b, 0);
  const unsigned char flag = 1;
  for (unsigned i = 0; i < points; ++i)
    append(&b, &flag, 1);
  int prev = 0;
  for (unsigned i = 0; i < points; ++i) {
    word(&b, (unsigned)(xs[i] - prev));
    prev = xs[i];
  }
  prev = 0;
  for (unsigned i = 0; i < points; ++i) {
    word(&b, (unsigned)(ys[i] - prev));
    prev = ys[i];
  }
  align4(&b);
  return b;
}
static void replace_ascii(struct font *f, const struct byte_buffer *psf) {
  struct byte_buffer *head = table(f, "head"), *hhea = table(f, "hhea"),
                     *maxp = table(f, "maxp"), *loca = table(f, "loca"),
                     *glyf = table(f, "glyf"), *hmtx = table(f, "hmtx"),
                     *cmap = table(f, "cmap");
  if (head->len < 54 || hhea->len < 36 || maxp->len < 6 ||
      u16(head->data + 50) != 1)
    fail("invalid long-loca metrics");
  unsigned count = u16(maxp->data + 4), metrics = u16(hhea->data + 34);
  int asc = (int16_t)u16(hhea->data + 4), desc = (int16_t)u16(hhea->data + 6),
      advance = (asc - desc) / 2;
  if (!count || !metrics || asc <= desc || hmtx->len < metrics * 4u ||
      loca->len < (count + 1u) * 4u)
    fail("invalid metrics");
  if (psf->len < 4100 || psf->data[0] != 0x36 || psf->data[1] != 4 ||
      psf->data[3] != 16)
    fail("invalid PSF");
  unsigned ids[95];
  for (unsigned cp = 32; cp <= 126; ++cp) {
    unsigned id = glyph_id(cmap, cp);
    if (!id || id >= count || id >= metrics)
      fail("ASCII glyph lacks metrics");
    ids[cp - 32] = id;
    p16(hmtx->data + id * 4, (unsigned)advance);
    p16(hmtx->data + id * 4 + 2, 0);
  }
  struct byte_buffer new_glyf = {0}, new_loca = {0};
  for (unsigned id = 0; id < count; ++id) {
    size_t first = u32(loca->data + id * 4),
           last = u32(loca->data + (id + 1) * 4);
    if (first > last || last > glyf->len)
      fail("invalid glyph extent");
    dword(&new_loca, (uint32_t)new_glyf.len);
    unsigned cp = 127;
    for (unsigned i = 0; i < 95; ++i)
      if (ids[i] == id)
        cp = i + 32;
    if (cp < 127) {
      struct byte_buffer b = pixel(psf->data + 4 + cp * 16, asc, desc, advance);
      append(&new_glyf, b.data, b.len);
      buffer_destroy(&b);
    } else
      append(&new_glyf, glyf->data + first, last - first);
    align4(&new_glyf);
  }
  dword(&new_loca, (uint32_t)new_glyf.len);
  buffer_destroy(glyf);
  buffer_destroy(loca);
  *glyf = new_glyf;
  *loca = new_loca;
}
int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: leonos-font SOURCE PSF METRO WIN95\n");
    return 2;
  }
  struct byte_buffer source = {0}, psf = {0};
  struct font f = {0};
  if (read_file_all(argv[1], &source) || read_file_all(argv[2], &psf))
    fail("cannot read input");
  parse(&f, &source);
  struct byte_buffer metro = {0};
  if (!memcmp(source.data, "ttcf", 4))
    metro = serialize(&f);
  else
    append(&metro, source.data, source.len);
  replace_ascii(&f, &psf);
  struct byte_buffer win95 = serialize(&f);
  /* Validate and render both faces before publishing either one. */
  if (write_file_if_changed(argv[3], metro.data, metro.len, 0644) ||
      write_file_if_changed(argv[4], win95.data, win95.len, 0644))
    fail("cannot publish font");
  buffer_destroy(&metro);
  buffer_destroy(&win95);
  for (size_t i = 0; i < f.count; ++i)
    buffer_destroy(&f.tables[i].bytes);
  free(f.tables);
  buffer_destroy(&source);
  buffer_destroy(&psf);
  return 0;
}
