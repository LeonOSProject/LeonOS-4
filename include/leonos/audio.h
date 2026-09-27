/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/audio.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_AUDIO_H
#define LEONOS_AUDIO_H
#include <reliefos/audio.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_AUDIO_IO_SLICE_BYTES RELIEFOS_AUDIO_IO_SLICE_BYTES
#define LEONOS_AUDIO_MAX_WRITE RELIEFOS_AUDIO_MAX_WRITE
#define LEONOS_AUDIO_STATUS_BAD_FORMAT RELIEFOS_AUDIO_STATUS_BAD_FORMAT
#define LEONOS_AUDIO_STATUS_NO_DEVICE RELIEFOS_AUDIO_STATUS_NO_DEVICE
#define LEONOS_AUDIO_STATUS_OK RELIEFOS_AUDIO_STATUS_OK
#define LEONOS_AUDIO_STATUS_PLAYBACK_FAILED RELIEFOS_AUDIO_STATUS_PLAYBACK_FAILED
#define LEONOS_AUDIO_STATUS_WOULD_BLOCK RELIEFOS_AUDIO_STATUS_WOULD_BLOCK
#define LEONOS_UAPI_AUDIO_ABI_H RELIEFOS_UAPI_AUDIO_ABI_H
#define leonos_audio_configure reliefos_audio_configure
#define leonos_audio_format reliefos_audio_format
#define leonos_audio_get_state reliefos_audio_get_state
#define leonos_audio_state reliefos_audio_state
#define leonos_audio_write reliefos_audio_write
#endif /* LEONOS_AUDIO_H */
