#ifndef SERVICED_NETMAND_H
#define SERVICED_NETMAND_H
#include <leonos/net.h>

void netmand_poll(void);
int netmand_config(struct leonos_net_config *config);
int netmand_dhcp(uint32_t timeout, struct leonos_net_dhcp *result);

#endif
