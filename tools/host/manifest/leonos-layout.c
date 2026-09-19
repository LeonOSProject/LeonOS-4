#include "include/uapi/leonos/rootfs.h"
#include <stdio.h>
int main(void) {
#define DIRECTORY(path, mode)                                                  \
    printf("d\t-\t%s\t%04o\trootfs-layout\toverride\n", path, mode);
    LEONOS_ROOTFS_DIRECTORIES(DIRECTORY)
#undef DIRECTORY
#define LINK(path, target)                                                     \
    printf("l\t%s\t%s\t0777\trootfs-layout\tunique\n", target, path);
    LEONOS_ROOTFS_SYMLINKS(LINK)
#undef LINK
    return ferror(stdout) ? 1 : 0;
}
