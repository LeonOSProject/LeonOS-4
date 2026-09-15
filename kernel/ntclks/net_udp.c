/* IPv4 datagrams share the task_file open-description lifetime. All entry
 * points run under the kernel execution lock, including packet delivery. */
#include <ntclks/net_udp.h>
#include <ntclks/net.h>
#include <ntclks/heap.h>
#include <ntclks/sched.h>
#include <ntclks/syscall.h>
#include <ntclks/syscall_internal.h>
#include <ntclks/usercopy.h>
#include <ntclks/time.h>
#include <ntclks/futex.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/capability.h>
#include <linux/poll.h>

#define UDP_MAX_SOCKETS 64u
#define UDP_QUEUE_LENGTH 16u
#define UDP_MAX_PAYLOAD 1472u
#define UDP_SO_BROADCAST 6

struct udp_packet {
    struct udp_packet *next;
    uint32_t source, length;
    uint16_t port;
    unsigned char data[];
};
struct udp_socket {
    uint32_t local, remote;
    uint16_t port, remote_port;
    bool bound, connected, broadcast, reuse;
    int ifindex;
    uint64_t receive_timeout;
    uint32_t queued;
    struct udp_packet *head, *tail;
};
static struct udp_socket *sockets[UDP_MAX_SOCKETS];
static uint16_t next_port = 49152;
static uint16_t swap16(uint16_t x) { return __builtin_bswap16(x); }
static uint32_t swap32(uint32_t x) { return __builtin_bswap32(x); }
static struct udp_socket *udp_from_file(struct task_file *file)
{ return file && file->kind == TASK_FILE_KIND_UDP ? (void *)(uintptr_t)file->aux : NULL; }

static int bind_port(struct udp_socket *s, uint32_t ip, uint16_t port)
{
    if (s->bound) return -LINUX_EINVAL;
    struct leonos_net_config config;
    net_get_config(&config);
    if (ip && (ip >> 24) != 127 && ip != config.local_ip) return -LINUX_EADDRNOTAVAIL;
    struct task *task = sched_current_task();
    if (port && port < 1024 && !(task->cap_effective & (1ULL << CAP_NET_BIND_SERVICE)))
        return -LINUX_EACCES;
    for (unsigned attempt = 0; attempt < 16384; ++attempt) {
        uint16_t candidate = port ? port : next_port++;
        if (next_port < 49152) next_port = 49152;
        bool conflict = false;
        for (unsigned i = 0; i < UDP_MAX_SOCKETS; ++i) {
            struct udp_socket *other = sockets[i];
            if (other && other != s && other->bound && other->port == candidate &&
                (!ip || !other->local || ip == other->local) && !(s->reuse && other->reuse))
                conflict = true;
        }
        if (!conflict) { s->local = ip; s->port = candidate; s->bound = true; return 0; }
        if (port) return -LINUX_EADDRINUSE;
    }
    return -LINUX_EADDRINUSE;
}

void net_udp_input(uint32_t source, uint32_t destination, uint16_t source_port,
                    uint16_t destination_port, const void *data, uint32_t length, int ifindex)
{
    if (length > UDP_MAX_PAYLOAD) return;
    for (unsigned i = 0; i < UDP_MAX_SOCKETS; ++i) {
        struct udp_socket *s = sockets[i];
        if (!s || !s->bound || (s->ifindex && s->ifindex != ifindex) || s->port != destination_port ||
            (s->local && s->local != destination) ||
            (s->connected && (s->remote != source || s->remote_port != source_port)) ||
            s->queued >= UDP_QUEUE_LENGTH) continue;
        struct udp_packet *packet = kernel_malloc(sizeof(*packet) + length);
        if (!packet) continue;
        *packet = (struct udp_packet){.source = source, .port = source_port, .length = length};
        __builtin_memcpy(packet->data, data, length);
        if (s->tail) s->tail->next = packet;
        else s->head = packet;
        s->tail = packet;
        ++s->queued;
        if (destination != 0xffffffffu) break;
    }
}

static int import_address(uint64_t pointer, uint32_t length, struct sockaddr_in *out)
{
    if ((int32_t)length < 0 || length < sizeof(*out)) return -LINUX_EINVAL;
    if (!user_range_ok(pointer, sizeof(*out))) return -LINUX_EFAULT;
    __builtin_memcpy(out, (void *)(uintptr_t)pointer, sizeof(*out));
    return out->sin_family == AF_INET ? 0 : -LINUX_EAFNOSUPPORT;
}

static int export_address(uint64_t pointer, uint64_t length_pointer, uint32_t ip, uint16_t port)
{
    if (!user_range_writable(length_pointer, 4)) return -LINUX_EFAULT;
    uint32_t length = *(uint32_t *)(uintptr_t)length_pointer;
    if ((int32_t)length < 0) return -LINUX_EINVAL;
    if (length > sizeof(struct sockaddr_in)) length = sizeof(struct sockaddr_in);
    if (length && !user_range_writable(pointer, length)) return -LINUX_EFAULT;
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = swap16(port),
                                  .sin_addr = {.s_addr = swap32(ip)}};
    __builtin_memcpy((void *)(uintptr_t)pointer, &address, length);
    *(uint32_t *)(uintptr_t)length_pointer = sizeof(address);
    return 0;
}

int task_udp_send(struct task_file *file, const void *data, uint32_t length,
                   uint32_t flags, uint64_t address, uint32_t address_length)
{
    struct udp_socket *s = udp_from_file(file);
    if (!s) return -LINUX_EBADF;
    if (flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL)) return -LINUX_EOPNOTSUPP;
    if (length > UDP_MAX_PAYLOAD) return -LINUX_EMSGSIZE;
    uint32_t ip = s->remote;
    uint16_t port = s->remote_port;
    if (address) {
        struct sockaddr_in to;
        int ret = import_address(address, address_length, &to);
        if (ret < 0) return ret;
        ip = swap32(to.sin_addr.s_addr); port = swap16(to.sin_port);
    } else if (!s->connected) return -LINUX_EDESTADDRREQ;
    if (!port) return -LINUX_EINVAL;
    if (ip == 0xffffffffu && !s->broadcast) return -LINUX_EACCES;
    if (!s->bound) { int ret = bind_port(s, 0, 0); if (ret < 0) return ret; }
    struct leonos_net_config config;
    net_get_config(&config);
    if ((ip >> 24) == 127 || (ip && ip == config.local_ip)) {
        if (s->ifindex && s->ifindex != 1) return -LINUX_ENETUNREACH;
        net_udp_input(s->local ? s->local : ip, ip, s->port, port, data, length, 1);
        return (int)length;
    }
    if (s->ifindex == 1) return -LINUX_ENETUNREACH;
    int ret = net_ipv4_send_udp(s->local, ip, s->port, port, data, length);
    return ret < 0 ? ret : (int)length;
}

int task_udp_recv(struct task_file *file, void *data, uint32_t length,
                   uint32_t flags, uint64_t address, uint64_t address_length)
{
    struct udp_socket *s = udp_from_file(file);
    if (!s) return -LINUX_EBADF;
    if (flags & ~(MSG_DONTWAIT | MSG_PEEK | MSG_TRUNC | MSG_WAITALL)) return -LINUX_EOPNOTSUPP;
    net_poll_packets();
    if (!s->head) {
        if ((file->flags & LEONOS_O_NONBLOCK) || (flags & MSG_DONTWAIT)) return -LINUX_EAGAIN;
        struct task *task = sched_current_task();
        uint64_t now = time_ticks();
        if (!task->socket_io_deadline) {
            task->socket_io_timed = s->receive_timeout != 0;
            task->socket_io_deadline = !s->receive_timeout || UINT64_MAX - now < s->receive_timeout
                ? UINT64_MAX : now + s->receive_timeout;
        }
        if (now >= task->socket_io_deadline) return -LINUX_EAGAIN;
        sched_sleep_current_until(now + 1);
        return KERNEL_SYSCALL_BLOCKED;
    }
    struct udp_packet *packet = s->head;
    if (address) {
        int ret = export_address(address, address_length, packet->source, packet->port);
        if (ret < 0) return ret;
    }
    uint32_t copied = length < packet->length ? length : packet->length;
    __builtin_memcpy(data, packet->data, copied);
    int ret = (flags & MSG_TRUNC) ? (int)packet->length : (int)copied;
    if (!(flags & MSG_PEEK)) {
        s->head = packet->next;
        if (!s->head) s->tail = NULL;
        --s->queued;
        kernel_free(packet);
    }
    return ret;
}

short task_udp_poll(struct task_file *file, short events)
{
    struct udp_socket *s = udp_from_file(file);
    if (!s) return POLLNVAL;
    net_poll_packets();
    return (events & POLLOUT) | (s->head ? (events & POLLIN) : 0);
}

int task_udp_available(struct task_file *file)
{
    struct udp_socket *s = udp_from_file(file);
    return !s ? -LINUX_EBADF : s->head ? (int)s->head->length : 0;
}

void task_udp_release(struct task_file *file)
{
    struct udp_socket *s = udp_from_file(file);
    if (!s) return;
    for (unsigned i = 0; i < UDP_MAX_SOCKETS; ++i) if (sockets[i] == s) sockets[i] = NULL;
    while (s->head) {
        struct udp_packet *p = s->head;
        s->head = p->next;
        kernel_free(p);
    }
    kernel_free(s);
    file->aux = 0;
}

int64_t syscall_udp(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a5;
    struct task *task = sched_current_task();
    if (!task) return -LINUX_EPERM;
    if (number == __NR_socket) {
        if (a2 && a2 != 17) return -LINUX_EPROTONOSUPPORT;
        if (a1 & ~(uint32_t)(SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC)) return -LINUX_EINVAL;
        unsigned index = 0;
        while (index < UDP_MAX_SOCKETS && sockets[index]) ++index;
        if (index == UDP_MAX_SOCKETS) return -LINUX_ENFILE;
        struct udp_socket *s = kernel_malloc(sizeof(*s));
        if (!s) return -LINUX_ENOMEM;
        struct task_file *file;
        int fd = task_allocate_fd(task, 0, &file);
        if (fd < 0) { kernel_free(s); return fd; }
        *s = (struct udp_socket){0};
        sockets[index] = s;
        file->flags = TASK_FILE_FLAG_SOCKET | TASK_FILE_FLAG_SOCKET_INET | LEONOS_O_RDWR |
            (a1 & SOCK_NONBLOCK ? LEONOS_O_NONBLOCK : 0);
        file->fd_flags = (a1 & SOCK_CLOEXEC) != 0;
        file->kind = TASK_FILE_KIND_UDP;
        file->aux = (uintptr_t)s;
        file->node.type = LEONOS_FS_TYPE_DEVICE;
        return fd;
    }
    struct task_file *file = task_file_for_fd(task, (int32_t)a0);
    struct udp_socket *s = udp_from_file(file);
    if (!s) return -LINUX_EBADF;
    if (number == __NR_bind || number == __NR_connect) {
        if (number == __NR_connect && a2 >= 2 && user_range_ok(a1, 2) &&
            *(uint16_t *)(uintptr_t)a1 == AF_UNSPEC) { s->connected = false; return 0; }
        struct sockaddr_in address;
        int ret = import_address(a1, (uint32_t)a2, &address);
        if (ret < 0) return ret;
        uint32_t ip = swap32(address.sin_addr.s_addr);
        uint16_t port = swap16(address.sin_port);
        if (number == __NR_bind) return bind_port(s, ip, port);
        if (!s->bound && (ret = bind_port(s, 0, 0)) < 0) return ret;
        s->remote = ip; s->remote_port = port; s->connected = true;
        return 0;
    }
    if (number == __NR_getsockname || number == __NR_getpeername) {
        if (number == __NR_getpeername && !s->connected) return -LINUX_ENOTCONN;
        struct leonos_net_config config;
        net_get_config(&config);
        uint32_t local = s->local;
        if (!local && s->connected) local = s->remote >> 24 == 127 ? 0x7f000001 : config.local_ip;
        return export_address(a1, a2, number == __NR_getsockname ? local : s->remote,
                               number == __NR_getsockname ? s->port : s->remote_port);
    }
    if (number == __NR_setsockopt || number == __NR_getsockopt) {
        if (a1 != SOL_SOCKET) return -LINUX_ENOPROTOOPT;
        bool set = number == __NR_setsockopt;
        uint32_t length;
        if (set) length = (uint32_t)a4;
        else {
            if (!user_range_writable(a4, 4)) return -LINUX_EFAULT;
            length = *(uint32_t *)(uintptr_t)a4;
        }
        if ((int32_t)length < 0) return -LINUX_EINVAL;
        if (a2 == SO_BINDTODEVICE) {
            if (set) {
                struct task *task = sched_current_task();
                if (!(task->cap_effective & (1ULL << CAP_NET_RAW))) return -LINUX_EPERM;
                char name[16] = {0};
                uint32_t size = length < sizeof(name) - 1 ? length : sizeof(name) - 1;
                if (size && !user_range_ok(a3, size)) return -LINUX_EFAULT;
                if (size) __builtin_memcpy(name, (void *)(uintptr_t)a3, size);
                int index = !name[0] ? 0 : !__builtin_strcmp(name, "eth0") ? 2 : !__builtin_strcmp(name, "lo") ? 1 : -1;
                if (index < 0) return -LINUX_ENODEV;
                s->ifindex = index;
                return 0;
            }
            const char *name = s->ifindex == 2 ? "eth0" : s->ifindex == 1 ? "lo" : "";
            uint32_t size = s->ifindex == 2 ? 5 : s->ifindex == 1 ? 3 : 0;
            if (size && length < 16) return -LINUX_EINVAL;
            if (size && !user_range_writable(a3, size)) return -LINUX_EFAULT;
            if (size) __builtin_memcpy((void *)(uintptr_t)a3, name, size);
            *(uint32_t *)(uintptr_t)a4 = size;
            return 0;
        }
        int value = 0;
        if (a2 == SO_RCVTIMEO) {
            int64_t tv[2];
            uint64_t *timeout = &s->receive_timeout;
            if (set) {
                if (length < sizeof(tv)) return -LINUX_EINVAL;
                if (!user_range_ok(a3, sizeof(tv))) return -LINUX_EFAULT;
                __builtin_memcpy(tv, (void *)(uintptr_t)a3, sizeof(tv));
                if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000) return -LINUX_EDOM;
                uint64_t seconds = (uint64_t)tv[0];
                *timeout = seconds > (UINT64_MAX - NTCLKS_TICK_HZ) / NTCLKS_TICK_HZ
                    ? UINT64_MAX : seconds * NTCLKS_TICK_HZ +
                      ((uint64_t)tv[1] * NTCLKS_TICK_HZ + 999999) / 1000000;
                return 0;
            }
            tv[0] = *timeout / NTCLKS_TICK_HZ;
            tv[1] = (*timeout % NTCLKS_TICK_HZ) * 1000000 / NTCLKS_TICK_HZ;
            if (length > sizeof(tv)) length = sizeof(tv);
            if (length && !user_range_writable(a3, length)) return -LINUX_EFAULT;
            __builtin_memcpy((void *)(uintptr_t)a3, tv, length);
            *(uint32_t *)(uintptr_t)a4 = length;
            return 0;
        }
        if (set) {
            if (length < 4) return -LINUX_EINVAL;
            if (!user_range_ok(a3, 4)) return -LINUX_EFAULT;
            value = *(int *)(uintptr_t)a3;
            if (a2 == SO_REUSEADDR) s->reuse = value != 0;
            else if (a2 == UDP_SO_BROADCAST) s->broadcast = value != 0;
            else return -LINUX_ENOPROTOOPT;
            return 0;
        }
        if (a2 == SO_TYPE) value = SOCK_DGRAM;
        else if (a2 == SO_DOMAIN) value = AF_INET;
        else if (a2 == SO_PROTOCOL) value = 17;
        else if (a2 == SO_ERROR) value = 0;
        else if (a2 == SO_REUSEADDR) value = s->reuse;
        else if (a2 == UDP_SO_BROADCAST) value = s->broadcast;
        else return -LINUX_ENOPROTOOPT;
        if (length > 4) length = 4;
        if (length && !user_range_writable(a3, length)) return -LINUX_EFAULT;
        __builtin_memcpy((void *)(uintptr_t)a3, &value, length);
        *(uint32_t *)(uintptr_t)a4 = length;
        return 0;
    }
    return -LINUX_EOPNOTSUPP;
}
