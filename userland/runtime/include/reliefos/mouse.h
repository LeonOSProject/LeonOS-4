#ifndef RELIEFOS_MOUSE_H
#define RELIEFOS_MOUSE_H

#include <reliefos/gui.h>

int reliefos_mouse_hide(uint32_t window_id);
int reliefos_mouse_show(uint32_t window_id);
int reliefos_mouse_is_visible(void);
int reliefos_mouse_set_position(uint32_t window_id, int32_t x, int32_t y);
int reliefos_mouse_set_style(uint32_t window_id, uint32_t style);
int reliefos_mouse_set_auto(uint32_t window_id);
int reliefos_mouse_set_region(const struct reliefos_gui_cursor_region_request *region);
int reliefos_mouse_clear_regions(uint32_t window_id);
int reliefos_mouse_get_state(struct reliefos_mouse_state *state);
int reliefos_mouse_get_position(int32_t *x, int32_t *y);

#endif
