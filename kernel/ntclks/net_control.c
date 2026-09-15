#include <leonos/net_control.h>
#include <ntclks/net.h>
#include <ntclks/net_udp.h>
#include <ntclks/net_packet.h>
#include <ntclks/sched.h>
#include <ntclks/syscall.h>
#include <ntclks/usercopy.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/if.h>

int task_net_control(struct task_file *file, uint32_t request, uint64_t address)
{
    if (!file || !(file->flags & TASK_FILE_FLAG_SOCKET_INET)) return -LINUX_ENOTTY;
    if (request == FIONBIO) {
        if (!user_range_ok(address, sizeof(int))) return -LINUX_EFAULT;
        if (*(int *)(uintptr_t)address) file->flags |= LEONOS_O_NONBLOCK;
        else file->flags &= ~LEONOS_O_NONBLOCK;
        return 0;
    }
    if (request == FIONREAD) {
        if (!user_range_writable(address, sizeof(int))) return -LINUX_EFAULT;
        net_poll_packets();
        int bytes = file->kind == TASK_FILE_KIND_PACKET ? task_packet_available(file) : file->kind == TASK_FILE_KIND_UDP ? task_udp_available(file) :
            net_socket_available((int32_t)file->aux);
        if (bytes < 0) return bytes;
        *(int *)(uintptr_t)address = bytes;
        return 0;
    }
    if (request >= 0x8900 && request <= 0x89ff)
        return net_interface_ioctl(request, address);
    if (request != LEONOS_NET_CONTROL_IOCTL) return -LINUX_ENOTTY;
    struct leonos_net_control control;
    if (!user_range_ok(address, sizeof(control)) || !user_range_writable(address, sizeof(control)))
        return -LINUX_EFAULT;
    __builtin_memcpy(&control, (void *)(uintptr_t)address, sizeof(control));
    if (control.version != LEONOS_NET_CONTROL_VERSION || control.reserved) return -LINUX_EINVAL;
    struct task *task = sched_current_task();
    if (!task) return -LINUX_EPERM;
    bool change = control.operation == LEONOS_NET_CONTROL_DHCP ||
        (control.operation == LEONOS_NET_CONTROL_DNS_POLICY &&
         control.data.dns_policy.mode != LEONOS_NET_DNS_MODE_QUERY);
    if (change && !(task->cap_effective & (1ULL << CAP_NET_ADMIN))) return -LINUX_EPERM;
    switch (control.operation) {
    case LEONOS_NET_CONTROL_CONFIG:
        control.result = net_get_config(&control.data.config);
        break;
    case LEONOS_NET_CONTROL_DNS_POLICY:
        control.result = net_set_dns_policy(&control.data.dns_policy);
        break;
    case LEONOS_NET_CONTROL_DHCP:
        /* DHCP is an upstream userspace client; there is no kernel fallback. */
        control.result = -LINUX_EOPNOTSUPP;
        break;
    case LEONOS_NET_CONTROL_PING:
        control.result = net_ping(&control.data.ping);
        break;
    case LEONOS_NET_CONTROL_CONNECTIONS: {
        struct leonos_net_connection_list list = {
            .capacity = LEONOS_NET_SOCKET_MAX, .entries = control.data.connections.entries,
        };
        __builtin_memset(&control.data, 0, sizeof(control.data));
        control.result = net_connections(&list, task);
        control.data.connections.count = list.count;
        break;
    }
    default: return -LINUX_EOPNOTSUPP;
    }
    __builtin_memcpy((void *)(uintptr_t)address, &control, sizeof(control));
    return 0;
}
