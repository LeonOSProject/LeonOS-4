#ifndef LEONOS_INSTALLER_SETUP_H
#define LEONOS_INSTALLER_SETUP_H
#include <leonos/auth_user.h>
#include <stdint.h>

struct installer_setup {
    char username[LEONOS_AUTH_USERNAME_LEN];
    char password[LEONOS_AUTH_PASSWORD_LEN];
    char password_confirm[LEONOS_AUTH_PASSWORD_LEN];
    char root_password[LEONOS_AUTH_PASSWORD_LEN];
    char root_password_confirm[LEONOS_AUTH_PASSWORD_LEN];
};

int installer_setup_valid(const struct installer_setup *setup);
int installer_setup_write(const struct installer_setup *setup, const char *target);
#endif
