#include "installer_setup.h"
#include "../../auth/standard_accounts.h"
#include <leonos/launch.h>
#include <leonos/layout.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int installer_setup_valid(const struct installer_setup *setup)
{
    if (!leonos_account_name_valid(setup->username, sizeof(setup->username)) ||
        !strcmp(setup->username, "root") || !strcmp(setup->username, "nobody") ||
        !strcmp(setup->username, "wheel")) return 0;
    const char *passwords[] = {setup->password, setup->password_confirm,
                               setup->root_password, setup->root_password_confirm};
    for (unsigned i = 0; i < 4; ++i)
        if (!leonos_auth_password_valid(passwords[i], LEONOS_AUTH_PASSWORD_LEN)) return 0;
    return !strcmp(setup->password, setup->password_confirm) &&
           !strcmp(setup->root_password, setup->root_password_confirm);
}

static int prepare_home(const char *target, const struct leonos_user_info *user)
{
    const char *const directories[] = {"", "/desktop", "/documents", "/downloads"};
    char path[512];
    for (unsigned i = 0; i < 4; ++i) {
        int n = snprintf(path, sizeof(path), "%s%s%s", target, user->home, directories[i]);
        if (n < 0 || (size_t)n >= sizeof(path)) { errno = ENAMETOOLONG; return -1; }
        if (mkdir(path, 0700) < 0 && errno != EEXIST) return -1;
        struct stat st;
        if (lstat(path, &st) < 0) return -1;
        if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
        if (chown(path, user->uid, user->uid) < 0 || chmod(path, 0700) < 0) return -1;
    }
    const char *const applications[] = {"fileman", "terminal", "settings", "run", "taskmgr"};
    char desktop[512], executable[256];
    snprintf(desktop, sizeof(desktop), "%s%s/desktop", target, user->home);
    for (unsigned i = 0; i < sizeof(applications) / sizeof(applications[0]); ++i) {
        snprintf(executable, sizeof(executable), LEONOS_LAYOUT_LEONOS_APPS "/%s/%s.elf",
                 applications[i], applications[i]);
        if (leonos_launch_create_shortcut_in_dir(desktop, executable, path, sizeof(path)) < 0 ||
            chown(path, user->uid, user->uid) < 0 || chmod(path, 0600) < 0) return -1;
    }
    return 0;
}

int installer_setup_write(const struct installer_setup *setup, const char *target)
{
    struct leonos_user_info records[2] = {
        {.uid = 0, .role = 2, .username = "root", .home = "/root"},
        {.uid = 1000, .role = 1},
    };
    char path[512];
    int result = -1;
    if (!installer_setup_valid(setup)) { errno = EINVAL; return -1; }
    if (strlen(target) > 256) { errno = ENAMETOOLONG; return -1; }
    strcpy(records[1].username, setup->username);
    snprintf(records[1].home, sizeof(records[1].home), "/home/%s", setup->username);
    if (leonos_account_legacy_check(target) < 0) goto out;
    for (unsigned i = 0; i < 2; ++i)
        if (prepare_home(target, &records[i]) < 0) goto out;
    if (leonos_account_seed(target, setup->username, setup->password,
                            setup->root_password) < 0) goto out;
    snprintf(path, sizeof(path), "%s/etc/leonos/installed", target);
    FILE *file = fopen(path, "w");
    if (!file) goto out;
    int failed = fputs("installed=1\n", file) == EOF;
    if (fflush(file) || fchmod(fileno(file), 0644) || fsync(fileno(file))) failed = 1;
    if (fclose(file)) failed = 1;
    if (!failed) result = 0;
out:
    explicit_bzero(records, sizeof(records));
    return result;
}
