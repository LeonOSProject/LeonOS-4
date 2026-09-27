#ifndef RELIEFOS_NET_SERVICE_H
#define RELIEFOS_NET_SERVICE_H

/* Versioned ReliefOS network service SDK.
 *
 * Consumers talk to the kernel query adapter and authenticated OpenRC commands
 * through this library; /dev/net0 and the fd 3 network channel are gone.
 * Data traffic uses standard AF_INET sockets.
 */
#include <reliefos/net.h>
#include <stdint.h>

#define NET_SERVICE_ABI_VERSION 1U

typedef struct reliefos_net_config net_service_config_t;
typedef struct reliefos_net_dns_policy net_service_dns_policy_t;
typedef struct reliefos_net_dhcp net_service_dhcp_t;
typedef struct reliefos_net_ping net_service_ping_t;
typedef struct reliefos_net_dns net_service_dns_t;
typedef struct reliefos_net_http_get net_service_http_get_t;
typedef struct reliefos_net_socket_open net_service_socket_open_t;
typedef struct reliefos_net_socket_connect net_service_socket_connect_t;
typedef struct reliefos_net_socket_io net_service_socket_io_t;
typedef struct reliefos_net_socket_close net_service_socket_close_t;
typedef struct reliefos_net_connection_info net_service_connection_info_t;
typedef struct reliefos_net_connection_list net_service_connection_list_t;

#define NET_SERVICE_AF_INET RELIEFOS_NET_AF_INET
#define NET_SERVICE_SOCK_STREAM RELIEFOS_NET_SOCK_STREAM
#define NET_SERVICE_IPPROTO_TCP RELIEFOS_NET_IPPROTO_TCP

#define NET_SERVICE_STATUS_OK RELIEFOS_NET_STATUS_OK
#define NET_SERVICE_STATUS_NO_DEVICE RELIEFOS_NET_STATUS_NO_DEVICE
#define NET_SERVICE_STATUS_ARP_TIMEOUT RELIEFOS_NET_STATUS_ARP_TIMEOUT
#define NET_SERVICE_STATUS_ECHO_TIMEOUT RELIEFOS_NET_STATUS_ECHO_TIMEOUT
#define NET_SERVICE_STATUS_BAD_ARGUMENT RELIEFOS_NET_STATUS_BAD_ARGUMENT
#define NET_SERVICE_STATUS_TX_FAILED RELIEFOS_NET_STATUS_TX_FAILED
#define NET_SERVICE_STATUS_DHCP_TIMEOUT RELIEFOS_NET_STATUS_DHCP_TIMEOUT
#define NET_SERVICE_STATUS_DHCP_FAILED RELIEFOS_NET_STATUS_DHCP_FAILED
#define NET_SERVICE_STATUS_DNS_TIMEOUT RELIEFOS_NET_STATUS_DNS_TIMEOUT
#define NET_SERVICE_STATUS_DNS_FAILED RELIEFOS_NET_STATUS_DNS_FAILED
#define NET_SERVICE_STATUS_DNS_NO_ANSWER RELIEFOS_NET_STATUS_DNS_NO_ANSWER
#define NET_SERVICE_STATUS_TCP_TIMEOUT RELIEFOS_NET_STATUS_TCP_TIMEOUT
#define NET_SERVICE_STATUS_TCP_RESET RELIEFOS_NET_STATUS_TCP_RESET
#define NET_SERVICE_STATUS_TCP_FAILED RELIEFOS_NET_STATUS_TCP_FAILED
#define NET_SERVICE_STATUS_HTTP_FAILED RELIEFOS_NET_STATUS_HTTP_FAILED
#define NET_SERVICE_STATUS_HTTP_TOO_LARGE RELIEFOS_NET_STATUS_HTTP_TOO_LARGE
#define NET_SERVICE_STATUS_SOCKET_LIMIT RELIEFOS_NET_STATUS_SOCKET_LIMIT
#define NET_SERVICE_STATUS_SOCKET_BAD_HANDLE RELIEFOS_NET_STATUS_SOCKET_BAD_HANDLE
#define NET_SERVICE_STATUS_SOCKET_NOT_CONNECTED RELIEFOS_NET_STATUS_SOCKET_NOT_CONNECTED
#define NET_SERVICE_STATUS_SOCKET_CLOSED RELIEFOS_NET_STATUS_SOCKET_CLOSED
#define NET_SERVICE_STATUS_PROTOCOL_UNSUPPORTED RELIEFOS_NET_STATUS_PROTOCOL_UNSUPPORTED
#define NET_SERVICE_STATUS_TLS_FAILED RELIEFOS_NET_STATUS_TLS_FAILED

#define NET_SERVICE_DNS_MODE_QUERY RELIEFOS_NET_DNS_MODE_QUERY
#define NET_SERVICE_DNS_MODE_CLOUDFLARE RELIEFOS_NET_DNS_MODE_CLOUDFLARE
#define NET_SERVICE_DNS_MODE_DHCP RELIEFOS_NET_DNS_MODE_DHCP
#define NET_SERVICE_DNS_MODE_CUSTOM RELIEFOS_NET_DNS_MODE_CUSTOM
#define NET_SERVICE_CLOUDFLARE_DNS_IP RELIEFOS_NET_CLOUDFLARE_DNS_IP

#define NET_SERVICE_TCP_SYN_SENT RELIEFOS_NET_TCP_SYN_SENT
#define NET_SERVICE_TCP_ESTABLISHED RELIEFOS_NET_TCP_ESTABLISHED
#define NET_SERVICE_TCP_TIME_WAIT RELIEFOS_NET_TCP_TIME_WAIT
#define NET_SERVICE_TCP_CLOSED RELIEFOS_NET_TCP_CLOSED

#define NET_SERVICE_CONFIG_SOURCE_DHCP RELIEFOS_NET_CONFIG_SOURCE_DHCP
#define NET_SERVICE_CONFIG_SOURCE_STATIC RELIEFOS_NET_CONFIG_SOURCE_STATIC
#define NET_SERVICE_CONFIG_FLAG_ACTIVE RELIEFOS_NET_CONFIG_FLAG_ACTIVE
#define NET_SERVICE_CONFIG_FLAG_DHCP RELIEFOS_NET_CONFIG_FLAG_DHCP

#define NET_SERVICE_DEFAULT_TIMEOUT_MS RELIEFOS_NET_DEFAULT_TIMEOUT_MS
#define NET_SERVICE_HOSTNAME_LEN RELIEFOS_NET_HOSTNAME_LEN
#define NET_SERVICE_HTTP_PATH_LEN RELIEFOS_NET_HTTP_PATH_LEN
#define NET_SERVICE_SOCKET_MAX RELIEFOS_NET_SOCKET_MAX

int net_service_config(net_service_config_t *config);
int net_service_get_dns_policy(net_service_dns_policy_t *result);
int net_service_set_dns_policy(uint32_t mode, uint32_t custom_dns_ip,
                               net_service_dns_policy_t *result);
int net_service_dhcp_renew(uint32_t timeout_ms, net_service_dhcp_t *result);
int net_service_ping(uint32_t target_ip, uint32_t timeout_ms,
                     net_service_ping_t *result);
int net_service_dns_resolve(const char *name, uint32_t timeout_ms,
                            net_service_dns_t *result);
int net_service_connections(net_service_connection_info_t *entries,
                            uint32_t capacity, uint32_t *out_count);

#endif
