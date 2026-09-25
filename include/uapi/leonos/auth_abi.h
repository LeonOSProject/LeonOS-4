#ifndef LEONOS_UAPI_AUTH_ABI_H
#define LEONOS_UAPI_AUTH_ABI_H
/*
 * Authentication wire ABI between ntclks and userland. Userland wrappers live
 * in <leonos/auth.h>. UAPI only: nothing here may include a non-UAPI header.
 */

#include <stdint.h>
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

#endif /* LEONOS_UAPI_AUTH_ABI_H */
