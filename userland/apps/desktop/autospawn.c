/* Test/debug autospawn policy, owned by userspace.
 *
 * The kernel keeps only the /proc/cmdline mechanism (ntclks separation M5);
 * this file decides which programs a boot command line requests and launches
 * them as desktop children. Matching is exact-token ("autospawn=<name>"), so a
 * token like autospawn=helloworld can no longer trigger the hello target the
 * way the old kernel substring match did. Each target runs at most once per
 * boot (one-shot), and the log stays quiet when no autospawn= token exists.
 */
#include "desktop.h"
#include <errno.h>
#include <fcntl.h>
#include <leonos/launch.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define AUTOSPAWN_CMDLINE_MAX 1024

struct autospawn_target {
    const char *name;
    const char *path;
    const char *const *argv; /* NULL: argv = {path, 0} like the old kernel spawns */
    const char *sub;         /* log sub-label for multi-binary targets, else NULL */
    unsigned char done;      /* one-shot latch, one per target per boot */
};

/* Verbatim spawn mapping from the retired kernel block: same paths and the
 * same fixed argv for the diagnostic probes. The old autospawn=vim flag was
 * parsed but never spawned anything; it is deliberately not resurrected. */
static const char *const inventory_argv[] = {"linux-inventory", 0};
static const char *const ioctl_argv[] = {"linux-ioctl-cloexec", 0};
static const char *const python_argv[] = {"python3", "/bin/hello.py", 0};

static struct autospawn_target autospawn_targets[] = {
    {"hello", LEONOS_LAYOUT_LEONOS_APPS "/hello/hello.elf", NULL, NULL, 0},
    {"uidemo", LEONOS_LAYOUT_LEONOS_APPS "/uidemo/uidemo.elf", NULL, NULL, 0},
    {"terminal", LEONOS_LAYOUT_LEONOS_APPS "/terminal/terminal.elf", NULL, NULL, 0},
    {"memtest", LEONOS_LAYOUT_LEONOS_APPS "/memtest/memtest.elf", NULL, NULL, 0},
    {"linuxabi", LEONOS_LAYOUT_LEONOS_TESTS "/musl-abi-dynamic.elf", NULL, "dynamic", 0},
    {"linuxabi", LEONOS_LAYOUT_LEONOS_TESTS "/musl-abi-static.elf", NULL, "static", 0},
    {"ltp", LEONOS_LAYOUT_LEONOS_TESTS "/ltp-runner.elf", NULL, NULL, 0},
    {"gcc", LEONOS_LAYOUT_LEONOS_TESTS "/gcc-probe.elf", NULL, NULL, 0},
    {"inventory", LEONOS_LAYOUT_LEONOS_TESTS "/linux-inventory.elf", inventory_argv, NULL, 0},
    {"ioctlcloexec", LEONOS_LAYOUT_LEONOS_TESTS "/linux-ioctl-cloexec.elf", ioctl_argv, NULL, 0},
    {"python315", "/opt/python/bin/python3.15", python_argv, NULL, 0},
};

/* Console sink for the [desktop] autospawn lines and for the children's
 * stdio. The retired kernel spawns started with console-only implicit stdio,
 * so their output landed in the serial log; rebinding fd 0/1/2 to
 * /dev/console around the fork+exec keeps that contract for desktop
 * children (whose own stdio points at /var/log/desktop.log). */
static int autospawn_console_fd = -1;

static void autospawn_log(const char *format, ...)
{
    char line[256];
    int length;
    va_list args;
    va_start(args, format);
    length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0) return;
    if ((size_t)length >= sizeof(line)) length = (int)sizeof(line) - 1;
    if (autospawn_console_fd >= 0) {
        (void)write(autospawn_console_fd, line, (size_t)length);
    } else {
        fwrite(line, 1, (size_t)length, stdout);
    }
}

/* Spawn one target as a desktop child carrying console-only stdio. Returns
 * the child pid, or a negative errno-style value when no child was created
 * (mirroring the retired kernel spawn's negative pid on lookup failure). */
static int autospawn_spawn(const struct autospawn_target *target)
{
    char *default_argv[2];
    char *const *argv = (char *const *)target->argv;
    int saved[3] = {-1, -1, -1};
    int bound = 0;
    int pid;

    if (!argv) {
        default_argv[0] = (char *)target->path;
        default_argv[1] = NULL;
        argv = default_argv;
    }
    errno = 0;
    if (access(target->path, X_OK) != 0) return errno ? -errno : -2;
    if (autospawn_console_fd >= 0) {
        bound = 1;
        for (int i = 0; i < 3; ++i) {
            saved[i] = dup(i);
            if (saved[i] < 0 || dup2(autospawn_console_fd, i) < 0) {
                bound = 0;
                break;
            }
        }
        if (!bound) {
            for (int i = 0; i < 3; ++i) {
                if (saved[i] >= 0) {
                    dup2(saved[i], i);
                    close(saved[i]);
                    saved[i] = -1;
                }
            }
        }
    }
    pid = leonos_spawn_argv(target->path, argv);
    if (bound) {
        for (int i = 0; i < 3; ++i) {
            if (saved[i] >= 0) {
                dup2(saved[i], i);
                close(saved[i]);
            }
        }
    }
    return pid;
}

void desktop_autospawn_from_cmdline(void)
{
    char cmdline[AUTOSPAWN_CMDLINE_MAX];
    size_t used = 0;
    int saw_token = 0;
    int handled = 0;
    int fd;

    autospawn_console_fd = open("/dev/console", O_RDWR);
    fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        while (used + 1 < sizeof(cmdline)) {
            ssize_t count = read(fd, cmdline + used, sizeof(cmdline) - 1 - used);
            if (count <= 0) break;
            used += (size_t)count;
        }
        close(fd);
    }
    cmdline[used] = 0;
    if (!used) {
        if (autospawn_console_fd >= 0) {
            close(autospawn_console_fd);
            autospawn_console_fd = -1;
        }
        return;
    }

    /* The autospawn children keep the desktop's uid/cwd/session like the old
     * kernel spawns did; the login-session identity application must not run
     * for them (no session exists at desktop startup and its failure would
     * kill the child). */
    leonos_launch_use_session(0);
    for (char *token = cmdline; *token;) {
        char *end = token;
        char *name;
        while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r') ++end;
        if (*end) *end++ = 0;
        if (strncmp(token, "autospawn=", 10) != 0) {
            token = end;
            continue;
        }
        saw_token = 1;
        name = token + 10;
        if (!name[0]) {
            token = end;
            continue;
        }
        for (size_t i = 0; i < sizeof(autospawn_targets) / sizeof(autospawn_targets[0]); ++i) {
            struct autospawn_target *target = &autospawn_targets[i];
            if (target->done || strcmp(target->name, name) != 0) continue;
            target->done = 1;
            ++handled;
            int pid = autospawn_spawn(target);
            if (target->sub) {
                autospawn_log("[desktop] autospawn %s %s pid=%d\n",
                              target->name, target->sub, pid);
            } else {
                autospawn_log("[desktop] autospawn %s pid=%d\n", target->name, pid);
            }
        }
        token = end;
    }
    if (saw_token && !handled) {
        autospawn_log("[desktop] autospawn: no matching targets\n");
    }
    leonos_launch_use_session(1);
    if (autospawn_console_fd >= 0) {
        close(autospawn_console_fd);
        autospawn_console_fd = -1;
    }
}
