#ifndef LEONOS_DEVICE_H
#define LEONOS_DEVICE_H

/*
 * Userland device API. The wire types and constants moved to the kernel UAPI
 * (<leonos/device_abi.h>); this header re-exports them so existing
 * `#include <leonos/device.h>` callers keep working.
 */
#include <leonos/device_abi.h>
#include <stdint.h>

int leonos_device_list(struct leonos_device_info *devices,
                       uint32_t capacity, uint32_t *out_count);

#endif
