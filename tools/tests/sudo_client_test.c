#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <reliefos/authd.h>
#include <reliefos/sudo.h>
#include <reliefos/unix_ipc.h>
#include "../../userland/runtime/src/authd_client_internal.h"

/* The transport and client are real. A protocol peer runs at the host user's
 * UID; only the server-UID check is substituted, never password decisions. */
int __wrap_reliefos_ipc_peer_credentials(int fd, struct ucred *peer)
{
    socklen_t size = sizeof(*peer);
    int ret = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, peer, &size);
    if (!ret) peer->uid = 0;
    return ret;
}

int __wrap_reliefos_ipc_connect(const char *path)
{
    (void)path;
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(pair[0]);
        unsigned streams = 0;
        for (;;) {
            char data[4096];
            uint32_t type, length;
            int received = -1;
            if (reliefos_ipc_recv_fd(pair[1], &type, data, sizeof(data), &length, &received) < 0) {
                if (errno == EAGAIN || errno == EINTR) { poll(NULL, 0, 1); continue; }
                break;
            }
            if (type == RELIEFOS_AUTHD_MSG_RUN_FD) {
                struct reliefos_authd_run_fd message;
                memcpy(&message, data, sizeof(message));
                assert(message.index < 3 && received >= 0);
                assert(fcntl(received, F_GETFD) & FD_CLOEXEC);
                streams |= 1u << message.index;
            } else if (type == RELIEFOS_AUTHD_MSG_RUN) {
                assert(streams == 7 && length == sizeof(struct reliefos_authd_run));
                const struct reliefos_authd_run *run = (const void *)data;
                assert(run->cwd[0] == '/');
                struct reliefos_authd_run_ack reply = {.child_pid = 123};
                assert(reliefos_ipc_send(pair[1], type, &reply, sizeof(reply)) == 0);
            } else if (type == RELIEFOS_AUTHD_MSG_WAIT) {
                struct reliefos_authd_wait_ack reply = {.status = 7 << 8};
                assert(reliefos_ipc_send(pair[1], type, &reply, sizeof(reply)) == 0);
            } else {
                struct reliefos_authd_ack reply = {.code = type == RELIEFOS_AUTHD_MSG_SUDO_CHECK};
                assert(reliefos_ipc_send(pair[1], RELIEFOS_AUTHD_MSG_ACK, &reply, sizeof(reply)) == 0);
            }
            if (received >= 0) close(received);
        }
        _exit(0);
    }
    close(pair[1]);
    return pair[0];
}

int main(void)
{
    int cached = reliefos_authd_client_open();
    assert(cached >= 0);
    for (int i = 0; i < 8; ++i) {
        char *command[] = {"id", NULL};
        uint32_t pid;
        int status;
        assert(reliefos_sudo_check() == RELIEFOS_SUDO_CACHED);
        assert(reliefos_sudo_run(NULL, "secret", command, &pid) == 0 && pid == 123);
        assert(reliefos_sudo_wait(pid, &status) == 0 && WEXITSTATUS(status) == 7);
        assert(reliefos_sudo_kill() == 0);
        assert(fcntl(cached, F_GETFD) >= 0);
    }
    reliefos_authd_client_close();
    int status;
    while (waitpid(-1, &status, 0) > 0) assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    puts("sudo client: private socket lifecycle, repeated RUN/WAIT and SCM_RIGHTS PASS");
    return 0;
}
