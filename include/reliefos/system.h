#ifndef RELIEFOS_SYSTEM_H
#define RELIEFOS_SYSTEM_H

/*
 * Userland system client API. The wire types and constants moved to the
 * kernel UAPI (<reliefos/system_abi.h>); this header re-exports them so existing
 * `#include <reliefos/system.h>` callers keep working.
 */
#include <reliefos/system_abi.h>
#include <reliefos/kernel_debug.h>
#include <stdint.h>

int reliefos_system_info(struct reliefos_system_info *info);
int reliefos_perf_info(struct reliefos_perf_info *info);
int reliefos_task_affinity_get(uint32_t pid, uint64_t *mask);
int reliefos_task_affinity_set(uint32_t pid, uint64_t mask);
int reliefos_time_info(struct reliefos_time_info *info);
int reliefos_time_ntp_sync(uint32_t timeout_ms, struct reliefos_time_sync *result);
int reliefos_machine_identity(struct reliefos_machine_identity *identity);
int reliefos_system_reboot(void);
int reliefos_system_shutdown(void);

#endif
