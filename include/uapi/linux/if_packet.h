#ifndef LEONOS_UAPI_LINUX_IF_PACKET_H
#define LEONOS_UAPI_LINUX_IF_PACKET_H
#include <linux/types.h>
/* Linux v6.12 native sockaddr_ll layout; protocol is in network byte order. */
struct sockaddr_ll {
    uint16_t sll_family, sll_protocol;
    int32_t sll_ifindex;
    uint16_t sll_hatype;
    uint8_t sll_pkttype, sll_halen;
    uint8_t sll_addr[8];
};
#define PACKET_HOST 0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#define PACKET_OTHERHOST 3
#define PACKET_OUTGOING 4
#endif
