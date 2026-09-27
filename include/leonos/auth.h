#ifndef LEONOS_AUTH_H
#define LEONOS_AUTH_H

/*
 * Userland authentication client API. The wire types and constants moved to
 * the kernel UAPI (<leonos/auth_abi.h>); this header re-exports them so
 * existing `#include <leonos/auth.h>` callers keep working.
 */
#include <leonos/auth_abi.h>

/* Between 1 and 32 UTF-8 characters, with no whitespace. */
int leonos_auth_password_valid(const char *password, uint32_t capacity);
int leonos_auth_status(struct leonos_auth_status *status);
int leonos_auth_current(struct leonos_user_info *user);
int leonos_auth_list_users(struct leonos_user_info *users, uint32_t capacity,
                           uint32_t include_disabled, uint32_t *out_count);
int leonos_auth_users_alloc(struct leonos_user_info **users, uint32_t include_disabled,
                            uint32_t *out_count);
int leonos_auth_logout(void);
int leonos_auth_create_user(const char *username, const char *password,
                            uint32_t role, struct leonos_user_info *user);
int leonos_auth_update_user(uint32_t uid, uint32_t mask, uint32_t role,
                            uint32_t flags);
int leonos_auth_request_power(uint32_t command);

#endif
