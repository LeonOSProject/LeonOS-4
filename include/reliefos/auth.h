#ifndef RELIEFOS_AUTH_H
#define RELIEFOS_AUTH_H

/*
 * Userland authentication client API. The wire types and constants moved to
 * the kernel UAPI (<reliefos/auth_abi.h>); this header re-exports them so
 * existing `#include <reliefos/auth.h>` callers keep working.
 */
#include <reliefos/auth_abi.h>

/* Between 1 and 32 UTF-8 characters, with no whitespace. */
int reliefos_auth_password_valid(const char *password, uint32_t capacity);
int reliefos_auth_status(struct reliefos_auth_status *status);
int reliefos_auth_current(struct reliefos_user_info *user);
int reliefos_auth_list_users(struct reliefos_user_info *users, uint32_t capacity,
                           uint32_t include_disabled, uint32_t *out_count);
int reliefos_auth_users_alloc(struct reliefos_user_info **users, uint32_t include_disabled,
                            uint32_t *out_count);
int reliefos_auth_logout(void);
int reliefos_auth_create_user(const char *username, const char *password,
                            uint32_t role, struct reliefos_user_info *user);
int reliefos_auth_update_user(uint32_t uid, uint32_t mask, uint32_t role,
                            uint32_t flags);
int reliefos_auth_request_power(uint32_t command);

#endif
