/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/fb.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_FB_H
#define LEONOS_FB_H
#include <reliefos/fb.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_FBIOBLIT RELIEFOS_FBIOBLIT
#define LEONOS_FBIOGET_CAPABILITIES RELIEFOS_FBIOGET_CAPABILITIES
#define LEONOS_FBIOUPDATE_REGION RELIEFOS_FBIOUPDATE_REGION
#define LEONOS_FB_BACKEND_BOCHS_VBE RELIEFOS_FB_BACKEND_BOCHS_VBE
#define LEONOS_FB_BACKEND_BOOT RELIEFOS_FB_BACKEND_BOOT
#define LEONOS_FB_BACKEND_VMWARE_SVGA RELIEFOS_FB_BACKEND_VMWARE_SVGA
#define LEONOS_FB_CAP_MODE_SET RELIEFOS_FB_CAP_MODE_SET
#define leonos_fb_capabilities reliefos_fb_capabilities
#define leonos_fb_present reliefos_fb_present
#endif /* LEONOS_FB_H */
