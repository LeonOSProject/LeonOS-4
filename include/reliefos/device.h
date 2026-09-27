#ifndef RELIEFOS_DEVICE_H
#define RELIEFOS_DEVICE_H

/*
 * Userland device API. The wire types and constants moved to the kernel UAPI
 * (<reliefos/device_abi.h>); this header re-exports them so existing
 * `#include <reliefos/device.h>` callers keep working.
 */
#include <reliefos/device_abi.h>
#include <stdint.h>

int reliefos_device_list(struct reliefos_device_info *devices,
                       uint32_t capacity, uint32_t *out_count);

#endif
