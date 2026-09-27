/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/auth.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_AUTH_H
#define LEONOS_AUTH_H
#include <reliefos/auth.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_AUTH_HOME_LEN RELIEFOS_AUTH_HOME_LEN
#define LEONOS_AUTH_PASSWORD_LEN RELIEFOS_AUTH_PASSWORD_LEN
#define LEONOS_AUTH_PASSWORD_MAX_CHARS RELIEFOS_AUTH_PASSWORD_MAX_CHARS
#define LEONOS_AUTH_PASSWORD_MIN_CHARS RELIEFOS_AUTH_PASSWORD_MIN_CHARS
#define LEONOS_AUTH_ROLE_ADMIN RELIEFOS_AUTH_ROLE_ADMIN
#define LEONOS_AUTH_ROLE_NONE RELIEFOS_AUTH_ROLE_NONE
#define LEONOS_AUTH_ROLE_USER RELIEFOS_AUTH_ROLE_USER
#define LEONOS_AUTH_UPDATE_FLAGS RELIEFOS_AUTH_UPDATE_FLAGS
#define LEONOS_AUTH_UPDATE_ROLE RELIEFOS_AUTH_UPDATE_ROLE
#define LEONOS_AUTH_USERNAME_LEN RELIEFOS_AUTH_USERNAME_LEN
#define LEONOS_AUTH_USER_DISABLED RELIEFOS_AUTH_USER_DISABLED
#define LEONOS_UAPI_AUTH_ABI_H RELIEFOS_UAPI_AUTH_ABI_H
#define LEONOS_UAPI_AUTH_USER_H RELIEFOS_UAPI_AUTH_USER_H
#define leonos_auth_create_user reliefos_auth_create_user
#define leonos_auth_current reliefos_auth_current
#define leonos_auth_list_users reliefos_auth_list_users
#define leonos_auth_logout reliefos_auth_logout
#define leonos_auth_password_valid reliefos_auth_password_valid
#define leonos_auth_request_power reliefos_auth_request_power
#define leonos_auth_status reliefos_auth_status
#define leonos_auth_update_user reliefos_auth_update_user
#define leonos_auth_users_alloc reliefos_auth_users_alloc
#define leonos_user_info reliefos_user_info
#endif /* LEONOS_AUTH_H */
