#ifndef RELIEFOS_PGL_H
#define RELIEFOS_PGL_H

/* The system library and SDK use one fixed framebuffer ABI. */
#ifndef PGL_ABGR32
#define PGL_ABGR32
#endif
#ifndef PGL_D24S8
#define PGL_D24S8
#endif
#ifndef PGL_SMALL_MEM
#define PGL_SMALL_MEM
#endif

#include <portablegl.h>
#include <reliefos/gui.h>

#define RELIEFOS_PGL_EINVAL (-22)
#define RELIEFOS_PGL_ENOMEM (-12)
#define RELIEFOS_PGL_ENOSYS (-38)

typedef struct reliefos_pgl_context reliefos_pgl_context;

/* Return values from reliefos_pgl_process_event. */
#define RELIEFOS_PGL_EVENT_NONE 0
#define RELIEFOS_PGL_EVENT_CLOSE 1
#define RELIEFOS_PGL_EVENT_RESIZED 2

reliefos_pgl_context *reliefos_pgl_create(
    int width, int height, const char *title);

void reliefos_pgl_destroy(reliefos_pgl_context *ctx);

int reliefos_pgl_resize(
    reliefos_pgl_context *ctx, int width, int height);

int reliefos_pgl_present(reliefos_pgl_context *ctx);

int reliefos_pgl_window_id(
    const reliefos_pgl_context *ctx);

int reliefos_pgl_process_event(
    reliefos_pgl_context *ctx,
    const struct reliefos_gui_app_event *event);

void reliefos_pgl_make_current(
    reliefos_pgl_context *ctx);

glContext *reliefos_pgl_native_context(
    reliefos_pgl_context *ctx);

#endif
