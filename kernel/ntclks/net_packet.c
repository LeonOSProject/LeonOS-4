/* Raw IPv4 and Ethernet sockets. Serialized by the kernel execution lock. */
#include <ntclks/net_packet.h>
#include <ntclks/net.h>
#include <ntclks/e1000.h>
#include <ntclks/heap.h>
#include <ntclks/sched.h>
#include <ntclks/syscall.h>
#include <ntclks/syscall_internal.h>
#include <ntclks/usercopy.h>
#include <ntclks/time.h>
#include <ntclks/futex.h>
#include <linux/socket.h>
#include <linux/if_packet.h>
#include <linux/capability.h>
#include <linux/poll.h>
#include <linux/errno.h>

#define PACKET_LIMIT 64u
#define PACKET_QUEUE_LIMIT 32u
struct packet_message {
    struct packet_message *next;
    uint32_t length;
    struct sockaddr_ll link;
    struct sockaddr_in ip;
    uint8_t data[];
};
struct packet_socket {
    int domain, type, protocol, ifindex;
    uint32_t local_ip, remote_ip;
    bool hdrincl, broadcast;
    uint64_t timeout;
    unsigned queued;
    struct packet_message *head, *tail;
};
static struct packet_socket *sockets[PACKET_LIMIT];

/** @brief Resolve the raw socket owned by a pinned description.
 * @param file Description or NULL.
 * @return Socket pointer or NULL for another descriptor kind.
 */
static struct packet_socket *packet_of(const struct task_file *file)
{
    return file && file->kind == TASK_FILE_KIND_PACKET ? (void *)(uintptr_t)file->aux : NULL;
}

/** @brief Queue a bounded copy of each matching ingress packet.
 * @param input Ethernet frame under the execution lock.
 * @param length Complete frame length.
 */
void net_packet_input(const void *input, uint32_t length)
{
    if (length < 14 || length > 1514) return;
    const uint8_t *frame = input;
    uint16_t protocol;
    __builtin_memcpy(&protocol, frame + 12, 2);
    for (unsigned i = 0; i < PACKET_LIMIT; ++i) {
        struct packet_socket *s = sockets[i];
        if (!s || s->queued >= PACKET_QUEUE_LIMIT) continue;
        uint32_t offset = s->domain == AF_PACKET && s->type == SOCK_RAW ? 0 : 14;
        uint32_t size = length - offset;
        uint32_t source = 0, destination = 0;
        if (s->domain == AF_PACKET) {
            if (!s->protocol || (s->protocol != (int)__builtin_bswap16(3) && s->protocol != protocol)) continue;
            if (s->ifindex && s->ifindex != 2) continue;
        } else {
            if (protocol != __builtin_bswap16(0x0800) || length < 34 || frame[14] >> 4 != 4 ||
                frame[23] != s->protocol) continue;
            uint32_t ihl = (frame[14] & 15u) * 4;
            uint32_t total = ((uint32_t)frame[16] << 8) | frame[17];
            if (ihl < 20 || total < ihl || total > length - 14) continue;
            __builtin_memcpy(&source, frame + 26, 4);
            __builtin_memcpy(&destination, frame + 30, 4);
            if ((s->local_ip && s->local_ip != destination) || (s->remote_ip && s->remote_ip != source)) continue;
            size = total;
        }
        struct packet_message *m = kernel_malloc(sizeof(*m) + size);
        if (!m) continue;
        *m = (struct packet_message){.length = size,
            .link = {.sll_family = AF_PACKET, .sll_protocol = protocol, .sll_ifindex = 2,
                .sll_hatype = 1, .sll_halen = 6,
                .sll_pkttype = frame[0] == 255 ? PACKET_BROADCAST : frame[0] & 1 ? PACKET_MULTICAST : PACKET_HOST},
            .ip = {.sin_family = AF_INET, .sin_addr.s_addr = source}};
        __builtin_memcpy(m->link.sll_addr, frame + 6, 6);
        __builtin_memcpy(m->data, frame + offset, size);
        if (s->tail) s->tail->next = m;
        else s->head = m;
        s->tail = m;
        ++s->queued;
    }
}

/** @brief Send a complete packet with native address validation.
 * @param file Pinned socket description.
 * @param data Validated packet payload.
 * @param length Payload bytes.
 * @param flags Linux message flags.
 * @param address Optional user sockaddr.
 * @param address_length Size of sockaddr.
 * @return Bytes accepted or negative errno.
 */
int task_packet_send(struct task_file *file, const void *data, uint32_t length,
                     uint32_t flags, uint64_t address, uint32_t address_length)
{
    struct packet_socket *s = packet_of(file);
    if (!s) return -LINUX_EBADF;
    if (flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL)) return -LINUX_EOPNOTSUPP;
    if (!net_interface_ready()) return -LINUX_ENETDOWN;
    if (s->domain == AF_INET) {
        uint32_t destination = s->remote_ip;
        if (address) {
            if ((int32_t)address_length < 0 || address_length < sizeof(struct sockaddr_in)) return -LINUX_EINVAL;
            if (!user_range_ok(address, sizeof(struct sockaddr_in))) return -LINUX_EFAULT;
            struct sockaddr_in to;
            __builtin_memcpy(&to, (void *)(uintptr_t)address, sizeof(to));
            if (to.sin_family != AF_INET) return -LINUX_EAFNOSUPPORT;
            destination = to.sin_addr.s_addr;
        }
        if (!destination) return -LINUX_EDESTADDRREQ;
        if (destination == UINT32_MAX && !s->broadcast) return -LINUX_EACCES;
        return net_ipv4_send_raw(__builtin_bswap32(destination), __builtin_bswap32(s->local_ip),
            (uint8_t)s->protocol, s->hdrincl, data, length);
    }
    struct sockaddr_ll to = {.sll_family = AF_PACKET, .sll_protocol = (uint16_t)s->protocol, .sll_ifindex = s->ifindex};
    if (address) {
        if ((int32_t)address_length < 0 || address_length < sizeof(to)) return -LINUX_EINVAL;
        if (!user_range_ok(address, sizeof(to))) return -LINUX_EFAULT;
        __builtin_memcpy(&to, (void *)(uintptr_t)address, sizeof(to));
        if (to.sll_family != AF_PACKET) return -LINUX_EINVAL;
    }
    if (to.sll_ifindex != 2) return -LINUX_ENXIO;
    uint8_t frame[1514];
    uint32_t total = length;
    if (s->type == SOCK_DGRAM) {
        if (length > 1500) return -LINUX_EMSGSIZE;
        if (to.sll_halen < 6 || to.sll_halen > 8) return -LINUX_EINVAL;
        __builtin_memcpy(frame, to.sll_addr, 6);
        __builtin_memcpy(frame + 6, e1000_mac(), 6);
        __builtin_memcpy(frame + 12, &to.sll_protocol, 2);
        __builtin_memcpy(frame + 14, data, length);
        total += 14;
    } else {
        if (length < 14) return -LINUX_EINVAL;
        if (length > sizeof(frame)) return -LINUX_EMSGSIZE;
        __builtin_memcpy(frame, data, length);
    }
    int ret = e1000_send(frame, total);
    return ret < 0 ? ret : (int)length;
}

/** @brief Receive a queued packet or wait interruptibly for ingress.
 * @param file Pinned socket description.
 * @param data Writable payload destination.
 * @param length Payload capacity.
 * @param flags Linux receive flags.
 * @param address Optional user source sockaddr destination.
 * @param address_length User socklen_t pointer.
 * @return Message bytes, negative errno or KERNEL_SYSCALL_BLOCKED.
 */
int task_packet_recv(struct task_file *file, void *data, uint32_t length,
                     uint32_t flags, uint64_t address, uint64_t address_length)
{
    struct packet_socket *s = packet_of(file);
    if (!s) return -LINUX_EBADF;
    if (flags & ~(MSG_DONTWAIT | MSG_PEEK | MSG_TRUNC | MSG_WAITALL)) return -LINUX_EOPNOTSUPP;
    net_poll_packets();
    if (!s->head) {
        if ((file->flags & LEONOS_O_NONBLOCK) || (flags & MSG_DONTWAIT)) return -LINUX_EAGAIN;
        struct task *task = sched_current_task();
        uint64_t now = time_ticks();
        if (!task->socket_io_deadline) {
            task->socket_io_timed = s->timeout != 0;
            task->socket_io_deadline = !s->timeout || UINT64_MAX - now < s->timeout ? UINT64_MAX : now + s->timeout;
        }
        if (now >= task->socket_io_deadline) return -LINUX_EAGAIN;
        sched_sleep_current_until(now + 1);
        return KERNEL_SYSCALL_BLOCKED;
    }
    struct packet_message *m = s->head;
    if (address) {
        if (!user_range_writable(address_length, 4)) return -LINUX_EFAULT;
        uint32_t capacity = *(uint32_t *)(uintptr_t)address_length;
        if ((int32_t)capacity < 0) return -LINUX_EINVAL;
        const void *source = s->domain == AF_PACKET ? (void *)&m->link : (void *)&m->ip;
        uint32_t size = s->domain == AF_PACKET ? sizeof(m->link) : sizeof(m->ip);
        uint32_t copied = capacity < size ? capacity : size;
        if (copied && !user_range_writable(address, copied)) return -LINUX_EFAULT;
        if (copied) __builtin_memcpy((void *)(uintptr_t)address, source, copied);
        *(uint32_t *)(uintptr_t)address_length = size;
    }
    uint32_t copied = length < m->length ? length : m->length;
    if (copied) __builtin_memcpy(data, m->data, copied);
    int result = flags & MSG_TRUNC ? (int)m->length : (int)copied;
    if (!(flags & MSG_PEEK)) {
        s->head = m->next;
        if (!s->head) s->tail = NULL;
        --s->queued;
        kernel_free(m);
    }
    return result;
}

/** @brief Query packet readiness under the execution lock.
 * @param file Pinned socket.
 * @param events Requested readiness bits.
 * @return Readiness bits or POLLNVAL.
 */
short task_packet_poll(struct task_file *file, short events)
{
    struct packet_socket *s = packet_of(file);
    if (!s) return POLLNVAL;
    net_poll_packets();
    return (events & POLLOUT) | (s->head ? events & POLLIN : 0);
}

/** @brief Query the next datagram length.
 * @param file Pinned socket.
 * @return Length, zero, or EBADF.
 */
int task_packet_available(struct task_file *file)
{
    struct packet_socket *s = packet_of(file);
    return !s ? -LINUX_EBADF : s->head ? (int)s->head->length : 0;
}

/** @brief Free a final socket description and its bounded receive queue.
 * @param file Owned description under the execution lock.
 */
void task_packet_release(struct task_file *file)
{
    struct packet_socket *s = packet_of(file);
    if (!s) return;
    for (unsigned i = 0; i < PACKET_LIMIT; ++i) if (sockets[i] == s) sockets[i] = NULL;
    while (s->head) { struct packet_message *m = s->head; s->head = m->next; kernel_free(m); }
    kernel_free(s);
    file->aux = 0;
}

/** @brief Handle raw socket creation, binding and options with Linux-sized arguments.
 * @param number Native syscall number.
 * @param a0 First native argument.
 * @param a1 Second native argument.
 * @param a2 Third native argument.
 * @param a3 Fourth native argument.
 * @param a4 Fifth native argument.
 * @param a5 Sixth native argument.
 * @return Native result or negative errno; unsupported options are not accepted.
 */
int64_t syscall_packet(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2,
                       uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a5;
    struct task *task = sched_current_task();
    if (!task) return -LINUX_ESRCH;
    if (number == __NR_socket) {
        int type = (int)a1 & 15;
        if ((uint32_t)a1 & ~(uint32_t)(15 | SOCK_NONBLOCK | SOCK_CLOEXEC)) return -LINUX_EINVAL;
        if (type != SOCK_RAW && !((int)a0 == AF_PACKET && type == SOCK_DGRAM)) return -LINUX_ESOCKTNOSUPPORT;
        if ((int)a0 == AF_INET && ((int)a2 <= 0 || (int)a2 > 255)) return -LINUX_EPROTONOSUPPORT;
        if (!(task->cap_effective & (1ULL << CAP_NET_RAW))) return -LINUX_EPERM;
        unsigned index = 0;
        while (index < PACKET_LIMIT && sockets[index]) ++index;
        if (index == PACKET_LIMIT) return -LINUX_ENFILE;
        struct packet_socket *s = kernel_malloc(sizeof(*s));
        if (!s) return -LINUX_ENOMEM;
        struct task_file *file;
        int fd = task_allocate_fd(task, 0, &file);
        if (fd < 0) { kernel_free(s); return fd; }
        *s = (struct packet_socket){.domain = (int)a0, .type = type, .protocol = (int)a2,
            .hdrincl = (int)a0 == AF_INET && (int)a2 == 255};
        sockets[index] = s;
        file->flags = TASK_FILE_FLAG_SOCKET | TASK_FILE_FLAG_SOCKET_INET | LEONOS_O_RDWR | (a1 & SOCK_NONBLOCK ? LEONOS_O_NONBLOCK : 0);
        file->fd_flags = a1 & SOCK_CLOEXEC ? LEONOS_FD_CLOEXEC : 0;
        file->kind = TASK_FILE_KIND_PACKET;
        file->aux = (uintptr_t)s;
        file->node.type = LEONOS_FS_TYPE_DEVICE;
        return fd;
    }
    struct task_file *file = task_file_for_fd(task, (int32_t)a0);
    struct packet_socket *s = packet_of(file);
    if (!s) return -LINUX_EBADF;
    if (number == __NR_bind || number == __NR_connect) {
        if (s->domain == AF_PACKET) {
            if (number == __NR_connect) return -LINUX_EOPNOTSUPP;
            if ((int32_t)a2 < 0 || (uint32_t)a2 < sizeof(struct sockaddr_ll)) return -LINUX_EINVAL;
            if (!user_range_ok(a1, sizeof(struct sockaddr_ll))) return -LINUX_EFAULT;
            struct sockaddr_ll address;
            __builtin_memcpy(&address, (void *)(uintptr_t)a1, sizeof(address));
            if (address.sll_family != AF_PACKET) return -LINUX_EINVAL;
            if (address.sll_ifindex && address.sll_ifindex != 2) return -LINUX_ENODEV;
            s->ifindex = address.sll_ifindex;
            s->protocol = address.sll_protocol;
        } else {
            if ((int32_t)a2 < 0 || (uint32_t)a2 < sizeof(struct sockaddr_in)) return -LINUX_EINVAL;
            if (!user_range_ok(a1, sizeof(struct sockaddr_in))) return -LINUX_EFAULT;
            struct sockaddr_in address;
            __builtin_memcpy(&address, (void *)(uintptr_t)a1, sizeof(address));
            if (address.sin_family != AF_INET) return -LINUX_EAFNOSUPPORT;
            if (number == __NR_connect) s->remote_ip = address.sin_addr.s_addr;
            else {
                struct leonos_net_config config;
                net_get_config(&config);
                if (address.sin_addr.s_addr && address.sin_addr.s_addr != __builtin_bswap32(config.local_ip))
                    return -LINUX_EADDRNOTAVAIL;
                s->local_ip = address.sin_addr.s_addr;
            }
        }
        return 0;
    }
    if (number == __NR_setsockopt) {
        if ((int)a1 == SOL_SOCKET && ((int)a2 == SO_RCVTIMEO || (int)a2 == SO_RCVTIMEO_NEW)) {
            int64_t timeout[2];
            if ((uint32_t)a4 < sizeof(timeout)) return -LINUX_EINVAL;
            if (!user_range_ok(a3, sizeof(timeout))) return -LINUX_EFAULT;
            __builtin_memcpy(timeout, (void *)(uintptr_t)a3, sizeof(timeout));
            if (timeout[1] < 0 || timeout[1] >= 1000000) return -LINUX_EDOM;
            if (timeout[0] < 0) s->timeout = 1;
            else if ((uint64_t)timeout[0] > (UINT64_MAX - NTCLKS_TICK_HZ) / NTCLKS_TICK_HZ) s->timeout = UINT64_MAX;
            else s->timeout = timeout[0] * NTCLKS_TICK_HZ + (timeout[1] * NTCLKS_TICK_HZ + 999999) / 1000000;
            return 0;
        }
        if (!((int)a1 == SOL_SOCKET && (int)a2 == 6) &&
            !((int)a1 == 0 && (int)a2 == 3 && s->domain == AF_INET)) return -LINUX_ENOPROTOOPT;
        if ((uint32_t)a4 < 4) return -LINUX_EINVAL;
        if (!user_range_ok(a3, 4)) return -LINUX_EFAULT;
        bool value = *(int *)(uintptr_t)a3 != 0;
        if ((int)a1 == SOL_SOCKET) s->broadcast = value;
        else s->hdrincl = value;
        return 0;
    }
    return -LINUX_EOPNOTSUPP;
}
