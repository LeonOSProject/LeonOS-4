/* Run in an ordinary root login against the booted disk; never write sectors. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    struct stat boot, root, esp, data;
    unsigned char magic[2];
    int fd = open("/dev/sda1", O_RDONLY);
    if (getuid() || fd < 0 || stat("/boot", &boot) || stat("/", &root) ||
        fstat(fd, &esp) || stat("/dev/sda2", &data) ||
        boot.st_dev != esp.st_rdev || root.st_dev != data.st_rdev ||
        pread(fd, magic, 2, 510) != 2 || magic[0] != 0x55 || magic[1] != 0xaa) {
        perror("block metadata identity/read");
        return 1;
    }
    const char *ordinary_path = "/etc/lsblk-metadata-test";
    unsigned char ordinary[512];
    memset(ordinary, 'Q', sizeof(ordinary));
    ordinary[510] = 'X';
    ordinary[511] = 'Y';
    int fixture = open(ordinary_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fixture < 0 || write(fixture, ordinary, sizeof(ordinary)) != sizeof(ordinary) ||
        fchmod(fixture, 0644) || close(fixture)) return 1;
    pid_t child = fork();
    if (child < 0) return 1;
    if (!child) {
        if (setresuid(1000, 1000, 1000) || pread(fd, magic, 2, 510) != 2 ||
            magic[0] != 0x55 || magic[1] != 0xaa) _exit(2);
        /* An inherited readable fd works; opening a new root-only device
         * must still honor the device's Linux DAC permissions. */
        int denied = open("/dev/sda1", O_RDONLY);
        if (denied >= 0 || errno != EACCES) _exit(3);
        /* Unknown Linux open bits must not become kernel descriptor-kind
         * flags and redirect an ordinary readable file into raw disk I/O. */
        const unsigned int injected[] = {0, 0x02000000u, 0x12000000u};
        for (unsigned i = 0; i < sizeof(injected) / sizeof(injected[0]); ++i) {
            int plain = syscall(SYS_openat, AT_FDCWD, ordinary_path,
                                O_RDONLY | injected[i], 0);
            if (plain < 0 || pread(plain, magic, 2, 510) != 2 ||
                magic[0] != 'X' || magic[1] != 'Y' || lseek(plain, 510, SEEK_SET) != 510 ||
                read(plain, magic, 2) != 2 || magic[0] != 'X' || magic[1] != 'Y') _exit(4 + i);
            close(plain);
        }
        _exit(0);
    }
    int status;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status)) return 1;
    if (WEXITSTATUS(status)) {
        printf("FAIL block metadata: child check %d\n", WEXITSTATUS(status));
        return 1;
    }
    close(fd);
    unlink(ordinary_path);
    puts("PASS block metadata: root read, inherited read after UID drop, open DAC, filtered open flags, stat/mount identity");
    return 0;
}
