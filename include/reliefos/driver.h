#ifndef RELIEFOS_DRIVER_H
#define RELIEFOS_DRIVER_H

/*
 * Userland driver-control client API. The control wire types and constants
 * live in the kernel UAPI (<reliefos/driver_abi.h>); this header re-exports
 * them so existing callers keep working.
 *
 * The Ring-0 driver module API (module magic/ABI version,
 * struct reliefos_driver_module / reliefos_driver_kernel_api and the per-device
 * ops/state structs) belongs to the kernel module domain and lives in the
 * kernel repository's own copy of this header
 * (kernel/reliefnt/include/reliefos/driver.h).
 */
#include <stdint.h>
#include <reliefos/driver_abi.h>

/*
 * Driver table capacity for the runtime control clients (the driver
 * managers size their arrays with it). Kept here: it is runtime sizing,
 * not part of the Ring-0 module ABI.
 */
#define RELIEFOS_DRIVER_MAX 16U

int reliefos_driver_list(struct reliefos_driver_info *drivers, uint32_t capacity,
                       uint32_t *out_count);
int reliefos_driver_control(uint32_t action, const char *file);

#endif
