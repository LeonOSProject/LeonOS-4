#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static void check_pipe(int fd)
{
    struct stat st;
    struct statx sx;
    assert(syscall(SYS_fstat, fd, &st) == 0);
    assert(st.st_mode == (S_IFIFO | 0600) && st.st_rdev == 0);
    assert(fstat(fd, &st) == 0);
    assert(st.st_mode == (S_IFIFO | 0600) && st.st_rdev == 0);
    assert(syscall(SYS_newfstatat, fd, "", &st, AT_EMPTY_PATH) == 0);
    assert(st.st_mode == (S_IFIFO | 0600) && st.st_rdev == 0);
    assert(statx(fd, "", AT_EMPTY_PATH, STATX_TYPE | STATX_MODE, &sx) == 0);
    assert((sx.stx_mask & (STATX_TYPE | STATX_MODE)) == (STATX_TYPE | STATX_MODE));
    assert(sx.stx_mode == (S_IFIFO | 0600) && !sx.stx_rdev_major && !sx.stx_rdev_minor);
}

int main(void)
{
    int pair[2];
    assert(pipe2(pair, O_CLOEXEC) == 0);
    check_pipe(pair[0]);
    check_pipe(pair[1]);
    int duplicate = dup(pair[0]);
    assert(duplicate >= 0);
    close(pair[0]);
    check_pipe(duplicate);
    close(duplicate);
    close(pair[1]);
    int nullfd = open("/dev/null", O_RDONLY);
    struct stat st;
    assert(nullfd >= 0 && fstat(nullfd, &st) == 0 && S_ISCHR(st.st_mode));
    close(nullfd);
    puts("PASS pipe stat: raw fstat/newfstatat/statx, musl fstat, dup and character device");
    return 0;
}
