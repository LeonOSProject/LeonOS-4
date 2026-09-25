#ifndef LEONOS_AUDIO_H
#define LEONOS_AUDIO_H

/*
 * Userland audio API. The wire types and constants moved to the kernel UAPI
 * (<leonos/audio_abi.h>); this header re-exports them so existing
 * `#include <leonos/audio.h>` callers keep working.
 */
#include <leonos/audio_abi.h>
#include <stdint.h>

int leonos_audio_configure(const struct leonos_audio_format *format);
long leonos_audio_write(const void *data, uint32_t length,
                        uint32_t *out_status);
int leonos_audio_get_state(struct leonos_audio_state *state);

#endif
