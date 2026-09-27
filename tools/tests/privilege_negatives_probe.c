/* Guest-side helper for tools/test_privilege_negatives.py (ntclks separation
 * phase 4 privilege negative tests). Never run on the host: every mode reads
 * LeonOS /proc and /dev/ttyS0 as implemented by this kernel.
 *
 * All output lines are "PR <tag> key=value ..." written to /dev/ttyS0 so the
 * QEMU harness can parse them from the serial log. Modes:
 *
 *   flags  <tag>                own identity, LeonOSFlags, AT_SECURE, dumpable
 *   hang   <tag> <seconds>      report, then sleep (same-user kill target)
 *   imp    <tag> <uid> <path> [arg ...]
 *                               drop to <uid>, fork+exec <path>, observe the
 *                               child via /proc/<pid>/status, SIGKILL it,
 *                               reap it ("PR <tag>.result ...")
 *   reexec <tag> <path>         report, then exec <path> "flags <tag>.after"
 *   forkp  <tag>                report (parent), fork a child that reports its
 *                               own flags ("PR <tag>.child") and hangs; kill
 *                               and reap it ("PR <tag>.result")
 *   nnp    <tag> <path>         PR_SET_NO_NEW_PRIVS, then exec <path>
 *                               "flags <tag>.after"
 *   rd     <tag> <device>       open O_RDONLY + read 512 bytes, report rc/errno
 *   scm    <tag>                drop to uid 1000, then send a forged and a
 *                               real SCM_CREDENTIALS message over a unix
 *                               socketpair ("PR <tag>.forged/.real")
 *
 * The probe target may be exec'd at a service path, so the flags a mode
 * reports for itself are exactly the post-exec TASK_FLAG_* state decided by
 * userland_exec_current_node (03-permission-matrix M1).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#endif
#ifndef PR_CAP_AMBIENT_IS_SET
#define PR_CAP_AMBIENT_IS_SET 1
#endif

#define TASK_FLAG_SERVICE 0x1u
#define TASK_FLAG_WINDOW_SERVER 0x8u

struct cap_header_local {
    unsigned int version;
    int pid;
};

struct cap_data_local {
    unsigned int effective, permitted, inheritable;
};

struct ucred_local {
    int pid;
    unsigned int uid, gid;
};

static int report_fd = -1;

static void report_init(void)
{
    report_fd = open("/dev/ttyS0", O_WRONLY);
    if (report_fd < 0) report_fd = 1;
}

static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(report_fd, fmt, ap);
    va_end(ap);
}

struct status_info {
    char name[64];
    char uid[64];
    unsigned long long flags;
};

/* Read LeonOS /proc/<pid>/status: Name, Uid (real/effective) and LeonOSFlags. */
static int read_status(int pid, struct status_info *out)
{
    char path[64], buf[8192];
    out->name[0] = '?';
    out->name[1] = 0;
    strcpy(out->uid, "?");
    out->flags = 0;
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;
    for (char *line = buf, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        if (!strncmp(line, "Name:\t", 6)) {
            snprintf(out->name, sizeof(out->name), "%s", line + 6);
        } else if (!strncmp(line, "Uid:\t", 5)) {
            unsigned real = 0, eff = 0;
            if (sscanf(line + 5, "%u %u", &real, &eff) == 2)
                snprintf(out->uid, sizeof(out->uid), "%u/%u", real, eff);
        } else if (!strncmp(line, "LeonOSFlags:\t", 13)) {
            out->flags = strtoull(line + 13, NULL, 10);
        }
    }
    return 0;
}

/* One "PR <tag> pid=.. uid=.. euid=.. flags=0x.. atsecure=.. dumpable=.." line. */
static void emit_status(const char *tag, int pid)
{
    struct status_info info;
    read_status(pid, &info);
    struct cap_header_local header = {0x20080522u, 0};
    struct cap_data_local data[2] = {{0}, {0}};
    int ambient = syscall(SYS_capget, &header, data) == 0
        ? (int)prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, 0, 0, 0)
        : -1;
    emit("PR %s pid=%d uid=%d euid=%d gid=%d egid=%d flags=0x%llx "
         "atsecure=%d dumpable=%d amb=%d name=%s\n",
         tag, pid, (int)getuid(), (int)geteuid(), (int)getgid(), (int)getegid(),
         info.flags, (int)getauxval(AT_SECURE), (int)prctl(PR_GET_DUMPABLE),
         ambient, info.name);
}

static int drop_to(uid_t uid)
{
    if (geteuid() != 0) return 0;
    if (setgroups(0, NULL) < 0) return -1;
    if (setresgid(uid, uid, uid) < 0) return -1;
    if (setresuid(uid, uid, uid) < 0) return -1;
    return 0;
}

static void reap_report(const char *tag, pid_t child)
{
    int rc = kill(child, SIGKILL);
    int saved = errno;
    int status = 0;
    waitpid(child, &status, 0);
    int dead = (WIFSIGNALED(status) || WIFEXITED(status)) ? 1 : 0;
    emit("PR %s.result kill_rc=%d kill_errno=%d dead=%d\n", tag, rc, saved, dead);
}

static int mode_imp(const char *tag, uid_t uid, char **argv)
{
    if (drop_to(uid) < 0) {
        emit("PR %s.result drop_rc=-1 drop_errno=%d\n", tag, errno);
        return 1;
    }
    pid_t child = fork();
    if (child < 0) return 1;
    if (child == 0) {
        execv(argv[0], argv);
        _exit(127);
    }
    sleep(1);
    struct status_info info;
    read_status(child, &info);
    emit("PR %s.result child_pid=%d child_uid=%s child_name=%s child_flags=0x%llx\n",
         tag, (int)child, info.uid, info.name, info.flags);
    reap_report(tag, child);
    return 0;
}

static int mode_forkp(const char *tag)
{
    emit_status(tag, (int)getpid());
    pid_t child = fork();
    if (child < 0) return 1;
    if (child == 0) {
        struct status_info info;
        read_status((int)getpid(), &info);
        emit("PR %s.child pid=%d uid=%d flags=0x%llx\n",
             tag, (int)getpid(), (int)getuid(), info.flags);
        for (;;) pause();
    }
    sleep(1);
    reap_report(tag, child);
    return 0;
}

static int mode_rd(const char *tag, const char *device)
{
    errno = 0;
    int fd = open(device, O_RDONLY);
    int open_errno = errno;
    long read_rc = -1;
    int read_errno = 0;
    if (fd >= 0) {
        char buffer[512];
        errno = 0;
        ssize_t n = read(fd, buffer, sizeof(buffer));
        read_errno = errno;
        read_rc = n < 0 ? -1 : (long)n;
        close(fd);
    }
    emit("PR %s open_rc=%d open_errno=%d read_rc=%ld read_errno=%d\n",
         tag, fd, open_errno, read_rc, read_errno);
    return 0;
}

static int send_cred(int fd, int pid, unsigned uid, unsigned gid)
{
    struct ucred_local cred = {pid, uid, gid};
    char control[CMSG_SPACE(sizeof(cred))];
    struct msghdr message;
    struct iovec iov;
    char byte = 'x';
    memset(&message, 0, sizeof(message));
    memset(control, 0, sizeof(control));
    iov.iov_base = &byte;
    iov.iov_len = 1;
    message.msg_iov = &iov;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);
    struct cmsghdr *header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_CREDENTIALS;
    header->cmsg_len = CMSG_LEN(sizeof(cred));
    memcpy(CMSG_DATA(header), &cred, sizeof(cred));
    return sendmsg(fd, &message, 0);
}

static int mode_scm(const char *tag)
{
    if (drop_to(1000) < 0) {
        emit("PR %s.forged drop_rc=-1 drop_errno=%d\n", tag, errno);
        return 1;
    }
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        emit("PR %s.forged socketpair_rc=-1 socketpair_errno=%d\n", tag, errno);
        return 1;
    }
    errno = 0;
    int forged = send_cred(sockets[0], (int)getpid(), 0, 0);
    int forged_errno = errno;
    emit("PR %s.forged rc=%d errno=%d\n", tag, forged, forged_errno);
    errno = 0;
    int real = send_cred(sockets[0], (int)getpid(), (unsigned)getuid(),
                         (unsigned)getgid());
    int real_errno = errno;
    emit("PR %s.real rc=%d errno=%d\n", tag, real, real_errno);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    report_init();
    const char *mode = argv[1];
    const char *tag = argv[2];
    if (!strcmp(mode, "flags")) {
        emit_status(tag, (int)getpid());
        return 0;
    }
    if (!strcmp(mode, "hang")) {
        emit_status(tag, (int)getpid());
        sleep(argc > 3 ? atoi(argv[3]) : 60);
        return 0;
    }
    if (!strcmp(mode, "imp") && argc >= 5)
        return mode_imp(tag, (uid_t)strtoul(argv[3], NULL, 10), &argv[4]);
    if (!strcmp(mode, "forkp"))
        return mode_forkp(tag);
    if (!strcmp(mode, "reexec") && argc >= 4) {
        emit_status(tag, (int)getpid());
        char after[64];
        snprintf(after, sizeof(after), "%s.after", tag);
        char *args[] = {argv[3], "flags", after, NULL};
        execv(argv[3], args);
        return 127;
    }
    if (!strcmp(mode, "nnp") && argc >= 4) {
        prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
        char after[64];
        snprintf(after, sizeof(after), "%s.after", tag);
        char *args[] = {argv[3], "flags", after, NULL};
        execv(argv[3], args);
        return 127;
    }
    if (!strcmp(mode, "rd") && argc >= 4)
        return mode_rd(tag, argv[3]);
    if (!strcmp(mode, "scm"))
        return mode_scm(tag);
    return 2;
}
