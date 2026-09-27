#ifndef LEONOS_INPUTM_H
#define LEONOS_INPUTM_H

/*
 * Userland input-method client API. The wire types and constants moved to the
 * kernel UAPI (<leonos/inputm_abi.h>); this header re-exports them so existing
 * `#include <leonos/inputm.h>` callers keep working.
 */
#include <leonos/inputm_abi.h>
#include <stdint.h>

int leonos_inputm_register(const struct leonos_inputm_provider *provider);
int leonos_inputm_unregister(void);
int leonos_inputm_provider_next(struct leonos_inputm_key_event *event);
int leonos_inputm_provider_result(const struct leonos_inputm_result *result);
int leonos_inputm_submit_key(uint32_t window_id, uint8_t keycode, uint8_t pressed);
int leonos_inputm_poll_result(struct leonos_inputm_result *result);
int leonos_inputm_set_active(uint32_t uid, const char *id);
int leonos_inputm_list(uint32_t uid, struct leonos_inputm_provider *providers,
                       uint32_t capacity, uint32_t *out_count);
int leonos_inputm_set_context(const struct leonos_inputm_context *context);
int leonos_inputm_get_state(uint32_t uid, struct leonos_inputm_state *state);
int leonos_inputm_notify_config(uint32_t uid);

/* Called by the GUI event wrapper and the standard editable widgets. */
int leonos_inputm_observe_gui_key(uint32_t window_id, uint8_t *keycode, uint8_t pressed);
int leonos_inputm_poll_gui_commit(uint32_t window_id);
int leonos_inputm_take_text(char *buffer, uint32_t capacity);
int leonos_inputm_take_key(uint8_t *keycode, uint8_t *pressed);

/* GUI and UI-library integration helpers for standard editable controls. */
void leonos_inputm_note_gui_window(uint32_t window_id);
int leonos_inputm_set_current_context(uint32_t flags, int32_t caret_x,
                                      int32_t caret_y, uint32_t caret_w,
                                      uint32_t caret_h);

#endif
