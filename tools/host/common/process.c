/*
 * Subprocess execution shared by the host build tools.
 *
 * fork()+execv() is used instead of posix_spawn() because the child must report
 * a chdir() or execv() failure back through errno, and the portable way to do
 * that is a pipe written by the child before it gives up.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "process.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Writes `value` to `descriptor` without letting a partial write masquerade as
 * a complete report. */
static int report_errno(int descriptor, int value)
{
    unsigned char bytes[sizeof(int)];
    size_t done = 0;
    size_t index;

    for (index = 0; index < sizeof(bytes); index++) {
        bytes[index] = (unsigned char)((value >> (index * 8)) & 0xff);
    }
    while (done < sizeof(bytes)) {
        ssize_t written = write(descriptor, bytes + done, sizeof(bytes) - done);

        if (written <= 0) {
            if (written < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)written;
    }
    return 0;
}

static int read_reported_errno(int descriptor, int *out_value)
{
    unsigned char bytes[sizeof(int)];
    size_t done = 0;
    size_t index;
    int value = 0;

    while (done < sizeof(bytes)) {
        ssize_t got = read(descriptor, bytes + done, sizeof(bytes) - done);

        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (got == 0) {
            /* EOF with nothing written: execv() succeeded and replaced the
             * child before it could report anything. */
            return (done == 0) ? 1 : -1;
        }
        done += (size_t)got;
    }

    for (index = 0; index < sizeof(bytes); index++) {
        value |= (int)((unsigned int)bytes[index] << (index * 8));
    }
    *out_value = (value != 0) ? value : EIO;
    return 0;
}

int run_process(const char *program, char *const argv[], const char *working_dir,
                int *out_child_status)
{
    int descriptors[2];
    int child_status = 0;
    int reported = 0;
    pid_t child;
    int pipe_result;

    if (program == NULL || program[0] == '\0' || argv == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (pipe(descriptors) != 0) {
        return -1;
    }
    (void)fcntl(descriptors[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(descriptors[1], F_SETFD, FD_CLOEXEC);

    child = fork();
    if (child < 0) {
        int saved = errno;

        close(descriptors[0]);
        close(descriptors[1]);
        errno = saved;
        return -1;
    }

    if (child == 0) {
        close(descriptors[0]);
        if (working_dir != NULL && chdir(working_dir) != 0) {
            (void)report_errno(descriptors[1], errno);
            _exit(127);
        }
        execv(program, argv);
        (void)report_errno(descriptors[1], errno);
        _exit(127);
    }

    close(descriptors[1]);
    pipe_result = read_reported_errno(descriptors[0], &reported);
    close(descriptors[0]);

    for (;;) {
        if (waitpid(child, &child_status, 0) >= 0) {
            break;
        }
        if (errno != EINTR) {
            return -1;
        }
    }

    if (pipe_result < 0) {
        errno = EIO;
        return -1;
    }
    if (pipe_result == 0) {
        errno = reported;
        return -1;
    }

    if (out_child_status != NULL) {
        *out_child_status = child_status;
    }
    return 0;
}
