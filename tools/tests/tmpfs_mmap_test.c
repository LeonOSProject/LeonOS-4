/* Identical Linux/musl and LeonOS tests; no LeonOS headers or libc adapters. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned failures;
static void check(int ok, const char *label)
{
    printf("[storage-upstream] %s tmpfs mmap: %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
}
static void bus_handler(int signal, siginfo_t *info, void *context)
{
    (void)context;
    _exit(signal == SIGBUS && info->si_code == BUS_ADRERR ? 0 : 90);
}
static int expect_bus(volatile char *address)
{
    pid_t child = fork();
    if (!child) {
        struct sigaction sa = {.sa_sigaction = bus_handler, .sa_flags = SA_SIGINFO};
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGBUS, &sa, NULL))
            _exit(91);
        volatile char value = *address;
        (void)value;
        _exit(92);
    }
    int status;
    return child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
unsigned test_tmpfs_mmap(const char *directory)
{
    failures = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/mmap-XXXXXX", directory);
    int fd = mkstemp(path);
    check(fd >= 0 && ftruncate(fd, 8192) == 0, "create sparse file");
    if (fd < 0)
        return failures;
    volatile char *a = mmap(NULL, 12288, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    volatile char *b = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    volatile char *p = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    check(a != MAP_FAILED && b != MAP_FAILED && p != MAP_FAILED, "shared and private mappings");
    if (a == MAP_FAILED || b == MAP_FAILED || p == MAP_FAILED)
        goto cleanup;
    check(a[0] == 0 && b[4096] == 0 && p[0] == 0, "sparse pages are zero");
    char byte = 0;
    a[0] = 'A';
    check(b[0] == 'A' && p[0] == 'A' && pread(fd, &byte, 1, 0) == 1 && byte == 'A',
          "mapped write visible to read and other mappings");
    check(pwrite(fd, "B", 1, 0) == 1 && a[0] == 'B' && p[0] == 'B', "file write visible before private COW");
    p[0] = 'P';
    p[4096] = 'Q';
    check(a[0] == 'B' && a[4096] == 0 && p[0] == 'P', "private writes remain private");
    pid_t child = fork();
    if (!child) {
        b[1] = 'F';
        p[0] = 'C';
        _exit(0);
    }
    int status = 0;
    check(child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status) &&
              a[1] == 'F' && p[0] == 'P',
          "fork sharing and private COW");
    check(msync((void *)a, 8192, MS_SYNC) == 0 && msync((void *)a, 8192, MS_ASYNC) == 0 &&
              msync((void *)a, 8192, 0) == 0 && msync((void *)a, 0, MS_SYNC) == 0 &&
              msync((void *)p, 8192, MS_SYNC | MS_INVALIDATE) == 0,
          "msync Linux flag and zero-length behavior");
    check(msync((void *)(a + 1), 4096, MS_SYNC) < 0 && errno == EINVAL &&
              msync((void *)a, 4096, MS_SYNC | MS_ASYNC) < 0 && errno == EINVAL,
          "msync invalid arguments rejected");
    check(expect_bus(a + 8192), "access beyond EOF signals SIGBUS with BUS_ADRERR");
    check(ftruncate(fd, 17) == 0 && a[16] == 0 && a[17] == 0 && expect_bus(a + 4096) && expect_bus(p + 4096),
          "truncate invalidates shared and private pages");
    check(ftruncate(fd, 12288) == 0 && a[4096] == 0 && p[4096] == 0 && a[8192] == 0,
          "regrowth faults in zeroed pages through existing mappings");
    a[8192] = 'G';
    check(pread(fd, &byte, 1, 8192) == 1 && byte == 'G', "grown mapping remains coherent");
    check(ftruncate(fd, 0) == 0 && expect_bus(a) && expect_bus(b) && expect_bus(p) &&
              ftruncate(fd, 4096) == 0 && a[0] == 0 && b[0] == 0 && p[0] == 0,
          "repeated truncate uses inode identity after file size changes");
    int ro = open(path, O_RDONLY);
    void *r = mmap(NULL, 4096, PROT_READ, MAP_SHARED, ro, 0);
    check(ro >= 0 && r != MAP_FAILED && mprotect(r, 4096, PROT_READ | PROT_WRITE) < 0 && errno == EACCES,
          "read-only descriptor cannot gain shared write permission");
    if (r != MAP_FAILED)
        munmap(r, 4096);
    if (ro >= 0)
        close(ro);
    check(unlink(path) == 0 && close(fd) == 0, "unlink and close while mappings live");
    fd = -1;
    a[2] = 'L';
    check(b[2] == 'L', "mapping keeps unlinked inode alive");
    check(munmap((void *)a, 4096) == 0 && munmap((void *)(a + 4096), 8192) == 0,
          "partial munmap preserves and releases inode references");
    a = MAP_FAILED;
cleanup:
    if (a != MAP_FAILED)
        munmap((void *)a, 12288);
    if (b != MAP_FAILED)
        munmap((void *)b, 8192);
    if (p != MAP_FAILED)
        munmap((void *)p, 8192);
    if (fd >= 0)
        close(fd);
    unlink(path);
    return failures;
}
#ifdef TMPFS_MMAP_STANDALONE
int main(void) { return test_tmpfs_mmap("/dev/shm") != 0; }
#endif

unsigned test_tmpfs_mmap_full(int fd)
{
    failures = 0;
    /* The guest has filled the two-page tmpfs quota, then grows a sparse tail. */
    check(ftruncate(fd, 12288) == 0, "grow past physical quota without allocating pages");
    void *p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 8192);
    check(p != MAP_FAILED && expect_bus(p), "quota exhausted page fault signals SIGBUS");
    if (p != MAP_FAILED)
        munmap(p, 4096);
    return failures;
}

unsigned test_tmpfs_mount_mappings(const char *directory)
{
    failures = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/remount-XXXXXX", directory);
    int fd = mkstemp(path);
    if (fd < 0) {
        check(0, "remount fixture");
        return failures;
    }
    check(ftruncate(fd, 4096) == 0 && mount(NULL, directory, NULL, MS_REMOUNT | MS_RDONLY, NULL) < 0 &&
              errno == EBUSY,
          "open writer blocks read-only remount");
    void *p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    check(p != MAP_FAILED, "readable shared mapping retains write capability");
    close(fd);
    check(umount(directory) < 0 && errno == EBUSY, "mapping without fd blocks unmount");
    check(mount(NULL, directory, NULL, MS_REMOUNT | MS_RDONLY, NULL) < 0 && errno == EBUSY,
          "shared mapping blocks read-only remount after fd close");
    if (p != MAP_FAILED)
        munmap(p, 4096);
    fd = open(path, O_RDONLY);
    p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    check(p != MAP_FAILED && mount(NULL, directory, NULL, MS_REMOUNT | MS_RDONLY, NULL) == 0,
          "read-only descriptor and mapping allow read-only remount");
    void *bad = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED, fd, 0);
    check(bad == MAP_FAILED && errno == EACCES, "read-only mapping error precedence");
    if (bad != MAP_FAILED)
        munmap(bad, 4096);
    if (p != MAP_FAILED)
        munmap(p, 4096);
    close(fd);
    check(mount(NULL, directory, NULL, MS_REMOUNT, NULL) == 0 && unlink(path) == 0,
          "restore writable mount after mappings close");
    return failures;
}
