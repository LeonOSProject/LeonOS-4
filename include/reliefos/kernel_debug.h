/*
 * ReliefOS kernel-debug control ABI.
 * Lets trusted desktop applications arm the next Ring-0 diagnostic boot.
 */
#ifndef RELIEFOS_KERNEL_DEBUG_H
#define RELIEFOS_KERNEL_DEBUG_H

/*
 * Userland kernel-debug control API. The wire types and constants moved to the
 * kernel UAPI (<reliefos/kernel_debug_abi.h>); this header re-exports them so
 * existing `#include <reliefos/kernel_debug.h>` callers keep working.
 */
#include <reliefos/kernel_debug_abi.h>
#include <stdint.h>

int reliefos_kernel_debug_get_state(uint32_t *flags);
int reliefos_kernel_debug_set_enabled(int enabled);
int reliefos_kernel_debug_arm_next_boot(void);
int reliefos_kernel_debug_clear(void);

#endif
