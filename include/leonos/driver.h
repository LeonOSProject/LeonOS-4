#ifndef LEONOS_DRIVER_H
#define LEONOS_DRIVER_H

/*
 * Userland driver-control client API. The control wire types and constants
 * live in the kernel UAPI (<leonos/driver_abi.h>); this header re-exports
 * them so existing callers keep working.
 *
 * The Ring-0 driver module API (module magic/ABI version,
 * struct leonos_driver_module / leonos_driver_kernel_api and the per-device
 * ops/state structs) belongs to the kernel module domain and lives in the
 * kernel repository's own copy of this header
 * (kernel/ntclks/include/leonos/driver.h).
 */
#include <stdint.h>
#include <leonos/driver_abi.h>

/*
 * Driver table capacity for the runtime control clients (the driver
 * managers size their arrays with it). Kept here: it is runtime sizing,
 * not part of the Ring-0 module ABI.
 */
#define LEONOS_DRIVER_MAX 16U

int leonos_driver_list(struct leonos_driver_info *drivers, uint32_t capacity,
                       uint32_t *out_count);
int leonos_driver_control(uint32_t action, const char *file);

#endif
