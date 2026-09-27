#ifndef RELIEFOS_NET_H
#define RELIEFOS_NET_H

/*
 * Userland network client API. The wire types and constants moved to the
 * kernel UAPI (<reliefos/net_abi.h>); this header re-exports them so existing
 * `#include <reliefos/net.h>` callers keep working.
 */
#include <reliefos/net_abi.h>
#include <stdint.h>

int reliefos_net_config(struct reliefos_net_config *config);
int reliefos_net_get_dns_policy(struct reliefos_net_dns_policy *result);
int reliefos_net_set_dns_policy(uint32_t mode, uint32_t custom_dns_ip,
                              struct reliefos_net_dns_policy *result);
int reliefos_net_dhcp_renew(uint32_t timeout_ms, struct reliefos_net_dhcp *result);
int reliefos_net_ping(uint32_t target_ip, uint32_t timeout_ms,
                    struct reliefos_net_ping *result);
int reliefos_net_dns_resolve(const char *name, uint32_t timeout_ms,
                           struct reliefos_net_dns *result);
int reliefos_net_http_get(const char *host, const char *path,
                        uint32_t port, uint32_t timeout_ms,
                        struct reliefos_net_http_get *result);
int reliefos_socket_tcp(void);
int reliefos_socket_connect(int socket, const char *host,
                          uint32_t port, uint32_t timeout_ms,
                          struct reliefos_net_socket_connect *result);
long reliefos_socket_send(int socket, const void *buffer, uint32_t length,
                        uint32_t timeout_ms, uint32_t *status);
long reliefos_socket_recv(int socket, void *buffer, uint32_t length,
                        uint32_t timeout_ms, uint32_t *status);
int reliefos_socket_close(int socket);
int reliefos_net_connections(struct reliefos_net_connection_info *entries,
                           uint32_t capacity, uint32_t *out_count);

#endif
