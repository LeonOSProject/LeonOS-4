#ifndef RELIEFOS_STARTUP_H
#define RELIEFOS_STARTUP_H

/*
 * Userland startup-approval client API. The wire types and constants moved to
 * the kernel UAPI (<reliefos/startup_abi.h>); this header re-exports them so
 * existing `#include <reliefos/startup.h>` callers keep working.
 */
#include <reliefos/startup_abi.h>
#include <stdint.h>

int reliefos_startup_request(const struct reliefos_startup_command *command,
                           uint32_t *out_request_id);
int reliefos_startup_request_status(uint32_t request_id, uint32_t *out_status);
int reliefos_startup_dialog_get(struct reliefos_startup_dialog_request *request);
int reliefos_startup_dialog_resolve(uint32_t request_id, uint32_t decision);
int reliefos_startup_list(uint32_t uid, struct reliefos_startup_entry *entries,
                        uint32_t capacity, uint32_t *out_count);
int reliefos_startup_set_enabled(uint32_t uid, uint32_t entry_id, uint32_t enabled);
int reliefos_startup_remove(uint32_t uid, uint32_t entry_id);
int reliefos_startup_launch_current_user(void);

#endif
