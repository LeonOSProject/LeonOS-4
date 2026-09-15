#ifndef LEONOS_UAPI_LINUX_IF_H
#define LEONOS_UAPI_LINUX_IF_H
#include <linux/socket.h>
#define IFNAMSIZ 16
#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_RUNNING 0x40
#define IFF_MULTICAST 0x1000
/* Native x86-64 Linux v6.12 ifreq/ifconf layouts. */
struct ifreq {
    char ifr_name[IFNAMSIZ];
    union { struct sockaddr addr; int32_t value; int16_t flags; uint8_t pad[24]; } data;
};
struct ifconf { int32_t ifc_len; uint64_t ifc_buf; };
struct rtentry {
    uint64_t rt_pad1;
    struct sockaddr rt_dst, rt_gateway, rt_genmask;
    uint16_t rt_flags;
    int16_t rt_pad2;
    uint64_t rt_pad3, rt_pad4;
    int16_t rt_metric;
    uint64_t rt_dev, rt_mtu, rt_window;
    uint16_t rt_irtt;
};
#define SIOCADDRT 0x890b
#define SIOCDELRT 0x890c
#define SIOCGIFNAME 0x8910
#define SIOCGIFCONF 0x8912
#define SIOCGIFFLAGS 0x8913
#define SIOCSIFFLAGS 0x8914
#define SIOCGIFADDR 0x8915
#define SIOCSIFADDR 0x8916
#define SIOCGIFBRDADDR 0x8919
#define SIOCSIFBRDADDR 0x891a
#define SIOCGIFNETMASK 0x891b
#define SIOCSIFNETMASK 0x891c
#define SIOCGIFMTU 0x8921
#define SIOCSIFMTU 0x8922
#define SIOCGIFHWADDR 0x8927
#define SIOCGIFINDEX 0x8933
_Static_assert(sizeof(struct ifreq) == 40, "Linux native ifreq");
_Static_assert(sizeof(struct ifconf) == 16, "Linux native ifconf");
_Static_assert(sizeof(struct rtentry) == 120, "Linux native rtentry");
#endif
