#ifndef RELIEFOS_AUDIO_H
#define RELIEFOS_AUDIO_H

/*
 * Userland audio API. The wire types and constants moved to the kernel UAPI
 * (<reliefos/audio_abi.h>); this header re-exports them so existing
 * `#include <reliefos/audio.h>` callers keep working.
 */
#include <reliefos/audio_abi.h>
#include <stdint.h>

int reliefos_audio_configure(const struct reliefos_audio_format *format);
long reliefos_audio_write(const void *data, uint32_t length,
                        uint32_t *out_status);
int reliefos_audio_get_state(struct reliefos_audio_state *state);

#endif
