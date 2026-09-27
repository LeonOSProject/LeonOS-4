#ifndef LEONOS_STARTUP_H
#define LEONOS_STARTUP_H

/*
 * Userland startup-approval client API. The wire types and constants moved to
 * the kernel UAPI (<leonos/startup_abi.h>); this header re-exports them so
 * existing `#include <leonos/startup.h>` callers keep working.
 */
#include <leonos/startup_abi.h>
#include <stdint.h>

int leonos_startup_request(const struct leonos_startup_command *command,
                           uint32_t *out_request_id);
int leonos_startup_request_status(uint32_t request_id, uint32_t *out_status);
int leonos_startup_dialog_get(struct leonos_startup_dialog_request *request);
int leonos_startup_dialog_resolve(uint32_t request_id, uint32_t decision);
int leonos_startup_list(uint32_t uid, struct leonos_startup_entry *entries,
                        uint32_t capacity, uint32_t *out_count);
int leonos_startup_set_enabled(uint32_t uid, uint32_t entry_id, uint32_t enabled);
int leonos_startup_remove(uint32_t uid, uint32_t entry_id);
int leonos_startup_launch_current_user(void);

#endif
