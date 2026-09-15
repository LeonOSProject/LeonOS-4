#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(x) do { if (!(x)) { printf("[fifo] FAIL line=%d errno=%d: %s\n", __LINE__, errno, #x); ++failures; } } while (0)
static void alarm_handler(int sig) { (void)sig; }
static void child_done(pid_t child)
{
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static void exercise(const char *base)
{
    printf("[fifo] START filesystem=%s inode and nonblocking endpoints\n", base);
    char directory[256];
    snprintf(directory, sizeof(directory), "%s/fifo-abi-XXXXXX", base);
    if (!mkdtemp(directory)) { perror("mkdtemp"); ++failures; return; }
    int dir = open(directory, O_RDONLY | O_DIRECTORY);
    CHECK(dir >= 0);
    mode_t old = umask(0027);
    CHECK(syscall(SYS_mknodat, dir, "pipe", S_IFIFO | 0666, 0) == 0);
    umask(old);
    struct stat st;
    CHECK(fstatat(dir, "pipe", &st, 0) == 0 && S_ISFIFO(st.st_mode) && (st.st_mode & 0777) == 0640);
    CHECK(syscall(SYS_mknodat, dir, "pipe", S_IFIFO | 0600, 0) == -1 && errno == EEXIST);
    CHECK(openat(dir, "pipe", O_WRONLY | O_NONBLOCK) == -1 && errno == ENXIO);
    int reader = openat(dir, "pipe", O_RDONLY | O_NONBLOCK);
    char byte;
    CHECK(reader >= 0 && read(reader, &byte, 1) == 0);
    struct pollfd p = {.fd = reader, .events = POLLIN};
    CHECK(poll(&p, 1, 0) == 0); /* No HUP before the first writer. */
    int writer = openat(dir, "pipe", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    CHECK(writer >= 0 && fcntl(writer, F_GETFD) == FD_CLOEXEC);
    CHECK(read(reader, &byte, 1) == -1 && errno == EAGAIN);
    CHECK(write(writer, "x", 1) == 1);
    CHECK(close(writer) == 0);
    CHECK(poll(&p, 1, 0) == 1 && (p.revents & (POLLIN | POLLHUP)) == (POLLIN | POLLHUP));
    CHECK(read(reader, &byte, 1) == 1 && byte == 'x');
    CHECK(read(reader, &byte, 1) == 0);
    CHECK(fstat(reader, &st) == 0 && S_ISFIFO(st.st_mode) && (st.st_mode & 0777) == 0640);
    CHECK(lseek(reader, 0, SEEK_SET) == -1 && errno == ESPIPE);
    CHECK(pread(reader, &byte, 1, 0) == -1 && errno == ESPIPE);
    CHECK(close(reader) == 0);
    puts("[fifo] duplex and unlinked inode lifetime");

    int both = openat(dir, "pipe", O_RDWR | O_NONBLOCK);
    CHECK(both >= 0 && write(both, "y", 1) == 1 && read(both, &byte, 1) == 1 && byte == 'y');
    int duplicate = dup(both);
    CHECK(duplicate >= 0 && close(both) == 0);
    CHECK(unlinkat(dir, "pipe", 0) == 0);
    CHECK(syscall(SYS_mknodat, dir, "pipe", S_IFIFO | 0600, 0) == 0);
    int fresh = openat(dir, "pipe", O_RDWR | O_NONBLOCK);
    CHECK(write(duplicate, "z", 1) == 1);
    CHECK(read(fresh, &byte, 1) == -1 && errno == EAGAIN);
    CHECK(read(duplicate, &byte, 1) == 1 && byte == 'z');
    close(duplicate); close(fresh);
    puts("[fifo] blocking rendezvous");

    for (int reverse = 0; reverse < 2; ++reverse) {
        pid_t child = fork();
        if (!child) {
            int fd = openat(dir, "pipe", reverse ? O_WRONLY : O_RDONLY);
            int ok = fd >= 0 && (reverse ? write(fd, "q", 1) == 1 : read(fd, &byte, 1) == 1 && byte == 'q');
            if (fd >= 0) close(fd);
            _exit(ok ? 0 : 1);
        }
        CHECK(child > 0);
        usleep(30000);
        int fd = openat(dir, "pipe", reverse ? O_RDONLY : O_WRONLY);
        CHECK(fd >= 0);
        CHECK(reverse ? read(fd, &byte, 1) == 1 && byte == 'q' : write(fd, "q", 1) == 1);
        close(fd);
        child_done(child);
    }
    pid_t child = fork();
    if (!child) {
        struct sigaction sa = {.sa_handler = alarm_handler};
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
        alarm(1);
        int fd = openat(dir, "pipe", O_RDONLY);
        _exit(fd == -1 && errno == EINTR ? 0 : 1);
    }
    CHECK(child > 0);
    child_done(child);
    CHECK(openat(dir, "pipe", O_WRONLY | O_NONBLOCK) == -1 && errno == ENXIO);
    CHECK(mkdirat(dir, "existing", 0755) == 0);
    CHECK(fchmod(dir, 0555) == 0);
    child = fork();
    if (!child) {
        if (geteuid() == 0 && setuid(65534)) _exit(1);
        int ret = mkdirat(dir, "existing", 0755);
        _exit(ret == -1 && errno == EEXIST ? 0 : 1);
    }
    CHECK(child > 0);
    child_done(child);
    CHECK(fchmod(dir, 0700) == 0);
    CHECK(unlinkat(dir, "existing", AT_REMOVEDIR) == 0);
    unlinkat(dir, "pipe", 0); close(dir); rmdir(directory);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    exercise("/tmp");
    exercise(getpid() == 1 ? "/run" : "/dev/shm");
    printf("[fifo] DONE failures=%d\n", failures);
    if (getpid() == 1) {
        execl("/sbin/init", "/sbin/init", (char *)NULL);
        for (;;) pause();
    }
    return failures ? 1 : 0;
}
