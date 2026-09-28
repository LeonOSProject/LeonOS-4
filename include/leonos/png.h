/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/png.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_PNG_H
#define LEONOS_PNG_H
#include <reliefos/png.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_PNG_MAX_FILE_BYTES RELIEFOS_PNG_MAX_FILE_BYTES
#define LEONOS_PNG_MAX_PIXELS RELIEFOS_PNG_MAX_PIXELS
#define leonos_png_decode_file reliefos_png_decode_file
#define leonos_png_free reliefos_png_free
#endif /* LEONOS_PNG_H */
