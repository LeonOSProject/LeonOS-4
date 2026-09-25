/*
 * LeonOS kernel-debug control ABI.
 * Lets trusted desktop applications arm the next Ring-0 diagnostic boot.
 */
#ifndef LEONOS_KERNEL_DEBUG_H
#define LEONOS_KERNEL_DEBUG_H

/*
 * Userland kernel-debug control API. The wire types and constants moved to the
 * kernel UAPI (<leonos/kernel_debug_abi.h>); this header re-exports them so
 * existing `#include <leonos/kernel_debug.h>` callers keep working.
 */
#include <leonos/kernel_debug_abi.h>
#include <stdint.h>

int leonos_kernel_debug_get_state(uint32_t *flags);
int leonos_kernel_debug_set_enabled(int enabled);
int leonos_kernel_debug_arm_next_boot(void);
int leonos_kernel_debug_clear(void);

#endif
