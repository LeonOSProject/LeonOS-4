/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/text.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_TEXT_H
#define LEONOS_TEXT_H
#include <reliefos/text.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_TEXT_ENCODING_GB2312 RELIEFOS_TEXT_ENCODING_GB2312
#define LEONOS_TEXT_ENCODING_GBK RELIEFOS_TEXT_ENCODING_GBK
#define LEONOS_TEXT_ENCODING_INVALID RELIEFOS_TEXT_ENCODING_INVALID
#define LEONOS_TEXT_ENCODING_NO_SPACE RELIEFOS_TEXT_ENCODING_NO_SPACE
#define LEONOS_TEXT_ENCODING_UTF16BE RELIEFOS_TEXT_ENCODING_UTF16BE
#define LEONOS_TEXT_ENCODING_UTF16LE RELIEFOS_TEXT_ENCODING_UTF16LE
#define LEONOS_TEXT_ENCODING_UTF8 RELIEFOS_TEXT_ENCODING_UTF8
#define LEONOS_TEXT_ENCODING_UTF8_BOM RELIEFOS_TEXT_ENCODING_UTF8_BOM
#define LEONOS_TEXT_REPLACEMENT_CHAR RELIEFOS_TEXT_REPLACEMENT_CHAR
#define leonos_text_decode reliefos_text_decode
#define leonos_text_detect_encoding reliefos_text_detect_encoding
#define leonos_text_encode reliefos_text_encode
#define leonos_text_glyph reliefos_text_glyph
#define leonos_text_layout reliefos_text_layout
#define leonos_text_layout_utf8 reliefos_text_layout_utf8
#endif /* LEONOS_TEXT_H */
