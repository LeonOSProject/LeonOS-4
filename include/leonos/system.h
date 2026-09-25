#ifndef LEONOS_SYSTEM_H
#define LEONOS_SYSTEM_H

/*
 * Userland system client API. The wire types and constants moved to the
 * kernel UAPI (<leonos/system_abi.h>); this header re-exports them so existing
 * `#include <leonos/system.h>` callers keep working.
 */
#include <leonos/system_abi.h>
#include <leonos/kernel_debug.h>
#include <stdint.h>

int leonos_system_info(struct leonos_system_info *info);
int leonos_perf_info(struct leonos_perf_info *info);
int leonos_task_affinity_get(uint32_t pid, uint64_t *mask);
int leonos_task_affinity_set(uint32_t pid, uint64_t mask);
int leonos_time_info(struct leonos_time_info *info);
int leonos_time_ntp_sync(uint32_t timeout_ms, struct leonos_time_sync *result);
int leonos_machine_identity(struct leonos_machine_identity *identity);
int leonos_system_reboot(void);
int leonos_system_shutdown(void);

#endif
