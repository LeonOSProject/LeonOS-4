#include "../../userland/apps/installer/installer_setup.c"

int main(int argc, char **argv)
{
    if (argc != 2) return 255;
    struct leonos_user_info user = {.uid = getuid(), .home = "/home/test"};
    return prepare_hyfetch_config(argv[1], &user) < 0 ? errno : 0;
}
