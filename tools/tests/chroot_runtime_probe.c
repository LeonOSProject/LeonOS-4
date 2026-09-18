#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static void marker(const char *path)
{
    char value[16] = {0};
    int fd = open(path, O_RDONLY);
    assert(fd >= 0 && read(fd, value, sizeof(value)) == 6);
    assert(!strcmp(value, "inside"));
    close(fd);
}
static void reaped(pid_t child)
{
    int status;
    assert(child > 0 && waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static int shared_root(void *unused)
{
    (void)unused;
    assert(chroot("/nested") == 0 && chdir("/") == 0);
    return 0;
}
int main(int argc, char **argv)
{
    char cwd[512];
    if (argc == 2 && !strcmp(argv[1], "--worker")) {
        assert(getcwd(cwd, sizeof(cwd)) && !strcmp(cwd, "/"));
        marker("/marker");
        marker("/absolute");
        marker("/relative");
        marker("/../../marker");
        int dir = open("/nested", O_DIRECTORY);
        int fd = openat(dir, "../../marker", O_RDONLY);
        assert(fd >= 0); close(fd); close(dir);
        assert(chdir("/nested") == 0 && chdir("../../..") == 0);
        assert(getcwd(cwd, sizeof(cwd)) && !strcmp(cwd, "/"));
        assert(chroot("/marker") < 0 && errno == ENOTDIR);
        assert(chroot("/missing") < 0 && errno == ENOENT);
        pid_t child = fork();
        assert(child >= 0);
        if (!child) {
            struct { unsigned version; int pid; } header = {0x20080522, 0};
            struct { unsigned effective, permitted, inheritable; } caps[2];
            assert(syscall(SYS_capget, &header, caps) == 0);
            caps[0].effective &= ~(1u << 18);
            assert(syscall(SYS_capset, &header, caps) == 0);
            assert(chroot("/") < 0 && errno == EPERM);
            _exit(0);
        }
        reaped(child);
        void *stack = malloc(65536);
        assert(stack);
        reaped(clone(shared_root, (char *)stack + 65536, CLONE_FS | SIGCHLD, NULL));
        assert(access("/marker", F_OK) < 0 && errno == ENOENT);
        assert(getcwd(cwd, sizeof(cwd)) && !strcmp(cwd, "/"));
        free(stack);
        return 0;
    }
    assert(argc == 2);
    assert(chdir(argv[1]) == 0);
    assert(getcwd(cwd, sizeof(cwd)));
    char previous[512]; strcpy(previous, cwd);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        assert(chdir("/") == 0 && chroot(argv[1]) == 0);
        assert(syscall(SYS_getcwd, cwd, sizeof(cwd)) > 0);
        assert(!strncmp(cwd, "(unreachable)", 13));
        assert(chdir("/") == 0);
        execl("/bin/probe", "probe", "--worker", NULL);
        perror("exec rooted dynamic probe");
        _exit(1);
    }
    reaped(child);
    assert(getcwd(cwd, sizeof(cwd)) && !strcmp(previous, cwd));
    puts("[apk-probe] DONE failures=0 chroot: fork, exec, PT_INTERP, symlinks, dotdot, CLONE_FS, permissions");
    return 0;
}
