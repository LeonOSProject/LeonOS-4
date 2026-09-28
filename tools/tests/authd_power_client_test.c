#include <assert.h>
#include <stdio.h>
#include <sys/un.h>
#include "../../userland/runtime/src/authd_client.c"

static unsigned connections, sends, closes;
static uint32_t server_uid;
static int receive_error, code = -EIO;
static time_t seconds;

int clock_gettime(clockid_t clock, struct timespec *value)
{ (void)clock; *value = (struct timespec){.tv_sec = seconds++}; return 0; }
int poll(struct pollfd *fds, nfds_t count, int timeout)
{ (void)fds; (void)count; (void)timeout; return 0; }
int reliefos_ipc_connect(const char *path)
{ assert(!strcmp(path, RELIEFOS_IPC_SOCK_AUTH)); ++connections; return 9; }
int reliefos_ipc_peer_credentials(int fd, struct ucred *peer)
{ assert(fd == 9); *peer = (struct ucred){.uid = server_uid}; return 0; }
int reliefos_ipc_set_nonblock(int fd, int enabled)
{ assert(fd == 9 && enabled); return 0; }
int reliefos_ipc_send(int fd, uint32_t type, const void *payload, uint32_t length)
{
    assert(fd == 9 && type == RELIEFOS_AUTHD_MSG_POWER && length == sizeof(struct reliefos_authd_power));
    const struct reliefos_authd_power *power = payload;
    assert(power->command == 0x01234567U && !power->reserved);
    ++sends;
    return 0;
}
int reliefos_ipc_recv(int fd, uint32_t *type, void *payload, uint32_t capacity, uint32_t *length)
{
    assert(fd == 9 && capacity == sizeof(struct reliefos_authd_ack));
    if (receive_error) { errno = receive_error; return -1; }
    *type = RELIEFOS_AUTHD_MSG_ACK;
    *length = sizeof(struct reliefos_authd_ack);
    *(struct reliefos_authd_ack *)payload = (struct reliefos_authd_ack){.code = code};
    return 0;
}
int reliefos_ipc_close(int fd)
{ assert(fd == 9); ++closes; errno = EBADF; return -1; }

int main(void)
{
    authd_fd = 17; /* Cached pre-login credentials must never be reused. */
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == EIO);
    assert(connections == 1 && closes == 1 && sends == 1 && authd_fd == 17);
    server_uid = 1000;
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == EPERM);
    assert(connections == 2 && closes == 2 && sends == 1);
    server_uid = 0;
    code = -EPERM;
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == EPERM);
    code = 0;
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == EPROTO);
    receive_error = EPIPE;
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == EPIPE);
    receive_error = EAGAIN;
    assert(reliefos_auth_request_power(0x01234567U) == -1 && errno == ETIMEDOUT);
    assert(connections == 6 && closes == 6 && sends == 5 && authd_fd == 17);
    puts("PASS power client: fresh credentials, root daemon, error/EOF/timeout handling and descriptor cleanup");
}
