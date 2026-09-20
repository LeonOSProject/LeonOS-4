#ifndef LEONOS_AUTH_H
#define LEONOS_AUTH_H

#include <leonos/auth_user.h>

#define LEONOS_AUTH_ROLE_NONE 0U
#define LEONOS_AUTH_ROLE_USER 1U
#define LEONOS_AUTH_ROLE_ADMIN 2U

#define LEONOS_AUTH_USER_DISABLED 0x00000001U

#define LEONOS_AUTH_UPDATE_ROLE 0x00000001U
#define LEONOS_AUTH_UPDATE_FLAGS 0x00000002U

struct leonos_auth_status {
    uint32_t user_count;
    uint32_t has_admin;
    uint32_t reserved0;
    uint32_t reserved1;
};

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
