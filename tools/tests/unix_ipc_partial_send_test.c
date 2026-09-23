/* Reproduce a paused compositor filling a nonblocking stream mid-frame. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <leonos/unix_ipc.h>

static uint8_t wire[4096];
static size_t written;
static size_t allowance = 7;
static unsigned rights_count;

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
    assert(fd == 42 && (flags & MSG_NOSIGNAL));
    if (!allowance) { errno = EAGAIN; return -1; }
    const struct iovec *vector = message->msg_iov;
    size_t count = vector->iov_len < allowance ? vector->iov_len : allowance;
    assert(count <= sizeof(wire) - written);
    memcpy(wire + written, vector->iov_base, count);
    written += count;
    allowance -= count;
    if (message->msg_controllen) ++rights_count;
    return (ssize_t)count;
}

/* Old code waits and loses the unsent tail when this timeout expires. */
int poll(struct pollfd *fds, nfds_t count, int timeout)
{
    (void)fds; (void)count; (void)timeout;
    return 0;
}

static size_t check_frame(size_t offset, uint32_t type, const char *payload)
{
    struct leonos_ipc_frame header;
    uint32_t actual_type;
    memcpy(&header, wire + offset, sizeof(header));
    assert(header.magic == LEONOS_IPC_MAGIC && header.version == LEONOS_IPC_VERSION);
    assert(header.length == sizeof(type) + strlen(payload) + 1);
    offset += sizeof(header);
    memcpy(&actual_type, wire + offset, sizeof(actual_type));
    assert(actual_type == type);
    offset += sizeof(actual_type);
    assert(memcmp(wire + offset, payload, strlen(payload) + 1) == 0);
    return offset + strlen(payload) + 1;
}

int main(void)
{
    assert(leonos_ipc_send_fd(42, 10, "window", 7, 73) == 0);
    assert(written == 7 && rights_count == 1);
    /* Backpressure must reject the new message without discarding the tail. */
    assert(leonos_ipc_send(42, 20, "input", 6) == -1 && errno == EAGAIN);
    assert(written == 7);
    allowance = sizeof(wire) - written;
    assert(leonos_ipc_send(42, 20, "input", 6) == 0);
    size_t end = check_frame(0, 10, "window");
    end = check_frame(end, 20, "input");
    assert(end == written && rights_count == 1);
    /* The final queued frame must finish without requiring a new message. */
    allowance = 3;
    assert(leonos_ipc_send(42, 30, "last", 5) == 0);
    assert(leonos_ipc_flush(42) == -1 && errno == EAGAIN);
    allowance = sizeof(wire) - written;
    assert(leonos_ipc_flush(42) == 0);
    assert(check_frame(end, 30, "last") == written);
    assert(leonos_ipc_flush(42) == 0);
    /* Closing a paused connection discards its tail before fd reuse. */
    allowance = 1;
    assert(leonos_ipc_send(42, 40, "closed", 7) == 0);
    (void)leonos_ipc_close(42);
    assert(leonos_ipc_flush(42) == 0);
    puts("PASS IPC partial send: paused peer, ordered resume and one SCM_RIGHTS delivery");
    return 0;
}
