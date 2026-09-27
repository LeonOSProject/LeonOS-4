#ifndef RELIEFOS_FB_H
#define RELIEFOS_FB_H

#include <stdint.h>

#define RELIEFOS_FB_CAP_MODE_SET 0x0001U
#define RELIEFOS_FB_BACKEND_BOOT 0U
#define RELIEFOS_FB_BACKEND_BOCHS_VBE 1U
#define RELIEFOS_FB_BACKEND_VMWARE_SVGA 2U

/* /dev/fb0 hardware limits; fbdev's visible geometry is not a mode limit. */
#define RELIEFOS_FBIOGET_CAPABILITIES 0x46f0UL
#define RELIEFOS_FBIOUPDATE_REGION 0x46f1UL
#define RELIEFOS_FBIOBLIT 0x46f2UL

/* Atomic presentation by the active graphical controlling VT. Inactive callers
 * receive EAGAIN. pixels == 0 fills color; otherwise stride counts RGB32 pixels. */
struct reliefos_fb_present {
    uint32_t x, y, width, height, stride, color;
    uint64_t pixels;
};

struct reliefos_fb_capabilities {
    uint8_t bytes_per_pixel;
    uint8_t reserved;
    uint16_t capabilities;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_bytes;
    uint32_t backend;
};

#endif
