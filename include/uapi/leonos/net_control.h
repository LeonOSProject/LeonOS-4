#ifndef LEONOS_UAPI_NET_CONTROL_H
#define LEONOS_UAPI_NET_CONTROL_H
#include <leonos/net.h>
#include <linux/ioctl.h>

/* LeonOS management extension on an AF_INET socket. Application data uses
 * standard Linux socket syscalls; this does not occupy a Linux syscall ID. */
#define LEONOS_NET_CONTROL_VERSION 1u
#define LEONOS_NET_CONTROL_CONFIG 1u
#define LEONOS_NET_CONTROL_DNS_POLICY 2u
#define LEONOS_NET_CONTROL_DHCP 3u
#define LEONOS_NET_CONTROL_PING 4u
#define LEONOS_NET_CONTROL_CONNECTIONS 5u
struct leonos_net_control {
    uint32_t version, operation;
    int32_t result;
    uint32_t reserved;
    union {
        struct leonos_net_config config;
        struct leonos_net_dns_policy dns_policy;
        struct leonos_net_dhcp dhcp;
        struct leonos_net_ping ping;
        struct {
            uint32_t count, reserved;
            struct leonos_net_connection_info entries[LEONOS_NET_SOCKET_MAX];
        } connections;
    } data;
};
#define LEONOS_NET_CONTROL_IOCTL _IOWR('L', 0x70, struct leonos_net_control)
#endif
