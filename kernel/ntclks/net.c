/*
 * LeonOS kernel networking: provides the network and socket service layer.
 * Handles interfaces, packet transport, TCP state, and user-facing requests.
 */
#include <ntclks/console.h>
#include <ntclks/e1000.h>
#include <ntclks/heap.h>
#include <ntclks/net.h>
#include <ntclks/net_packet.h>
#include <ntclks/net_udp.h>
#include <linux/errno.h>
#include <linux/if.h>
#include <linux/capability.h>
#include <ntclks/usercopy.h>
#include <ntclks/sched.h>
#include <linux/poll.h>
#include <ntclks/storage.h>
#include <ntclks/time.h>
#include <leonos/layout.h>

#include <leonos/device_abi.h>

#define ETH_TYPE_IPV4 0x0800u
#define ETH_TYPE_ARP 0x0806u
#define ARP_HTYPE_ETHERNET 0x0001u
#define ARP_OPER_REQUEST 0x0001u
#define ARP_OPER_REPLY 0x0002u
#define IPV4_PROTO_ICMP 1u
#define IPV4_PROTO_TCP 6u
#define IPV4_PROTO_UDP 17u
#define ICMP_ECHO_REPLY 0u
#define ICMP_ECHO_REQUEST 8u
#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u
#define NET_FRAME_MAX 1536u
#define NET_ICMP_PAYLOAD_LEN 16u
#define NET_DHCP_PACKET_MAX 548u
#define NET_DHCP_PACKET_LEN 300u
#define NET_DNS_PACKET_MAX 512u
#define NET_NTP_PACKET_LEN 48u
#define NET_HTTP_REQUEST_MAX 640u
#define NET_DHCP_CLIENT_PORT 68u
#define NET_DHCP_SERVER_PORT 67u
#define NET_DNS_PORT 53u
#define NET_NTP_PORT 123u
#define NET_HTTP_PORT 80u
#define NET_DHCP_MAGIC 0x63825363u
#define NET_DHCP_DISCOVER 1u
#define NET_DHCP_OFFER 2u
#define NET_DHCP_REQUEST 3u
#define NET_DHCP_ACK 5u
#define NET_DHCP_NAK 6u
#define NET_BOOT_DHCP_ATTEMPTS 3u
#define NET_BOOT_DHCP_TIMEOUT_MS 4000u
#define NET_TCP_MSS 1460u
#define NET_TCP_SYN_RETRANSMIT_MS 500u
#define NET_TCP_DATA_RETRANSMIT_MS 750u
#define NET_ARP_CACHE_SIZE 8u
#define NET_HTTP_DEFAULT_TIMEOUT_MS 8000u
#define NET_HTTP_MAX_TIMEOUT_MS 10000u
/* The full unscaled TCP window; scaling is not negotiated yet. */
#define NET_SOCKET_RX_CAP 65535u
#ifndef NET_TCP_TRACE
#define NET_TCP_TRACE 0
#endif
#define NET_SOCKET_CLOSE_HOLD_MS 10000u
#define NET_SOCKET_DEFAULT_TIMEOUT_MS 5000u
#define NET_NETWORK_CONFIG_PATH LEONOS_PATH_NETWORK_CONF
#define NET_NETWORK_CONFIG_BACKUP_PATH LEONOS_PATH_NETWORK_BAK
#define NET_NETWORK_CONFIG_MAX 256u
#define NET_NTP_UNIX_EPOCH_OFFSET 2208988800ULL

struct net_arp_wait {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t done;
};

struct net_ping_wait {
    uint32_t target_ip;
    uint16_t ident;
    uint16_t sequence;
    uint32_t done;
};

struct net_udp_wait {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t *payload;
    uint32_t capacity;
    uint32_t length;
    uint32_t done;
};

struct net_tcp_wait {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t remote_seq;
    uint32_t acked_seq;
    uint8_t *payload;
    uint32_t capacity;
    uint32_t length;
    uint32_t flags;
    uint32_t reset;
    uint32_t fin;
    uint32_t overflow;
    uint32_t changed;
};

struct net_dhcp_offer {
    uint32_t msg_type;
    uint32_t yiaddr;
    uint32_t subnet_mask;
    uint32_t router_ip;
    uint32_t dns_ip;
    uint32_t server_ip;
    uint32_t lease_seconds;
};

struct net_arp_cache_entry {
    uint32_t ip;
    uint8_t mac[6];
};

struct net_socket {
    uint32_t used;
    bool fd_owned;
    bool shutdown_read;
    bool shutdown_write;
    int error;
    uint64_t syn_deadline;
    uint64_t syn_retransmit;
    int32_t handle;
    uint32_t owner_pid;
    uint32_t owner_uid;
    uint32_t state;
    uint32_t status;
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t local_seq;
    uint32_t remote_seq;
    uint32_t acked_seq;
    uint32_t rx_len;
    uint32_t rx_head;
    uint32_t rx_window_ack;
    uint32_t rx_window;
    bool rx_window_valid;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint32_t created_ms;
    uint32_t changed_ms;
    uint32_t fin_received;
    uint8_t dst_mac[6];
    uint8_t *rx;
};

static const uint8_t net_broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint32_t net_sequence = 1;
static uint16_t net_ipv4_id = 1;
static struct leonos_net_config net_config;
static bool net_interface_up = true;
static uint32_t net_interface_broadcast;
static uint16_t net_packet_ip_id;
static uint32_t net_dns_mode = LEONOS_NET_DNS_MODE_DHCP;
static uint32_t net_dns_custom_ip;
static uint32_t net_dhcp_dns_ip;
static uint64_t net_dhcp_acquired_ms;
static struct net_arp_cache_entry net_arp_cache[NET_ARP_CACHE_SIZE];
static uint32_t net_arp_cache_next;
static struct net_socket net_sockets[LEONOS_NET_SOCKET_MAX];
static int32_t net_next_socket_handle = 1;

/**
 * Net socket handle tcp.
 * @param src_ip Value supplied by the caller.
 * @param src_port Value supplied by the caller.
 * @param dst_port Value supplied by the caller.
 * @param seq Value supplied by the caller.
 * @param ack Value supplied by the caller.
 * @param flags Identifier or flags controlling the operation.
 * @param payload Value supplied by the caller.
 * @param payload_len Value supplied by the caller.
 */
static void net_socket_handle_tcp(uint32_t src_ip, uint16_t src_port,
                                  uint16_t dst_port, uint32_t seq,
                                  uint32_t ack, uint8_t flags,
                                  const uint8_t *payload,
                                  uint32_t payload_len);

/**
 * Net cpu relax.
 */
static void net_cpu_relax(void)
{
    __asm__ volatile("sti; hlt; cli");
}

/**
 * Net memcpy.
 * @param dst Value supplied by the caller.
 * @param src Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 */
static void net_memcpy(void *dst, const void *src, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (len--) {
        *d++ = *s++;
    }
}

/**
 * Net memzero.
 * @param dst Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 */
static void net_memzero(void *dst, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    while (len--) {
        *d++ = 0;
    }
}

/**
 * Net strlen.
 * @param text NUL-terminated text supplied by the caller.
 * @param cap Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static uint32_t net_strlen(const char *text, uint32_t cap)
{
    uint32_t len = 0;
    while (text && len < cap && text[len]) {
        ++len;
    }
    return len;
}

/**
 * Net mac is zero.
 * @param mac Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_mac_is_zero(const uint8_t mac[6])
{
    return (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0;
}

/**
 * Net memeq.
 * @param a Value supplied by the caller.
 * @param b Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static int net_memeq(const uint8_t *a, const uint8_t *b, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/**
 * Net text eq len.
 * @param a Value supplied by the caller.
 * @param b Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static int net_text_eq_len(const char *a, const char *b, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        if (!a || !b || a[i] != b[i]) {
            return 0;
        }
    }
    return b[len] == 0;
}


/**
 * Net line value.
 * @param line Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param key Value supplied by the caller.
 * @param out_value Value supplied by the caller.
 * @param out_len Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_line_value(const char *line, uint32_t len, const char *key,
                          const char **out_value, uint32_t *out_len)
{
    uint32_t key_len = net_strlen(key, NET_NETWORK_CONFIG_MAX);
    if (!line || !key || !out_value || !out_len || !key_len || len <= key_len ||
        line[key_len] != '=' || !net_text_eq_len(line, key, key_len)) {
        return 0;
    }
    *out_value = line + key_len + 1u;
    *out_len = len - key_len - 1u;
    return 1;
}

/**
 * Net value eq.
 * @param value Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param expected Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_value_eq(const char *value, uint32_t len, const char *expected)
{
    uint32_t index = 0;
    while (expected && expected[index]) {
        if (index >= len || value[index] != expected[index]) {
            return 0;
        }
        ++index;
    }
    return index == len;
}

/**
 * Net parse ipv4.
 * @param text NUL-terminated text supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param out_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_parse_ipv4(const char *text, uint32_t len, uint32_t *out_ip)
{
    uint32_t ip = 0;
    uint32_t pos = 0;
    for (uint32_t octet = 0; octet < 4u; ++octet) {
        uint32_t value = 0;
        uint32_t digits = 0;
        while (pos < len && text[pos] >= '0' && text[pos] <= '9') {
            value = value * 10u + (uint32_t)(text[pos] - '0');
            if (value > 255u || ++digits > 3u) {
                return 0;
            }
            ++pos;
        }
        if (!digits) {
            return 0;
        }
        ip = (ip << 8) | value;
        if (octet != 3u) {
            if (pos >= len || text[pos++] != '.') {
                return 0;
            }
        }
    }
    if (pos != len || ip == 0 || ip == 0xffffffffu) {
        return 0;
    }
    *out_ip = ip;
    return 1;
}

/**
 * Net load dns policy file.
 * @param path NUL-terminated text supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_load_dns_policy_file(const char *path)
{
    struct storage_node node;
    char cfg[NET_NETWORK_CONFIG_MAX];
    uint32_t got = 0;
    uint32_t pos = 0;

    if (!path || !storage_ready() ||
        storage_lookup_path(path, &node) < 0 ||
        node.type != LEONOS_FS_TYPE_FILE) {
        return -1;
    }
    if (storage_read_node(&node, 0, cfg, sizeof(cfg) - 1u, &got) < 0) {
        return -1;
    }
    cfg[got] = 0;
    while (pos < got) {
        const char *value;
        uint32_t value_len;
        uint32_t start = pos;
        uint32_t line_len;
        while (pos < got && cfg[pos] != '\n' && cfg[pos] != '\r') {
            ++pos;
        }
        line_len = pos - start;
        while (pos < got && (cfg[pos] == '\n' || cfg[pos] == '\r')) {
            ++pos;
        }
        if (net_line_value(cfg + start, line_len, "dns_mode", &value, &value_len)) {
            if (net_value_eq(value, value_len, "dhcp")) {
                net_dns_mode = LEONOS_NET_DNS_MODE_DHCP;
            } else if (net_value_eq(value, value_len, "custom")) {
                net_dns_mode = LEONOS_NET_DNS_MODE_CUSTOM;
            } else if (net_value_eq(value, value_len, "cloudflare")) {
                net_dns_mode = LEONOS_NET_DNS_MODE_CLOUDFLARE;
            }
        } else if (net_line_value(cfg + start, line_len, "dns_custom", &value, &value_len)) {
            uint32_t parsed_ip;
            if (net_parse_ipv4(value, value_len, &parsed_ip)) {
                net_dns_custom_ip = parsed_ip;
            }
        }
    }
    return 0;
}

/**
 * Net load dns policy.
 */
static void net_load_dns_policy(void)
{
    net_dns_mode = LEONOS_NET_DNS_MODE_DHCP;
    net_dns_custom_ip = 0;
    if (net_load_dns_policy_file(NET_NETWORK_CONFIG_PATH) < 0) {
        (void)net_load_dns_policy_file(NET_NETWORK_CONFIG_BACKUP_PATH);
    }
    if (net_dns_mode == LEONOS_NET_DNS_MODE_CUSTOM && !net_dns_custom_ip) {
        net_dns_mode = LEONOS_NET_DNS_MODE_CLOUDFLARE;
    }
}

/**
 * Net effective dns ip.
 * @param dhcp_dns_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_effective_dns_ip(uint32_t dhcp_dns_ip)
{
    if (net_dns_mode == LEONOS_NET_DNS_MODE_CUSTOM && net_dns_custom_ip) {
        return net_dns_custom_ip;
    }
    if (net_dns_mode == LEONOS_NET_DNS_MODE_DHCP) {
        return dhcp_dns_ip;
    }
    return LEONOS_NET_CLOUDFLARE_DNS_IP;
}


/**
 * Net get u16.
 * @param p Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint16_t net_get_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/**
 * Net get u32.
 * @param p Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

/**
 * Net put u16.
 * @param p Value supplied by the caller.
 * @param value Value supplied by the caller.
 */
static void net_put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

/**
 * Net put u32.
 * @param p Value supplied by the caller.
 * @param value Value supplied by the caller.
 */
static void net_put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

/**
 * Net tcp seq after or equal.
 * @param a Value supplied by the caller.
 * @param b Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_tcp_seq_after_or_equal(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) >= 0;
}

/**
 * Net checksum.
 * @param data Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static uint16_t net_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0;
    while (len > 1) {
        sum += net_get_u16(data);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)data[0] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/**
 * Net checksum partial.
 * @param sum Value supplied by the caller.
 * @param data Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static uint32_t net_checksum_partial(uint32_t sum, const uint8_t *data,
                                     uint32_t len)
{
    while (len > 1) {
        sum += net_get_u16(data);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)data[0] << 8;
    }
    return sum;
}

/**
 * Net checksum finish.
 * @param sum Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint16_t net_checksum_finish(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/**
 * Net tcp checksum.
 * @param src_ip Value supplied by the caller.
 * @param dst_ip Value supplied by the caller.
 * @param tcp Value supplied by the caller.
 * @param tcp_len Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint16_t net_tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                                 const uint8_t *tcp, uint16_t tcp_len)
{
    uint8_t pseudo[12];
    uint32_t sum = 0;
    net_put_u32(pseudo, src_ip);
    net_put_u32(pseudo + 4, dst_ip);
    pseudo[8] = 0;
    pseudo[9] = IPV4_PROTO_TCP;
    net_put_u16(pseudo + 10, tcp_len);
    sum = net_checksum_partial(sum, pseudo, sizeof(pseudo));
    sum = net_checksum_partial(sum, tcp, tcp_len);
    return net_checksum_finish(sum);
}

/**
 * Net append char.
 * @param dst Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param ch Value supplied by the caller.
 */
static void net_append_char(char *dst, uint32_t *pos, uint32_t cap, char ch)
{
    if (dst && pos && *pos + 1u < cap) {
        dst[*pos] = ch;
        ++(*pos);
        dst[*pos] = 0;
    }
}

/**
 * Net append text.
 * @param dst Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param text NUL-terminated text supplied by the caller.
 */
static void net_append_text(char *dst, uint32_t *pos, uint32_t cap,
                            const char *text)
{
    while (text && *text) {
        net_append_char(dst, pos, cap, *text++);
    }
}

/**
 * Net append u32.
 * @param dst Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param value Value supplied by the caller.
 */
static void net_append_u32(char *dst, uint32_t *pos, uint32_t cap,
                           uint32_t value)
{
    char tmp[12];
    uint32_t n = 0;
    if (value == 0) {
        net_append_char(dst, pos, cap, '0');
        return;
    }
    while (value && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (n) {
        net_append_char(dst, pos, cap, tmp[--n]);
    }
}

/**
 * Net timeout expired.
 * @param start_ms Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @param spins Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_timeout_expired(uint64_t start_ms, uint32_t timeout_ms,
                               uint32_t spins)
{
    uint64_t now = time_uptime_ms();
    uint32_t spin_limit = timeout_ms * 2000u + 100000u;
    if (now != start_ms) {
        return now - start_ms >= timeout_ms;
    }
    return spins >= spin_limit;
}

/**
 * Net arp cache clear.
 */
static void net_arp_cache_clear(void)
{
    net_memzero(net_arp_cache, sizeof(net_arp_cache));
    net_arp_cache_next = 0;
}

/**
 * Net arp cache lookup.
 * @param ip Value supplied by the caller.
 * @param mac Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_arp_cache_lookup(uint32_t ip, uint8_t mac[6])
{
    if (!ip || !mac) {
        return 0;
    }
    for (uint32_t i = 0; i < NET_ARP_CACHE_SIZE; ++i) {
        if (net_arp_cache[i].ip == ip &&
            !net_mac_is_zero(net_arp_cache[i].mac)) {
            net_memcpy(mac, net_arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

/**
 * Net arp cache store.
 * @param ip Value supplied by the caller.
 * @param mac Value supplied by the caller.
 */
static void net_arp_cache_store(uint32_t ip, const uint8_t mac[6])
{
    uint32_t slot;
    if (!ip || !mac || net_mac_is_zero(mac) ||
        ip == 0xffffffffu || net_memeq(mac, net_broadcast_mac, 6)) {
        return;
    }
    for (uint32_t i = 0; i < NET_ARP_CACHE_SIZE; ++i) {
        if (net_arp_cache[i].ip == ip || net_arp_cache[i].ip == 0) {
            net_arp_cache[i].ip = ip;
            net_memcpy(net_arp_cache[i].mac, mac, 6);
            return;
        }
    }
    slot = net_arp_cache_next++ % NET_ARP_CACHE_SIZE;
    net_arp_cache[slot].ip = ip;
    net_memcpy(net_arp_cache[slot].mac, mac, 6);
}

/**
 * Net update config flags.
 */
static void net_update_config_flags(void)
{
    uint32_t flags = 0;
    struct e1000_info info;
    e1000_get_info(&info);
    if (info.present) flags |= LEONOS_NET_CONFIG_FLAG_PRESENT;
    if (info.active && net_interface_up) flags |= LEONOS_NET_CONFIG_FLAG_ACTIVE;
    if (net_config.source == LEONOS_NET_CONFIG_SOURCE_DHCP) {
        flags |= LEONOS_NET_CONFIG_FLAG_DHCP;
    }
    net_config.flags = flags;
    net_memzero(net_config.mac, sizeof(net_config.mac));
    if (info.present) net_memcpy(net_config.mac, info.mac, 6);
}

/**
 * Net set static fallback.
 */
static void net_set_static_fallback(void)
{
    net_arp_cache_clear();
    net_dhcp_dns_ip = 0;
    net_dhcp_acquired_ms = 0;
    net_config = (struct leonos_net_config){
        .flags = 0,
        .source = LEONOS_NET_CONFIG_SOURCE_NONE,
        .local_ip = 0,
        .subnet_mask = 0,
        .gateway_ip = 0,
        .dns_ip = net_effective_dns_ip(0),
        .dhcp_server_ip = 0,
        .lease_seconds = 0,
    };
    net_update_config_flags();
}

static void net_expire_lease(void)
{
    if (net_config.source == LEONOS_NET_CONFIG_SOURCE_DHCP && net_config.lease_seconds != UINT32_MAX &&
        time_uptime_ms() - net_dhcp_acquired_ms >= (uint64_t)net_config.lease_seconds * 1000)
        net_set_static_fallback();
}

/**
 * Net apply dhcp offer.
 * @param offer Value supplied by the caller.
 */
static void net_apply_dhcp_offer(const struct net_dhcp_offer *offer)
{
    net_arp_cache_clear();
    net_config.local_ip = offer->yiaddr;
    net_config.subnet_mask = offer->subnet_mask ? offer->subnet_mask : LEONOS_NET_DEFAULT_SUBNET_MASK;
    net_config.gateway_ip = offer->router_ip;
    net_dhcp_dns_ip = offer->dns_ip;
    net_config.dns_ip = net_effective_dns_ip(net_dhcp_dns_ip);
    net_config.dhcp_server_ip = offer->server_ip;
    net_config.lease_seconds = offer->lease_seconds;
    net_dhcp_acquired_ms = time_uptime_ms();
    net_config.source = LEONOS_NET_CONFIG_SOURCE_DHCP;
    net_update_config_flags();
}

/**
 * Net write eth.
 * @param frame Value supplied by the caller.
 * @param dst_mac Value supplied by the caller.
 * @param type Value supplied by the caller.
 */
static void net_write_eth(uint8_t *frame, const uint8_t *dst_mac,
                          uint16_t type)
{
    const uint8_t *src_mac = e1000_mac();
    net_memcpy(frame, dst_mac, 6);
    net_memcpy(frame + 6, src_mac, 6);
    net_put_u16(frame + 12, type);
}

/**
 * Net write arp ipv4.
 * @param frame Value supplied by the caller.
 * @param op Identifier or flags controlling the operation.
 * @param dst_eth_mac Value supplied by the caller.
 * @param target_mac Value supplied by the caller.
 * @param target_ip Value supplied by the caller.
 */
static void net_write_arp_ipv4(uint8_t *frame, uint16_t op,
                               const uint8_t *dst_eth_mac,
                               const uint8_t *target_mac,
                               uint32_t target_ip)
{
    const uint8_t *src_mac = e1000_mac();
    net_write_eth(frame, dst_eth_mac, ETH_TYPE_ARP);
    net_put_u16(frame + 14, ARP_HTYPE_ETHERNET);
    net_put_u16(frame + 16, ETH_TYPE_IPV4);
    frame[18] = 6;
    frame[19] = 4;
    net_put_u16(frame + 20, op);
    net_memcpy(frame + 22, src_mac, 6);
    net_put_u32(frame + 28, net_config.local_ip);
    net_memcpy(frame + 32, target_mac, 6);
    net_put_u32(frame + 38, target_ip);
}

/**
 * Net send arp request.
 * @param target_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_send_arp_request(uint32_t target_ip)
{
    uint8_t frame[64];
    uint8_t zero_mac[6] = {0, 0, 0, 0, 0, 0};
    net_memzero(frame, sizeof(frame));
    net_write_arp_ipv4(frame, ARP_OPER_REQUEST, net_broadcast_mac,
                       zero_mac, target_ip);
    return net_interface_up ? e1000_send(frame, 42) : -LINUX_ENETDOWN;
}

/**
 * Net send arp reply.
 * @param target_mac Value supplied by the caller.
 * @param target_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_send_arp_reply(const uint8_t *target_mac, uint32_t target_ip)
{
    uint8_t frame[64];
    net_memzero(frame, sizeof(frame));
    net_write_arp_ipv4(frame, ARP_OPER_REPLY, target_mac, target_mac, target_ip);
    return net_interface_up ? e1000_send(frame, 42) : -LINUX_ENETDOWN;
}

/**
 * Net route arp ip.
 * @param target_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_route_arp_ip(uint32_t target_ip)
{
    if ((target_ip & net_config.subnet_mask) ==
        (net_config.local_ip & net_config.subnet_mask)) {
        return target_ip;
    }
    return net_config.gateway_ip ? net_config.gateway_ip : target_ip;
}

/**
 * Net frame for us.
 * @param frame Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_frame_for_us(const uint8_t *frame)
{
    return net_memeq(frame, e1000_mac(), 6) ||
           net_memeq(frame, net_broadcast_mac, 6);
}

/**
 * Net ip for us.
 * @param dst_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_ip_for_us(uint32_t dst_ip)
{
    return dst_ip == net_config.local_ip ||
           dst_ip == 0xffffffffu ||
           dst_ip == 0;
}

/**
 * Net send udp to mac.
 * @param dst_mac Value supplied by the caller.
 * @param src_ip Value supplied by the caller.
 * @param dst_ip Value supplied by the caller.
 * @param src_port Value supplied by the caller.
 * @param dst_port Value supplied by the caller.
 * @param payload Value supplied by the caller.
 * @param payload_len Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_send_udp_to_mac(const uint8_t *dst_mac, uint32_t src_ip,
                               uint32_t dst_ip, uint16_t src_port,
                               uint16_t dst_port, const uint8_t *payload,
                               uint32_t payload_len)
{
    uint8_t frame[NET_FRAME_MAX];
    uint8_t *ip = frame + 14;
    uint8_t *udp = frame + 34;
    uint16_t udp_len;
    uint16_t total_len;
    if ((payload_len && !payload) || payload_len > NET_FRAME_MAX - 42u) {
        return -1;
    }
    udp_len = (uint16_t)(8u + payload_len);
    total_len = (uint16_t)(20u + udp_len);
    net_memzero(frame, 42u + payload_len);
    net_write_eth(frame, dst_mac, ETH_TYPE_IPV4);
    ip[0] = 0x45;
    ip[1] = 0;
    net_put_u16(ip + 2, total_len);
    net_put_u16(ip + 4, net_ipv4_id++);
    net_put_u16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = IPV4_PROTO_UDP;
    net_put_u32(ip + 12, src_ip);
    net_put_u32(ip + 16, dst_ip);
    net_put_u16(ip + 10, net_checksum(ip, 20));
    net_put_u16(udp, src_port);
    net_put_u16(udp + 2, dst_port);
    net_put_u16(udp + 4, udp_len);
    net_put_u16(udp + 6, 0);
    net_memcpy(udp + 8, payload, payload_len);
    return net_interface_up ? e1000_send(frame, 42u + payload_len) : -LINUX_ENETDOWN;
}

/* Remaining space already advertised to the peer, across sequence wrap. */
static uint32_t net_socket_receive_window(const struct net_socket *socket)
{
    if (!socket->rx_window_valid) return 0;
    uint32_t consumed = socket->remote_seq - socket->rx_window_ack;
    return consumed < socket->rx_window ? socket->rx_window - consumed : 0;
}

static uint32_t net_socket_select_window(const struct net_socket *socket)
{
    uint32_t free_bytes = NET_SOCKET_RX_CAP - socket->rx_len;
    uint32_t window = free_bytes - free_bytes % NET_TCP_MSS;
    uint32_t remaining = net_socket_receive_window(socket);
    /* As in Linux tcp_select_window(), do not retract an offered right edge.
     * Only extend it in full segments to avoid receiver silly-window syndrome. */
    return window > remaining ? window : remaining;
}

static int net_send_tcp_to_mac(const uint8_t *dst_mac, uint32_t src_ip,
                               uint32_t dst_ip, uint16_t src_port,
                               uint16_t dst_port, uint32_t seq,
                               uint32_t ack, uint8_t flags,
                               const uint8_t *payload,
                               uint32_t payload_len)
{
    uint8_t frame[NET_FRAME_MAX];
    uint8_t *ip = frame + 14;
    uint8_t *tcp = frame + 34;
    uint32_t tcp_header_len = (flags & TCP_FLAG_SYN) ? 24u : 20u;
    uint16_t tcp_len;
    uint16_t total_len;
    if (payload_len > NET_FRAME_MAX - 34u - tcp_header_len ||
        (payload_len && !payload)) {
        return -1;
    }
    tcp_len = (uint16_t)(tcp_header_len + payload_len);
    total_len = (uint16_t)(20u + tcp_len);
    net_memzero(frame, 34u + tcp_header_len + payload_len);
    net_write_eth(frame, dst_mac, ETH_TYPE_IPV4);
    ip[0] = 0x45;
    ip[1] = 0;
    net_put_u16(ip + 2, total_len);
    net_put_u16(ip + 4, net_ipv4_id++);
    net_put_u16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = IPV4_PROTO_TCP;
    net_put_u32(ip + 12, src_ip);
    net_put_u32(ip + 16, dst_ip);
    net_put_u16(ip + 10, net_checksum(ip, 20));

    net_put_u16(tcp, src_port);
    net_put_u16(tcp + 2, dst_port);
    net_put_u32(tcp + 4, seq);
    net_put_u32(tcp + 8, ack);
    tcp[12] = (uint8_t)((tcp_header_len / 4u) << 4);
    tcp[13] = flags;
    uint32_t window = 8192u;
    struct net_socket *receiver = 0;
    for (unsigned i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *s = &net_sockets[i];
        if (s->used && s->local_port == src_port && s->remote_port == dst_port &&
            s->remote_ip == dst_ip) {
            receiver = s;
            window = net_socket_select_window(s);
            break;
        }
    }
    net_put_u16(tcp + 14, (uint16_t)window);
    net_put_u16(tcp + 16, 0);
    net_put_u16(tcp + 18, 0);
    if (flags & TCP_FLAG_SYN) {
        tcp[20] = 2;
        tcp[21] = 4;
        net_put_u16(tcp + 22, NET_TCP_MSS);
    }
    if (payload_len) {
        net_memcpy(tcp + tcp_header_len, payload, payload_len);
    }
    net_put_u16(tcp + 16, net_tcp_checksum(src_ip, dst_ip, tcp, tcp_len));
    int result = net_interface_up ? e1000_send(frame, 14u + total_len) : -LINUX_ENETDOWN;
    if (result >= 0 && receiver && (flags & TCP_FLAG_ACK)) {
        receiver->rx_window_ack = ack;
        receiver->rx_window = window;
        receiver->rx_window_valid = true;
    }
    return result;
}

/**
 * Net handle arp.
 * @param frame Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param arp_wait Value supplied by the caller.
 */
static void net_handle_arp(const uint8_t *frame, uint32_t len,
                           struct net_arp_wait *arp_wait)
{
    uint16_t op;
    uint32_t sender_ip;
    uint32_t target_ip;
    const uint8_t *sender_mac;
    if (len < 42 ||
        net_get_u16(frame + 14) != ARP_HTYPE_ETHERNET ||
        net_get_u16(frame + 16) != ETH_TYPE_IPV4 ||
        frame[18] != 6 || frame[19] != 4) {
        return;
    }
    op = net_get_u16(frame + 20);
    sender_mac = frame + 22;
    sender_ip = net_get_u32(frame + 28);
    target_ip = net_get_u32(frame + 38);
    net_arp_cache_store(sender_ip, sender_mac);
    if (op == ARP_OPER_REQUEST && target_ip == net_config.local_ip) {
        (void)net_send_arp_reply(sender_mac, sender_ip);
        return;
    }
    if (op == ARP_OPER_REPLY && arp_wait && sender_ip == arp_wait->ip) {
        net_memcpy(arp_wait->mac, sender_mac, 6);
        arp_wait->done = 1;
    }
}

/**
 * Net send icmp echo request.
 * @param dst_mac Value supplied by the caller.
 * @param target_ip Value supplied by the caller.
 * @param ident Value supplied by the caller.
 * @param sequence Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_send_icmp_echo_request(const uint8_t *dst_mac, uint32_t target_ip,
                                      uint16_t ident, uint16_t sequence)
{
    uint8_t frame[98];
    uint8_t *ip = frame + 14;
    uint8_t *icmp = frame + 34;
    uint16_t icmp_len = 8u + NET_ICMP_PAYLOAD_LEN;
    uint16_t total_len = 20u + icmp_len;

    net_memzero(frame, sizeof(frame));
    net_write_eth(frame, dst_mac, ETH_TYPE_IPV4);
    ip[0] = 0x45;
    ip[1] = 0;
    net_put_u16(ip + 2, total_len);
    net_put_u16(ip + 4, net_ipv4_id++);
    net_put_u16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = IPV4_PROTO_ICMP;
    net_put_u32(ip + 12, net_config.local_ip);
    net_put_u32(ip + 16, target_ip);
    net_put_u16(ip + 10, net_checksum(ip, 20));

    icmp[0] = ICMP_ECHO_REQUEST;
    icmp[1] = 0;
    net_put_u16(icmp + 4, ident);
    net_put_u16(icmp + 6, sequence);
    for (uint32_t i = 0; i < NET_ICMP_PAYLOAD_LEN; ++i) {
        icmp[8 + i] = (uint8_t)('A' + (i % 26));
    }
    net_put_u16(icmp + 2, net_checksum(icmp, icmp_len));
    return net_interface_up ? e1000_send(frame, 14u + total_len) : -LINUX_ENETDOWN;
}

/**
 * Net handle udp.
 * @param ip Value supplied by the caller.
 * @param total_len Value supplied by the caller.
 * @param ihl Value supplied by the caller.
 * @param src_ip Value supplied by the caller.
 * @param udp_wait Value supplied by the caller.
 */
static void net_handle_udp(const uint8_t *ip, uint32_t total_len,
                           uint32_t ihl, uint32_t src_ip,
                           struct net_udp_wait *udp_wait)
{
    const uint8_t *udp;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;
    uint32_t payload_len;
    if (total_len < ihl + 8u) {
        return;
    }
    udp = ip + ihl;
    src_port = net_get_u16(udp);
    dst_port = net_get_u16(udp + 2);
    udp_len = net_get_u16(udp + 4);
    if (udp_len < 8u || ihl + udp_len > total_len) {
        return;
    }
    if (net_get_u16(udp + 6)) {
        uint32_t sum = net_checksum_partial(0, ip + 12, 8);
        sum += IPV4_PROTO_UDP + udp_len;
        if (net_checksum_finish(net_checksum_partial(sum, udp, udp_len))) return;
    }
    net_udp_input(src_ip, net_get_u32(ip + 16), src_port, dst_port, udp + 8, udp_len - 8, 2);
    if (!udp_wait || udp_wait->done) return;
    if (udp_wait->src_port && src_port != udp_wait->src_port) {
        return;
    }
    if (udp_wait->dst_port && dst_port != udp_wait->dst_port) {
        return;
    }
    if (udp_wait->src_ip && src_ip != udp_wait->src_ip) {
        return;
    }
    payload_len = udp_len - 8u;
    if (payload_len > udp_wait->capacity) {
        payload_len = udp_wait->capacity;
    }
    net_memcpy(udp_wait->payload, udp + 8, payload_len);
    udp_wait->length = payload_len;
    udp_wait->done = 1;
}

/**
 * Net handle tcp.
 * @param ip Value supplied by the caller.
 * @param total_len Value supplied by the caller.
 * @param ihl Value supplied by the caller.
 * @param src_ip Value supplied by the caller.
 * @param tcp_wait Value supplied by the caller.
 */
static void net_handle_tcp(const uint8_t *ip, uint32_t total_len,
                           uint32_t ihl, uint32_t src_ip,
                           struct net_tcp_wait *tcp_wait)
{
    const uint8_t *tcp;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint32_t tcp_len;
    uint32_t hdr_len;
    uint32_t payload_len;
    uint8_t flags;
    if (total_len < ihl + 20u) {
        return;
    }
    tcp = ip + ihl;
    tcp_len = total_len - ihl;
    if (net_tcp_checksum(src_ip, net_get_u32(ip + 16), tcp, tcp_len)) return;
    src_port = net_get_u16(tcp);
    dst_port = net_get_u16(tcp + 2);
    hdr_len = (uint32_t)(tcp[12] >> 4) * 4u;
    if (hdr_len < 20u || hdr_len > tcp_len) {
        return;
    }
    flags = tcp[13];
    seq = net_get_u32(tcp + 4);
    ack = net_get_u32(tcp + 8);
    payload_len = tcp_len - hdr_len;
    net_socket_handle_tcp(src_ip, src_port, dst_port, seq, ack, flags,
                          tcp + hdr_len, payload_len);
    if (!tcp_wait) {
        return;
    }
    if (tcp_wait->src_port && src_port != tcp_wait->src_port) {
        return;
    }
    if (tcp_wait->dst_port && dst_port != tcp_wait->dst_port) {
        return;
    }
    if (tcp_wait->src_ip && src_ip != tcp_wait->src_ip) {
        return;
    }
    tcp_wait->flags |= flags;
    if (flags & TCP_FLAG_ACK) {
        if (!tcp_wait->acked_seq ||
            net_tcp_seq_after_or_equal(ack, tcp_wait->acked_seq)) {
            tcp_wait->acked_seq = ack;
        }
    }
    if (flags & TCP_FLAG_RST) {
        tcp_wait->reset = 1;
        ++tcp_wait->changed;
        return;
    }
    if (flags & TCP_FLAG_SYN) {
        tcp_wait->remote_seq = seq + 1u;
        ++tcp_wait->changed;
    }
    if (payload_len) {
        if (tcp_wait->remote_seq == 0 || seq == tcp_wait->remote_seq) {
            uint32_t copy_len = payload_len;
            if (copy_len > tcp_wait->capacity - tcp_wait->length) {
                copy_len = tcp_wait->capacity - tcp_wait->length;
                tcp_wait->overflow = 1;
            }
            if (copy_len && tcp_wait->payload) {
                net_memcpy(tcp_wait->payload + tcp_wait->length,
                           tcp + hdr_len, copy_len);
                tcp_wait->length += copy_len;
            }
            tcp_wait->remote_seq = seq + payload_len;
            ++tcp_wait->changed;
        } else if (seq + payload_len > tcp_wait->remote_seq) {
            tcp_wait->overflow = 1;
        }
    }
    if (flags & TCP_FLAG_FIN) {
        if (tcp_wait->remote_seq == 0 ||
            seq + payload_len == tcp_wait->remote_seq) {
            ++tcp_wait->remote_seq;
            tcp_wait->fin = 1;
            ++tcp_wait->changed;
        }
    }
}

/**
 * Net handle ipv4.
 * @param frame Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param ping_wait Value supplied by the caller.
 * @param udp_wait Value supplied by the caller.
 * @param tcp_wait Value supplied by the caller.
 */
static void net_handle_ipv4(const uint8_t *frame, uint32_t len,
                            struct net_ping_wait *ping_wait,
                            struct net_udp_wait *udp_wait,
                            struct net_tcp_wait *tcp_wait)
{
    const uint8_t *ip;
    const uint8_t *icmp;
    uint32_t ihl;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t total_len;
    uint16_t icmp_len;
    if (len < 34) {
        return;
    }
    ip = frame + 14;
    if ((ip[0] >> 4) != 4) {
        return;
    }
    ihl = (uint32_t)(ip[0] & 0x0fu) * 4u;
    if (ihl < 20 || len < 14u + ihl) {
        return;
    }
    total_len = net_get_u16(ip + 2);
    if (total_len < ihl || 14u + total_len > len) {
        return;
    }
    /* Fragment reassembly is not implemented; fragments must never be
     * interpreted as independent transport packets. */
    if (net_checksum(ip, ihl) || (net_get_u16(ip + 6) & 0x3fffu)) return;
    dst_ip = net_get_u32(ip + 16);
    if (!net_ip_for_us(dst_ip)) {
        return;
    }
    src_ip = net_get_u32(ip + 12);
    if (ip[9] == IPV4_PROTO_UDP) {
        net_handle_udp(ip, total_len, ihl, src_ip, udp_wait);
        return;
    }
    if (ip[9] == IPV4_PROTO_TCP) {
        net_handle_tcp(ip, total_len, ihl, src_ip, tcp_wait);
        return;
    }
    if (ip[9] != IPV4_PROTO_ICMP || total_len < ihl + 8u) {
        return;
    }
    icmp = ip + ihl;
    icmp_len = (uint16_t)(total_len - ihl);
    if (icmp_len < 8) {
        return;
    }
    if (icmp[0] == ICMP_ECHO_REPLY && ping_wait &&
        src_ip == ping_wait->target_ip &&
        net_get_u16(icmp + 4) == ping_wait->ident &&
        net_get_u16(icmp + 6) == ping_wait->sequence) {
        ping_wait->done = 1;
    }
}

/**
 * Net process frame.
 * @param frame Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param arp_wait Value supplied by the caller.
 * @param ping_wait Value supplied by the caller.
 * @param udp_wait Value supplied by the caller.
 * @param tcp_wait Value supplied by the caller.
 */
static void net_process_frame(const uint8_t *frame, uint32_t len,
                              struct net_arp_wait *arp_wait,
                              struct net_ping_wait *ping_wait,
                              struct net_udp_wait *udp_wait,
                              struct net_tcp_wait *tcp_wait)
{
    uint16_t type;
    if (len < 14 || !net_frame_for_us(frame)) {
        return;
    }
    type = net_get_u16(frame + 12);
    if (type == ETH_TYPE_ARP) {
        net_handle_arp(frame, len, arp_wait);
    } else if (type == ETH_TYPE_IPV4) {
        net_handle_ipv4(frame, len, ping_wait, udp_wait, tcp_wait);
    }
}

/**
 * Net poll once.
 * @param arp_wait Value supplied by the caller.
 * @param ping_wait Value supplied by the caller.
 * @param udp_wait Value supplied by the caller.
 * @param tcp_wait Value supplied by the caller.
 */
static void net_poll_once(struct net_arp_wait *arp_wait,
                          struct net_ping_wait *ping_wait,
                          struct net_udp_wait *udp_wait,
                          struct net_tcp_wait *tcp_wait)
{
    uint8_t frame[NET_FRAME_MAX];
    uint32_t len = 0;
    int ret = e1000_poll(frame, sizeof(frame), &len);
    if (ret > 0 && len && net_interface_up) {
        net_packet_input(frame, len);
        net_process_frame(frame, len, arp_wait, ping_wait, udp_wait, tcp_wait);
    }
}

/**
 * Net resolve mac.
 * @param ip Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @param out_mac Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_resolve_mac(uint32_t ip, uint32_t timeout_ms, uint8_t *out_mac)
{
    struct net_arp_wait wait;
    uint64_t start = time_uptime_ms();
    uint64_t next_request = start;
    uint32_t spins = 0;
    wait = (struct net_arp_wait){0};
    wait.ip = ip;
    if (net_arp_cache_lookup(ip, out_mac)) {
        return 0;
    }
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        uint64_t now = time_uptime_ms();
        if (now >= next_request) {
            if (net_send_arp_request(ip) < 0) {
                return -1;
            }
            next_request = now + 250u;
        }
        net_poll_once(&wait, 0, 0, 0);
        if (wait.done) {
            net_memcpy(out_mac, wait.mac, 6);
            net_arp_cache_store(ip, wait.mac);
            return 0;
        }
        net_cpu_relax();
    }
    return -2;
}

/**
 * Net now32.
 * @return The value or status produced by the operation.
 */
static uint32_t net_now32(void)
{
    return (uint32_t)time_uptime_ms();
}

/**
 * Net socket clear.
 * @param socket Value supplied by the caller.
 */
static void net_socket_clear(struct net_socket *socket)
{
    if (socket) {
        kernel_free(socket->rx);
        net_memzero(socket, sizeof(*socket));
    }
}

/**
 * Net socket touch.
 * @param socket Value supplied by the caller.
 */
static void net_socket_touch(struct net_socket *socket)
{
    if (socket) {
        socket->changed_ms = net_now32();
    }
}

/**
 * Net socket gc.
 */
static void net_socket_gc(void)
{
    uint32_t now = net_now32();
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *socket = &net_sockets[i];
        if (!socket->used || socket->fd_owned) {
            continue;
        }
        if ((socket->state == LEONOS_NET_TCP_CLOSED ||
             socket->state == LEONOS_NET_TCP_TIME_WAIT) &&
            now - socket->changed_ms >= NET_SOCKET_CLOSE_HOLD_MS) {
            net_socket_clear(socket);
        }
    }
}

/**
 * Net socket find.
 * @param handle Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @param allow_closed Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static struct net_socket *net_socket_find(int32_t handle, uint32_t owner_pid,
                                          int allow_closed)
{
    if (handle <= 0) {
        return 0;
    }
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *socket = &net_sockets[i];
        if (!socket->used || socket->handle != handle) {
            continue;
        }
        if (owner_pid && socket->owner_pid != owner_pid) {
            continue;
        }
        if (!allow_closed && socket->state == LEONOS_NET_TCP_CLOSED) {
            return 0;
        }
        return socket;
    }
    return 0;
}

/**
 * Net socket match.
 * @param src_ip Value supplied by the caller.
 * @param src_port Value supplied by the caller.
 * @param dst_port Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static struct net_socket *net_socket_match(uint32_t src_ip, uint16_t src_port,
                                           uint16_t dst_port)
{
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *socket = &net_sockets[i];
        if (!socket->used || socket->state == LEONOS_NET_TCP_CLOSED) {
            continue;
        }
        if (socket->remote_ip == src_ip &&
            socket->remote_port == src_port &&
            socket->local_port == dst_port) {
            return socket;
        }
    }
    return 0;
}

/**
 * Net socket mark closed.
 * @param socket Value supplied by the caller.
 * @param status Output storage updated by the function.
 */
static void net_socket_mark_closed(struct net_socket *socket, uint32_t status)
{
    if (!socket) {
        return;
    }
    socket->state = LEONOS_NET_TCP_CLOSED;
    socket->status = status;
    net_socket_touch(socket);
}

/**
 * Net socket trace tls.
 * @param socket Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_socket_trace_tls(const struct net_socket *socket)
{
    return NET_TCP_TRACE && socket && socket->remote_port == 443u;
}

/**
 * Net socket handle tcp.
 * @param src_ip Value supplied by the caller.
 * @param src_port Value supplied by the caller.
 * @param dst_port Value supplied by the caller.
 * @param seq Value supplied by the caller.
 * @param ack Value supplied by the caller.
 * @param flags Identifier or flags controlling the operation.
 * @param payload Value supplied by the caller.
 * @param payload_len Value supplied by the caller.
 */
static void net_socket_handle_tcp(uint32_t src_ip, uint16_t src_port,
                                  uint16_t dst_port, uint32_t seq,
                                  uint32_t ack, uint8_t flags,
                                  const uint8_t *payload,
                                  uint32_t payload_len)
{
    struct net_socket *socket = net_socket_match(src_ip, src_port, dst_port);
    if (!socket) {
        return;
    }
    if (net_socket_trace_tls(socket)) {
        console_printf("[net] tls rx socket=%d state=%u flags=0x%x seq=%u ack=%u payload=%u expected=%u\n",
                       socket->handle, socket->state, flags, seq, ack,
                       payload_len, socket->remote_seq);
    }
    if (flags & TCP_FLAG_ACK) {
        if (!socket->acked_seq ||
            net_tcp_seq_after_or_equal(ack, socket->acked_seq)) {
            socket->acked_seq = ack;
        }
    }
    if (flags & TCP_FLAG_RST) {
        socket->error = socket->state == LEONOS_NET_TCP_SYN_SENT ? LINUX_ECONNREFUSED : LINUX_ECONNRESET;
        net_socket_mark_closed(socket, LEONOS_NET_STATUS_TCP_RESET);
        return;
    }
    if (socket->state == LEONOS_NET_TCP_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                (TCP_FLAG_SYN | TCP_FLAG_ACK) &&
            socket->acked_seq == socket->local_seq) {
            socket->remote_seq = seq + 1u;
            socket->state = LEONOS_NET_TCP_ESTABLISHED;
            socket->status = LEONOS_NET_STATUS_OK;
            socket->syn_deadline = 0;
            net_socket_touch(socket);
            if (net_socket_trace_tls(socket)) {
                console_printf("[net] tls connected socket=%d local=%u remote=%u\n",
                               socket->handle, socket->local_port,
                               socket->remote_port);
            }
            (void)net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                      socket->remote_ip, socket->local_port,
                                      socket->remote_port, socket->local_seq,
                                      socket->remote_seq, TCP_FLAG_ACK, 0, 0);
        }
        return;
    }
    if (socket->state != LEONOS_NET_TCP_ESTABLISHED &&
        socket->state != LEONOS_NET_TCP_TIME_WAIT) {
        return;
    }
    if (payload_len) {
        if (net_tcp_seq_after_or_equal(socket->remote_seq, seq) && seq != socket->remote_seq) {
            uint32_t duplicate = socket->remote_seq - seq;
            if (duplicate > payload_len) duplicate = payload_len;
            payload += duplicate;
            payload_len -= duplicate;
            seq += duplicate;
        }
        if (seq == socket->remote_seq) {
            uint32_t free_bytes = socket->shutdown_read ? payload_len : NET_SOCKET_RX_CAP - socket->rx_len;
            uint32_t copy_len = payload_len;
            uint32_t overflow = 0;
            if (copy_len > free_bytes) {
                copy_len = free_bytes;
                overflow = 1;
            }
            if (copy_len && !socket->shutdown_read) {
                uint32_t tail = (socket->rx_head + socket->rx_len) % NET_SOCKET_RX_CAP;
                uint32_t first = NET_SOCKET_RX_CAP - tail;
                if (first > copy_len) first = copy_len;
                net_memcpy(socket->rx + tail, payload, first);
                net_memcpy(socket->rx, payload + first, copy_len - first);
                socket->rx_len += copy_len;
                socket->rx_bytes += copy_len;
            }
            /* Acknowledge only bytes retained. The sender retransmits the
             * suffix once the application opens the receive window again. */
            socket->remote_seq += copy_len;
            socket->status = LEONOS_NET_STATUS_OK;
            net_socket_touch(socket);
            if (net_socket_trace_tls(socket)) {
                console_printf("[net] tls rx accepted socket=%d bytes=%u queued=%u next=%u overflow=%u\n",
                               socket->handle, copy_len, socket->rx_len,
                               socket->remote_seq, overflow);
            }
        } else if (net_socket_trace_tls(socket)) {
            console_printf("[net] tls rx ignored socket=%d seq=%u expected=%u bytes=%u\n",
                           socket->handle, seq, socket->remote_seq, payload_len);
        }
        (void)net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                  socket->remote_ip, socket->local_port,
                                  socket->remote_port, socket->local_seq,
                                  socket->remote_seq, TCP_FLAG_ACK, 0, 0);
    }
    if (flags & TCP_FLAG_FIN) {
        if (seq + payload_len == socket->remote_seq) {
            ++socket->remote_seq;
            socket->fin_received = 1;
            socket->state = LEONOS_NET_TCP_TIME_WAIT;
            socket->status = LEONOS_NET_STATUS_OK;
            net_socket_touch(socket);
            /**
 * Net dhcp add option.
 * @param dst_mac Value supplied by the caller.
 * @param local_ip Value supplied by the caller.
 * @param remote_ip Value supplied by the caller.
 * @param local_port Value supplied by the caller.
 * @param remote_port Value supplied by the caller.
 * @param local_seq Value supplied by the caller.
 * @param remote_seq Value supplied by the caller.
 * @param TCP_FLAG_ACK Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
            (void)net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                      socket->remote_ip, socket->local_port,
                                      socket->remote_port, socket->local_seq,
                                      socket->remote_seq, TCP_FLAG_ACK, 0, 0);
        }
    }
}

/**
 * Net dhcp add option.
 * @param payload Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param code Value supplied by the caller.
 * @param data Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static uint32_t net_dhcp_add_option(uint8_t *payload, uint32_t pos,
                                    uint32_t cap, uint8_t code,
                                    const uint8_t *data, uint8_t len)
{
    if (pos + 2u + len > cap) {
        return pos;
    }
    payload[pos++] = code;
    payload[pos++] = len;
    net_memcpy(payload + pos, data, len);
    return pos + len;
}

/**
 * Net dhcp add u8.
 * @param payload Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param code Value supplied by the caller.
 * @param value Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_dhcp_add_u8(uint8_t *payload, uint32_t pos,
                                uint32_t cap, uint8_t code, uint8_t value)
{
    return net_dhcp_add_option(payload, pos, cap, code, &value, 1);
}

/**
 * Net dhcp add u32.
 * @param payload Value supplied by the caller.
 * @param pos Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param code Value supplied by the caller.
 * @param value Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_dhcp_add_u32(uint8_t *payload, uint32_t pos,
                                 uint32_t cap, uint8_t code, uint32_t value)
{
    uint8_t data[4];
    net_put_u32(data, value);
    return net_dhcp_add_option(payload, pos, cap, code, data, 4);
}

/**
 * Net build dhcp packet.
 * @param payload Value supplied by the caller.
 * @param cap Maximum number of elements available in the related buffer.
 * @param xid Value supplied by the caller.
 * @param msg_type Value supplied by the caller.
 * @param requested_ip Value supplied by the caller.
 * @param server_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_build_dhcp_packet(uint8_t *payload, uint32_t cap,
                                      uint32_t xid, uint8_t msg_type,
                                      uint32_t requested_ip,
                                      uint32_t server_ip)
{
    uint32_t pos = 240;
    uint8_t params[] = {1, 3, 6, 51, 54};
    uint8_t client_id[7];
    if (cap < NET_DHCP_PACKET_LEN) {
        return 0;
    }
    net_memzero(payload, cap);
    payload[0] = 1;
    payload[1] = 1;
    payload[2] = 6;
    payload[3] = 0;
    net_put_u32(payload + 4, xid);
    net_put_u16(payload + 10, 0x8000u);
    net_memcpy(payload + 28, e1000_mac(), 6);
    net_put_u32(payload + 236, NET_DHCP_MAGIC);
    pos = net_dhcp_add_u8(payload, pos, cap, 53, msg_type);
    client_id[0] = 1;
    net_memcpy(client_id + 1, e1000_mac(), 6);
    pos = net_dhcp_add_option(payload, pos, cap, 61, client_id, sizeof(client_id));
    pos = net_dhcp_add_option(payload, pos, cap, 55, params, sizeof(params));
    if (requested_ip) {
        pos = net_dhcp_add_u32(payload, pos, cap, 50, requested_ip);
    }
    if (server_ip) {
        pos = net_dhcp_add_u32(payload, pos, cap, 54, server_ip);
    }
    if (pos < cap) {
        payload[pos++] = 255;
    }
    return pos < NET_DHCP_PACKET_LEN ? NET_DHCP_PACKET_LEN : pos;
}

/**
 * Net dhcp parse packet.
 * @param payload Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param xid Value supplied by the caller.
 * @param offer Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_dhcp_parse_packet(const uint8_t *payload, uint32_t len,
                                 uint32_t xid, struct net_dhcp_offer *offer)
{
    uint32_t pos;
    if (!payload || !offer || len < 240 ||
        payload[0] != 2 ||
        payload[1] != 1 ||
        payload[2] != 6 ||
        net_get_u32(payload + 4) != xid ||
        !net_memeq(payload + 28, e1000_mac(), 6) ||
        net_get_u32(payload + 236) != NET_DHCP_MAGIC) {
        return -1;
    }
    *offer = (struct net_dhcp_offer){
        .yiaddr = net_get_u32(payload + 16),
    };
    pos = 240;
    while (pos < len) {
        uint8_t code = payload[pos++];
        uint8_t opt_len;
        const uint8_t *opt;
        if (code == 0) {
            continue;
        }
        if (code == 255) {
            break;
        }
        if (pos >= len) return -1;
        opt_len = payload[pos++];
        if (pos + opt_len > len) return -1;
        opt = payload + pos;
        if (code == 53 && opt_len >= 1) {
            offer->msg_type = opt[0];
        } else if (code == 1 && opt_len >= 4) {
            offer->subnet_mask = net_get_u32(opt);
        } else if (code == 3 && opt_len >= 4) {
            offer->router_ip = net_get_u32(opt);
        } else if (code == 6 && opt_len >= 4) {
            offer->dns_ip = net_get_u32(opt);
        } else if (code == 51 && opt_len >= 4) {
            offer->lease_seconds = net_get_u32(opt);
        } else if (code == 54 && opt_len >= 4) {
            offer->server_ip = net_get_u32(opt);
        }
        pos += opt_len;
    }
    return offer->msg_type ? 0 : -1;
}

/**
 * Net dhcp send.
 * @param xid Value supplied by the caller.
 * @param msg_type Value supplied by the caller.
 * @param requested_ip Value supplied by the caller.
 * @param server_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_dhcp_send(uint32_t xid, uint8_t msg_type,
                         uint32_t requested_ip, uint32_t server_ip)
{
    uint8_t payload[NET_DHCP_PACKET_MAX];
    uint32_t len = net_build_dhcp_packet(payload, sizeof(payload), xid,
                                         msg_type, requested_ip, server_ip);
    if (!len) {
        return -1;
    }
    return net_send_udp_to_mac(net_broadcast_mac, 0, 0xffffffffu,
                               NET_DHCP_CLIENT_PORT, NET_DHCP_SERVER_PORT,
                               payload, len);
}

/**
 * Net dhcp wait.
 * @param xid Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @param expected_type Value supplied by the caller.
 * @param offer Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_dhcp_wait(uint32_t xid, uint32_t timeout_ms,
                         uint8_t expected_type, struct net_dhcp_offer *offer)
{
    uint8_t payload[NET_DHCP_PACKET_MAX];
    struct net_udp_wait udp_wait;
    uint64_t start = time_uptime_ms();
    uint32_t spins = 0;
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        udp_wait = (struct net_udp_wait){
            .src_port = NET_DHCP_SERVER_PORT,
            .dst_port = NET_DHCP_CLIENT_PORT,
            .payload = payload,
            .capacity = sizeof(payload),
        };
        net_poll_once(0, 0, &udp_wait, 0);
        if (udp_wait.done &&
            net_dhcp_parse_packet(payload, udp_wait.length, xid, offer) == 0) {
            if (offer->msg_type == expected_type) {
                return 0;
            }
            if (offer->msg_type == NET_DHCP_NAK) {
                return -3;
            }
        }
        net_cpu_relax();
    }
    return -2;
}

/**
 * Net dhcp request.
 * @param timeout_ms Value supplied by the caller.
 * @param out_config Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_dhcp_request(uint32_t timeout_ms,
                                 struct leonos_net_config *out_config)
{
    struct net_dhcp_offer offer;
    uint32_t xid;
    int ret;
    if (!e1000_is_ready()) {
        if (out_config) {
            *out_config = net_config;
        }
        return LEONOS_NET_STATUS_NO_DEVICE;
    }
    timeout_ms = timeout_ms ? timeout_ms : 3000u;
    if (timeout_ms > 10000u) {
        timeout_ms = 10000u;
    }
    xid = 0x4c4e0000u | (net_sequence++ & 0xffffu);
    if (net_dhcp_send(xid, NET_DHCP_DISCOVER, 0, 0) < 0) {
        if (out_config) {
            *out_config = net_config;
        }
        return LEONOS_NET_STATUS_TX_FAILED;
    }
    ret = net_dhcp_wait(xid, timeout_ms, NET_DHCP_OFFER, &offer);
    if (ret < 0 || !offer.yiaddr) {
        if (out_config) {
            *out_config = net_config;
        }
        return ret == -2 ? LEONOS_NET_STATUS_DHCP_TIMEOUT
                         : LEONOS_NET_STATUS_DHCP_FAILED;
    }
    if (net_dhcp_send(xid, NET_DHCP_REQUEST, offer.yiaddr, offer.server_ip) < 0) {
        if (out_config) {
            *out_config = net_config;
        }
        return LEONOS_NET_STATUS_TX_FAILED;
    }
    uint32_t offered_ip = offer.yiaddr, offered_server = offer.server_ip;
    ret = net_dhcp_wait(xid, timeout_ms, NET_DHCP_ACK, &offer);
    if (ret < 0 || offer.yiaddr != offered_ip || offer.server_ip != offered_server ||
        !offer.subnet_mask || !offer.lease_seconds) {
        if (out_config) {
            *out_config = net_config;
        }
        return ret == -2 ? LEONOS_NET_STATUS_DHCP_TIMEOUT
                         : LEONOS_NET_STATUS_DHCP_FAILED;
    }
    net_apply_dhcp_offer(&offer);
    if (out_config) {
        *out_config = net_config;
    }
    return LEONOS_NET_STATUS_OK;
}

/**
 * Net ensure ipv4 config.
 * @param timeout_ms Value supplied by the caller.
 * @param require_dns Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_ensure_ipv4_config(uint32_t timeout_ms, int require_dns)
{
    net_expire_lease();
    if (!e1000_is_ready() || !net_interface_up) {
        return LEONOS_NET_STATUS_NO_DEVICE;
    }
    if (net_config.local_ip &&
        (!require_dns || net_config.dns_ip)) {
        return LEONOS_NET_STATUS_OK;
    }
    (void)timeout_ms;
    return LEONOS_NET_STATUS_NO_ADDRESS;
}

/**
 * Net console ipv4.
 * @param ip Value supplied by the caller.
 */
static void net_console_ipv4(uint32_t ip)
{
    console_printf("%u.%u.%u.%u",
                   (ip >> 24) & 0xffu, (ip >> 16) & 0xffu,
                   (ip >> 8) & 0xffu, ip & 0xffu);
}

/**
 * Net log config.
 * @param prefix Value supplied by the caller.
 */
static void net_log_config(const char *prefix)
{
    console_printf("%s ip=", prefix);
    net_console_ipv4(net_config.local_ip);
    console_printf(" gateway=");
    net_console_ipv4(net_config.gateway_ip);
    console_printf(" dns=");
    net_console_ipv4(net_config.dns_ip);
    console_printf("\n");
}

/**
 * Net init.
 */
void net_init(void)
{
    net_load_dns_policy();
    e1000_init();
    net_set_static_fallback();
    if (!e1000_is_ready()) {
        console_printf("[ntclks] net unavailable: no active e1000\n");
        return;
    }
    net_log_config("[ntclks] interface ready; OpenRC DHCP client owns configuration");
}

/**
 * Net is ready.
 * @return The value or status produced by the operation.
 */
int net_is_ready(void)
{
    return e1000_is_ready();
}

void net_poll_packets(void)
{
    for (unsigned i = 0; i < 32; ++i) net_poll_once(0, 0, 0, 0);
}

/**
 * @brief Send raw IPv4 using the configured route and Ethernet transport.
 * @param destination Host-order destination IPv4 address.
 * @param source Host-order bound source, zero selects the interface address.
 * @param protocol IPv4 protocol used when constructing a header.
 * @param header_included True when data already contains an IPv4 header.
 * @param data Borrowed validated message payload under the execution lock.
 * @param length Payload bytes.
 * @return Bytes accepted or negative errno; no fragmentation is implemented.
 */
int net_ipv4_send_raw(uint32_t destination, uint32_t source, uint8_t protocol,
                      bool header_included, const void *data, uint32_t length)
{
    uint32_t header = header_included ? 0 : 20;
    if (length > 1500 - header) return -LINUX_EMSGSIZE;
    if (header_included && (length < 20 || ((const uint8_t *)data)[0] >> 4 != 4)) return -LINUX_EINVAL;
    uint8_t frame[1514] = {0};
    uint8_t *ip = frame + 14;
    if (!source) source = net_config.local_ip;
    if (header_included) {
        net_memcpy(ip, data, length);
        uint32_t ihl = (ip[0] & 15) * 4;
        if (ihl < 20 || ihl > length) return -LINUX_EINVAL;
        net_put_u16(ip + 2, (uint16_t)length);
        if (!net_get_u32(ip + 12)) net_put_u32(ip + 12, source);
        if (!net_get_u16(ip + 4)) net_put_u16(ip + 4, ++net_packet_ip_id);
        net_put_u16(ip + 10, 0);
        net_put_u16(ip + 10, net_checksum(ip, ihl));
    } else {
        ip[0] = 0x45; ip[8] = 64; ip[9] = protocol;
        net_put_u16(ip + 2, (uint16_t)(length + 20));
        net_put_u16(ip + 4, ++net_packet_ip_id);
        net_put_u32(ip + 12, source); net_put_u32(ip + 16, destination);
        net_put_u16(ip + 10, net_checksum(ip, 20));
        net_memcpy(ip + 20, data, length);
    }
    if (destination == UINT32_MAX) net_memcpy(frame, net_broadcast_mac, 6);
    else if (net_resolve_mac(net_route_arp_ip(destination), 1000, frame) < 0) return -LINUX_EHOSTUNREACH;
    net_memcpy(frame + 6, e1000_mac(), 6);
    net_put_u16(frame + 12, 0x0800);
    int ret = net_interface_up ? e1000_send(frame, length + header + 14) : -LINUX_ENETDOWN;
    return ret < 0 ? ret : (int)length;
}

int net_ipv4_send_udp(uint32_t source, uint32_t destination, uint16_t source_port,
                       uint16_t destination_port, const void *data, uint32_t length)
{
    if (!e1000_is_ready()) return -LINUX_ENETDOWN;
    if (net_ensure_ipv4_config(3000, 0) != LEONOS_NET_STATUS_OK) return -LINUX_ENETUNREACH;
    uint8_t mac[6];
    if (destination == 0xffffffffu) net_memcpy(mac, net_broadcast_mac, 6);
    else if (net_resolve_mac(net_route_arp_ip(destination), 1000, mac) < 0) return -LINUX_EHOSTUNREACH;
    if (!source) source = net_config.local_ip;
    return net_send_udp_to_mac(mac, source, destination, source_port, destination_port,
                               data, length) < 0 ? -LINUX_EIO : 0;
}

/**
 * Net get config.
 * @param config Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_get_config(struct leonos_net_config *config)
{
    if (!config) {
        return -1;
    }
    net_expire_lease();
    net_update_config_flags();
    *config = net_config;
    if (config->source == LEONOS_NET_CONFIG_SOURCE_DHCP && config->lease_seconds != UINT32_MAX)
        config->lease_seconds -= (uint32_t)((time_uptime_ms() - net_dhcp_acquired_ms) / 1000);
    return 0;
}

/**
 * Net set dns policy.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_set_dns_policy(struct leonos_net_dns_policy *request)
{
    uint32_t mode;
    uint32_t custom_dns_ip;
    if (!request) {
        return -1;
    }
    mode = request->mode;
    custom_dns_ip = request->custom_dns_ip;
    request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
    net_update_config_flags();
    if (mode == LEONOS_NET_DNS_MODE_QUERY) {
        request->mode = net_dns_mode;
        request->custom_dns_ip = net_dns_custom_ip;
        request->status = LEONOS_NET_STATUS_OK;
        request->config = net_config;
        return 0;
    }
    request->config = net_config;
    if (mode > LEONOS_NET_DNS_MODE_CUSTOM ||
        (mode == LEONOS_NET_DNS_MODE_CUSTOM &&
         (custom_dns_ip == 0 || custom_dns_ip == 0xffffffffu))) {
        return 0;
    }
    net_dns_mode = mode;
    net_dns_custom_ip = mode == LEONOS_NET_DNS_MODE_CUSTOM
                            ? custom_dns_ip
                            : 0;
    net_config.dns_ip = net_effective_dns_ip(net_dhcp_dns_ip);
    net_update_config_flags();
    request->mode = net_dns_mode;
    request->custom_dns_ip = net_dns_custom_ip;
    request->status = LEONOS_NET_STATUS_OK;
    request->config = net_config;
    return 0;
}

/**
 * Net dhcp renew.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_dhcp_renew(struct leonos_net_dhcp *request)
{
    if (!request) return -LINUX_EINVAL;
    request->status = LEONOS_NET_STATUS_PROTOCOL_UNSUPPORTED;
    request->config = net_config;
    return -LINUX_EOPNOTSUPP;
}

/**
 * Net ping.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_ping(struct leonos_net_ping *request)
{
    uint8_t next_hop_mac[6];
    struct net_ping_wait wait;
    uint32_t timeout_ms;
    uint32_t target_ip;
    uint32_t arp_ip;
    uint16_t sequence;
    uint64_t start;
    uint32_t spins = 0;

    if (!request || request->target_ip == 0 || request->target_ip == 0xffffffffu) {
        if (request) {
            request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        }
        return 0;
    }
    request->sent = 0;
    request->received = 0;
    request->rtt_ms = 0;
    if (!e1000_is_ready()) {
        request->status = LEONOS_NET_STATUS_NO_DEVICE;
        return 0;
    }

    timeout_ms = request->timeout_ms ? request->timeout_ms : LEONOS_NET_DEFAULT_TIMEOUT_MS;
    if (timeout_ms > LEONOS_NET_MAX_TIMEOUT_MS) {
        timeout_ms = LEONOS_NET_MAX_TIMEOUT_MS;
    }
    {
        uint32_t lease_status = net_ensure_ipv4_config(timeout_ms, 0);
        if (lease_status != LEONOS_NET_STATUS_OK) {
            request->status = lease_status;
            return 0;
        }
    }
    target_ip = request->target_ip;
    arp_ip = net_route_arp_ip(target_ip);
    {
        int arp_ret = net_resolve_mac(arp_ip, timeout_ms, next_hop_mac);
        if (arp_ret < 0) {
            request->status = arp_ret == -1
                                  ? LEONOS_NET_STATUS_TX_FAILED
                                  : LEONOS_NET_STATUS_ARP_TIMEOUT;
            return 0;
        }
    }

    sequence = (uint16_t)(request->sequence ? request->sequence : net_sequence++);
    wait = (struct net_ping_wait){
        .target_ip = target_ip,
        .ident = 0x4c4e,
        .sequence = sequence,
        .done = 0,
    };
    start = time_uptime_ms();
    if (net_send_icmp_echo_request(next_hop_mac, target_ip, wait.ident, wait.sequence) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return 0;
    }
    request->sent = 1;
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        net_poll_once(0, &wait, 0, 0);
        if (wait.done) {
            uint64_t now = time_uptime_ms();
            request->received = 1;
            request->rtt_ms = (uint32_t)(now >= start ? now - start : 0);
            request->status = LEONOS_NET_STATUS_OK;
            return 0;
        }
        net_cpu_relax();
    }
    request->status = LEONOS_NET_STATUS_ECHO_TIMEOUT;
    return 0;
}

/**
 * Net dns encode name.
 * @param name NUL-terminated text supplied by the caller.
 * @param out Output storage updated by the function.
 * @param cap Maximum number of elements available in the related buffer.
 * @param out_len Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_dns_encode_name(const char *name, uint8_t *out,
                               uint32_t cap, uint32_t *out_len)
{
    uint32_t label_start = 0;
    uint32_t len = net_strlen(name, LEONOS_NET_HOSTNAME_LEN);
    uint32_t pos = 0;
    if (!name || !out || !out_len || len == 0 || len >= LEONOS_NET_HOSTNAME_LEN) {
        return -1;
    }
    for (uint32_t i = 0; i <= len; ++i) {
        if (name[i] == '.' || name[i] == 0) {
            uint32_t label_len = i - label_start;
            if (label_len == 0 || label_len > 63 || pos + label_len + 1 >= cap) {
                return -1;
            }
            out[pos++] = (uint8_t)label_len;
            net_memcpy(out + pos, name + label_start, label_len);
            pos += label_len;
            label_start = i + 1;
        }
    }
    if (pos + 1 > cap) {
        return -1;
    }
    out[pos++] = 0;
    *out_len = pos;
    return 0;
}

/**
 * Net dns skip name.
 * @param packet Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param pos Output storage updated by the function.
 * @return The value or status produced by the operation.
 */
static int net_dns_skip_name(const uint8_t *packet, uint32_t len, uint32_t *pos)
{
    uint32_t p;
    if (!packet || !pos) {
        return -1;
    }
    p = *pos;
    while (p < len) {
        uint8_t label = packet[p];
        if (label == 0) {
            *pos = p + 1;
            return 0;
        }
        if ((label & 0xc0u) == 0xc0u) {
            if (p + 2 > len) {
                return -1;
            }
            *pos = p + 2;
            return 0;
        }
        if ((label & 0xc0u) != 0 || p + 1u + label > len) {
            return -1;
        }
        p += 1u + label;
    }
    return -1;
}

/**
 * Net dns build query.
 * @param packet Value supplied by the caller.
 * @param cap Maximum number of elements available in the related buffer.
 * @param name NUL-terminated text supplied by the caller.
 * @param ident Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_dns_build_query(uint8_t *packet, uint32_t cap,
                                    const char *name, uint16_t ident)
{
    uint32_t qname_len = 0;
    uint32_t pos = 12;
    if (cap < 32) {
        return 0;
    }
    net_memzero(packet, cap);
    if (net_dns_encode_name(name, packet + pos, cap - pos, &qname_len) < 0) {
        return 0;
    }
    net_put_u16(packet, ident);
    net_put_u16(packet + 2, 0x0100u);
    net_put_u16(packet + 4, 1);
    pos = 12 + qname_len;
    net_put_u16(packet + pos, 1);
    net_put_u16(packet + pos + 2, 1);
    return pos + 4;
}

/**
 * Net dns parse response.
 * @param packet Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param ident Value supplied by the caller.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_dns_parse_response(const uint8_t *packet, uint32_t len,
                                  uint16_t ident, struct leonos_net_dns *request)
{
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint32_t pos = 12;
    if (!packet || !request || len < 12 || net_get_u16(packet) != ident) {
        return -1;
    }
    flags = net_get_u16(packet + 2);
    if ((flags & 0x8000u) == 0 || (flags & 0x000fu) != 0) {
        return -1;
    }
    qdcount = net_get_u16(packet + 4);
    ancount = net_get_u16(packet + 6);
    for (uint32_t i = 0; i < qdcount; ++i) {
        if (net_dns_skip_name(packet, len, &pos) < 0 || pos + 4 > len) {
            return -1;
        }
        pos += 4;
    }
    request->address_count = 0;
    for (uint32_t i = 0; i < ancount && pos < len; ++i) {
        uint16_t type;
        uint16_t cls;
        uint16_t rdlen;
        if (net_dns_skip_name(packet, len, &pos) < 0 || pos + 10 > len) {
            return -1;
        }
        type = net_get_u16(packet + pos);
        cls = net_get_u16(packet + pos + 2);
        rdlen = net_get_u16(packet + pos + 8);
        pos += 10;
        if (pos + rdlen > len) {
            return -1;
        }
        if (type == 1 && cls == 1 && rdlen == 4 &&
            request->address_count < LEONOS_NET_DNS_MAX_ADDRESSES) {
            request->addresses[request->address_count++] = net_get_u32(packet + pos);
        }
        pos += rdlen;
    }
    return request->address_count ? 0 : -2;
}

/**
 * Net dns resolve.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_dns_resolve(struct leonos_net_dns *request)
{
    uint8_t query[NET_DNS_PACKET_MAX];
    uint8_t response[NET_DNS_PACKET_MAX];
    uint8_t dst_mac[6];
    struct net_udp_wait udp_wait;
    uint32_t timeout_ms;
    uint32_t query_len;
    uint32_t arp_ip;
    uint16_t ident;
    uint16_t local_port;
    uint64_t start;
    uint32_t spins = 0;
    int ret;

    if (!request || !request->name[0]) {
        if (request) {
            request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        }
        return 0;
    }
    request->status = LEONOS_NET_STATUS_DNS_FAILED;
    request->address_count = 0;
    for (uint32_t i = 0; i < LEONOS_NET_DNS_MAX_ADDRESSES; ++i) {
        request->addresses[i] = 0;
    }
    if (!e1000_is_ready()) {
        request->status = LEONOS_NET_STATUS_NO_DEVICE;
        return 0;
    }
    timeout_ms = request->timeout_ms ? request->timeout_ms : 3000u;
    if (timeout_ms > 10000u) {
        timeout_ms = 10000u;
    }
    {
        uint32_t lease_status = net_ensure_ipv4_config(timeout_ms, 1);
        if (lease_status != LEONOS_NET_STATUS_OK) {
            request->status = lease_status;
            return 0;
        }
    }
    if (!net_config.dns_ip) {
        request->status = LEONOS_NET_STATUS_DNS_FAILED;
        return 0;
    }
    ident = (uint16_t)(net_sequence++ & 0xffffu);
    local_port = (uint16_t)(49152u + (ident & 0x3fffu));
    query_len = net_dns_build_query(query, sizeof(query), request->name, ident);
    if (!query_len) {
        request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return 0;
    }
    arp_ip = net_route_arp_ip(net_config.dns_ip);
    ret = net_resolve_mac(arp_ip, timeout_ms, dst_mac);
    if (ret < 0) {
        request->status = ret == -1 ? LEONOS_NET_STATUS_TX_FAILED
                                    : LEONOS_NET_STATUS_ARP_TIMEOUT;
        return 0;
    }
    if (net_send_udp_to_mac(dst_mac, net_config.local_ip, net_config.dns_ip,
                            local_port, NET_DNS_PORT, query, query_len) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return 0;
    }
    start = time_uptime_ms();
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        udp_wait = (struct net_udp_wait){
            .src_ip = net_config.dns_ip,
            .src_port = NET_DNS_PORT,
            .dst_port = local_port,
            .payload = response,
            .capacity = sizeof(response),
        };
        net_poll_once(0, 0, &udp_wait, 0);
        if (udp_wait.done) {
            ret = net_dns_parse_response(response, udp_wait.length, ident, request);
            request->status = ret == 0 ? LEONOS_NET_STATUS_OK
                                       : LEONOS_NET_STATUS_DNS_NO_ANSWER;
            return 0;
        }
        net_cpu_relax();
    }
    request->status = LEONOS_NET_STATUS_DNS_TIMEOUT;
    return 0;
}

/**
 * Net ntp sync.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_ntp_sync(struct leonos_time_sync *request)
{
    struct leonos_net_dns dns;
    struct net_udp_wait udp_wait;
    uint8_t packet[NET_NTP_PACKET_LEN];
    uint8_t response[NET_NTP_PACKET_LEN];
    uint8_t dst_mac[6];
    uint32_t timeout_ms;
    uint32_t arp_ip;
    uint16_t local_port;
    uint64_t start;
    uint32_t spins = 0;
    int ret;
    if (!request) {
        return -1;
    }
    request->status = LEONOS_NET_STATUS_DNS_FAILED;
    request->server_ip = 0;
    request->valid = 0;
    request->unix_seconds = 0;
    if (!request->server[0]) {
        const char *fallback = "pool.ntp.org";
        uint32_t i = 0;
        while (fallback[i] && i + 1U < sizeof(request->server)) {
            request->server[i] = fallback[i];
            ++i;
        }
        request->server[i] = 0;
    }
    timeout_ms = request->timeout_ms ? request->timeout_ms : 4000u;
    if (timeout_ms > 10000u) {
        timeout_ms = 10000u;
    }
    dns = (struct leonos_net_dns){0};
    for (uint32_t i = 0; request->server[i] && i + 1U < sizeof(dns.name); ++i) {
        dns.name[i] = request->server[i];
    }
    dns.timeout_ms = timeout_ms;
    (void)net_dns_resolve(&dns);
    if (dns.status != LEONOS_NET_STATUS_OK || !dns.address_count ||
        !dns.addresses[0]) {
        request->status = dns.status;
        return 0;
    }
    request->server_ip = dns.addresses[0];
    arp_ip = net_route_arp_ip(request->server_ip);
    ret = net_resolve_mac(arp_ip, timeout_ms, dst_mac);
    if (ret < 0) {
        request->status = ret == -1 ? LEONOS_NET_STATUS_TX_FAILED
                                    : LEONOS_NET_STATUS_ARP_TIMEOUT;
        return 0;
    }
    net_memzero(packet, sizeof(packet));
    packet[0] = 0x23u;
    local_port = (uint16_t)(49152u + ((net_sequence++ >> 1U) & 0x3fffu));
    if (net_send_udp_to_mac(dst_mac, net_config.local_ip, request->server_ip,
                            local_port, NET_NTP_PORT, packet,
                            sizeof(packet)) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return 0;
    }
    start = time_uptime_ms();
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        udp_wait = (struct net_udp_wait){
            .src_ip = request->server_ip,
            .src_port = NET_NTP_PORT,
            .dst_port = local_port,
            .payload = response,
            .capacity = sizeof(response),
        };
        net_poll_once(0, 0, &udp_wait, 0);
        if (udp_wait.done) {
            uint64_t ntp_seconds;
            if (udp_wait.length < NET_NTP_PACKET_LEN ||
                ((response[0] & 7u) != 4u && (response[0] & 7u) != 5u) ||
                response[1] == 0 || response[1] > 15u) {
                request->status = LEONOS_NET_STATUS_HTTP_FAILED;
                return 0;
            }
            ntp_seconds = net_get_u32(response + 40);
            if (ntp_seconds < NET_NTP_UNIX_EPOCH_OFFSET ||
                time_set_wall_clock(ntp_seconds - NET_NTP_UNIX_EPOCH_OFFSET) < 0) {
                request->status = LEONOS_NET_STATUS_HTTP_FAILED;
                return 0;
            }
            request->unix_seconds = ntp_seconds - NET_NTP_UNIX_EPOCH_OFFSET;
            request->valid = 1;
            request->status = LEONOS_NET_STATUS_OK;
            return 0;
        }
        net_cpu_relax();
    }
    request->status = LEONOS_NET_STATUS_DNS_TIMEOUT;
    return 0;
}

/**
 * Net parse ipv4 literal.
 * @param text NUL-terminated text supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @param out_ip Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_parse_ipv4_literal(const char *text, uint32_t len,
                                  uint32_t *out_ip)
{
    uint32_t parts[4] = {0, 0, 0, 0};
    uint32_t part = 0;
    uint32_t value = 0;
    uint32_t digits = 0;
    if (!text || !out_ip || len == 0) {
        return -1;
    }
    for (uint32_t i = 0; i <= len; ++i) {
        char ch = i < len ? text[i] : '.';
        if (ch >= '0' && ch <= '9') {
            value = value * 10u + (uint32_t)(ch - '0');
            if (value > 255u) {
                return -1;
            }
            ++digits;
            continue;
        }
        if (ch != '.' || digits == 0 || part >= 4) {
            return -1;
        }
        parts[part++] = value;
        value = 0;
        digits = 0;
    }
    if (part != 4) {
        return -1;
    }
    *out_ip = (parts[0] << 24) | (parts[1] << 16) |
              (parts[2] << 8) | parts[3];
    return 0;
}

/**
 * Net http resolve hosts.
 * @param host Value supplied by the caller.
 * @param host_len Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @param out_ips Value supplied by the caller.
 * @param out_count Value supplied by the caller.
 * @param out_status Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_http_resolve_hosts(const char *host, uint32_t host_len,
                                  uint32_t timeout_ms, uint32_t *out_ips,
                                  uint32_t *out_count,
                                  uint32_t *out_status)
{
    struct leonos_net_dns dns;
    uint32_t literal_ip;
    if (!out_ips || !out_count) {
        return -1;
    }
    *out_count = 0;
    if (net_parse_ipv4_literal(host, host_len, &literal_ip) == 0) {
        out_ips[0] = literal_ip;
        *out_count = 1;
        return 0;
    }
    dns = (struct leonos_net_dns){0};
    dns.timeout_ms = timeout_ms;
    dns.status = LEONOS_NET_STATUS_DNS_FAILED;
    for (uint32_t i = 0; i < host_len && i + 1u < sizeof(dns.name); ++i) {
        dns.name[i] = host[i];
    }
    if (net_dns_resolve(&dns) < 0) {
        if (out_status) {
            *out_status = LEONOS_NET_STATUS_DNS_FAILED;
        }
        return -1;
    }
    if (dns.status != LEONOS_NET_STATUS_OK || dns.address_count == 0) {
        if (out_status) {
            *out_status = dns.status;
        }
        return -1;
    }
    for (uint32_t i = 0; i < dns.address_count &&
             *out_count < LEONOS_NET_DNS_MAX_ADDRESSES; ++i) {
        if (dns.addresses[i]) {
            out_ips[(*out_count)++] = dns.addresses[i];
        }
    }
    if (*out_count == 0) {
        if (out_status) {
            *out_status = LEONOS_NET_STATUS_DNS_NO_ANSWER;
        }
        return -1;
    }
    return 0;
}

/**
 * Net socket alloc.
 * @param owner_pid Value supplied by the caller.
 * @param owner_uid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static struct net_socket *net_socket_alloc(uint32_t owner_pid, uint32_t owner_uid)
{
    struct net_socket *slot = 0;
    net_socket_gc();
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        if (!net_sockets[i].used) {
            slot = &net_sockets[i];
            break;
        }
    }
    if (!slot) {
        for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
            if (!net_sockets[i].fd_owned && net_sockets[i].state == LEONOS_NET_TCP_CLOSED) {
                slot = &net_sockets[i];
                break;
            }
        }
    }
    if (!slot) {
        for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
            if (!net_sockets[i].fd_owned && net_sockets[i].state == LEONOS_NET_TCP_TIME_WAIT) {
                slot = &net_sockets[i];
                break;
            }
        }
    }
    if (!slot) {
        return 0;
    }
    net_socket_clear(slot);
    slot->rx = kernel_malloc(NET_SOCKET_RX_CAP);
    if (!slot->rx) return 0;
    slot->used = 1;
    slot->handle = net_next_socket_handle++;
    if (net_next_socket_handle <= 0) {
        net_next_socket_handle = 1;
    }
    slot->owner_pid = owner_pid;
    slot->owner_uid = owner_uid;
    slot->state = LEONOS_NET_TCP_CLOSED;
    slot->status = LEONOS_NET_STATUS_SOCKET_CLOSED;
    slot->created_ms = net_now32();
    slot->changed_ms = slot->created_ms;
    return slot;
}

/**
 * Net socket clamp timeout.
 * @param timeout_ms Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_socket_clamp_timeout(uint32_t timeout_ms)
{
    if (!timeout_ms) {
        timeout_ms = NET_SOCKET_DEFAULT_TIMEOUT_MS;
    }
    if (timeout_ms > LEONOS_NET_MAX_TIMEOUT_MS) {
        timeout_ms = LEONOS_NET_MAX_TIMEOUT_MS;
    }
    return timeout_ms;
}

/**
 * Net socket open.
 * @param request Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @param owner_uid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_socket_open(struct leonos_net_socket_open *request, uint32_t owner_pid,
                    uint32_t owner_uid)
{
    struct net_socket *socket;
    if (!request) {
        return -1;
    }
    request->socket = -1;
    request->status = LEONOS_NET_STATUS_PROTOCOL_UNSUPPORTED;
    if (request->domain != LEONOS_NET_AF_INET ||
        request->type != LEONOS_NET_SOCK_STREAM ||
        (request->protocol != 0 &&
         request->protocol != LEONOS_NET_IPPROTO_TCP)) {
        return 0;
    }
    socket = net_socket_alloc(owner_pid, owner_uid);
    if (!socket) {
        request->status = LEONOS_NET_STATUS_SOCKET_LIMIT;
        return 0;
    }
    request->socket = socket->handle;
    request->status = LEONOS_NET_STATUS_OK;
    return 0;
}

/**
 * Net socket connect ip.
 * @param socket Value supplied by the caller.
 * @param remote_ip Value supplied by the caller.
 * @param remote_port Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_socket_connect_ip(struct net_socket *socket,
                                      uint32_t remote_ip, uint16_t remote_port,
                                      uint32_t timeout_ms, bool asynchronous)
{
    uint8_t dst_mac[6];
    uint32_t arp_ip;
    uint32_t syn_seq;
    uint64_t start;
    uint64_t next_retransmit;
    uint32_t spins = 0;
    int ret;

    if (!socket || !remote_ip || !remote_port) {
        return LEONOS_NET_STATUS_BAD_ARGUMENT;
    }
    arp_ip = net_route_arp_ip(remote_ip);
    ret = net_resolve_mac(arp_ip, timeout_ms, dst_mac);
    if (ret < 0) {
        return ret == -1 ? LEONOS_NET_STATUS_TX_FAILED
                         : LEONOS_NET_STATUS_ARP_TIMEOUT;
    }

    socket->state = LEONOS_NET_TCP_SYN_SENT;
    socket->rx_head = 0;
    socket->rx_window_valid = false;
    socket->status = LEONOS_NET_STATUS_TCP_TIMEOUT;
    socket->local_ip = net_config.local_ip;
    socket->remote_ip = remote_ip;
    socket->remote_port = remote_port;
    socket->local_seq = 0x4c4e0000u ^ (net_sequence++ << 8) ^
                        (uint32_t)time_uptime_ms();
    socket->local_port = (uint16_t)(49152u + (socket->local_seq & 0x3fffu));
    socket->remote_seq = 0;
    socket->acked_seq = 0;
    socket->rx_len = 0;
    socket->fin_received = 0;
    net_memcpy(socket->dst_mac, dst_mac, 6);
    net_socket_touch(socket);

    syn_seq = socket->local_seq;
    if (net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                            socket->remote_ip, socket->local_port,
                            socket->remote_port, syn_seq, 0,
                            TCP_FLAG_SYN, 0, 0) < 0) {
        net_socket_mark_closed(socket, LEONOS_NET_STATUS_TX_FAILED);
        return socket->status;
    }
    ++socket->local_seq;
    start = time_uptime_ms();
    next_retransmit = start + NET_TCP_SYN_RETRANSMIT_MS;
    if (asynchronous) {
        socket->syn_deadline = start + timeout_ms;
        socket->syn_retransmit = next_retransmit;
        return LEONOS_NET_STATUS_TCP_TIMEOUT;
    }
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        uint64_t now;
        net_poll_once(0, 0, 0, 0);
        if (socket->state == LEONOS_NET_TCP_ESTABLISHED) {
            return LEONOS_NET_STATUS_OK;
        }
        if (socket->state == LEONOS_NET_TCP_CLOSED) {
            return socket->status;
        }
        now = time_uptime_ms();
        if (now >= next_retransmit) {
            if (net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                    socket->remote_ip, socket->local_port,
                                    socket->remote_port, syn_seq, 0,
                                    TCP_FLAG_SYN, 0, 0) < 0) {
                net_socket_mark_closed(socket, LEONOS_NET_STATUS_TX_FAILED);
                return socket->status;
            }
            next_retransmit = now + NET_TCP_SYN_RETRANSMIT_MS;
        }
        net_cpu_relax();
    }
    net_socket_mark_closed(socket, LEONOS_NET_STATUS_TCP_TIMEOUT);
    return socket->status;
}

/**
 * Net socket connect.
 * @param request Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_socket_connect(struct leonos_net_socket_connect *request,
                       uint32_t owner_pid)
{
    struct net_socket *socket;
    uint32_t remote_ips[LEONOS_NET_DNS_MAX_ADDRESSES];
    uint32_t remote_count = 0;
    uint32_t timeout_ms;
    uint32_t host_len;
    uint32_t status = LEONOS_NET_STATUS_TCP_FAILED;
    uint32_t literal_ip = 0;
    int literal = 0;

    if (!request) {
        return -1;
    }
    request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
    request->remote_ip = 0;
    request->local_ip = 0;
    request->local_port = 0;
    if (!request->host[0] || request->port == 0 || request->port > 65535u) {
        return 0;
    }
    socket = net_socket_find(request->socket, owner_pid, 1);
    if (!socket) {
        request->status = LEONOS_NET_STATUS_SOCKET_BAD_HANDLE;
        return 0;
    }
    if (socket->state != LEONOS_NET_TCP_CLOSED) {
        request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return 0;
    }
    if (!e1000_is_ready()) {
        request->status = LEONOS_NET_STATUS_NO_DEVICE;
        return 0;
    }
    timeout_ms = net_socket_clamp_timeout(request->timeout_ms);
    host_len = net_strlen(request->host, LEONOS_NET_HOSTNAME_LEN);
    if (host_len == 0 || host_len >= LEONOS_NET_HOSTNAME_LEN) {
        request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return 0;
    }
    literal = net_parse_ipv4_literal(request->host, host_len, &literal_ip) == 0;
    {
        uint32_t config_status = net_ensure_ipv4_config(timeout_ms, literal ? 0 : 1);
        if (config_status != LEONOS_NET_STATUS_OK) {
            request->status = config_status;
            return 0;
        }
    }
    if (literal) {
        remote_ips[0] = literal_ip;
        remote_count = 1;
    } else if (net_http_resolve_hosts(request->host, host_len, timeout_ms,
                                      remote_ips, &remote_count,
                                      &status) < 0) {
        request->status = status;
        return 0;
    }
    for (uint32_t i = 0; i < remote_count; ++i) {
        status = net_socket_connect_ip(socket, remote_ips[i],
                                       (uint16_t)request->port, timeout_ms, false);
        if (status == LEONOS_NET_STATUS_OK) {
            request->status = status;
            request->remote_ip = socket->remote_ip;
            request->local_ip = socket->local_ip;
            request->local_port = socket->local_port;
            return 0;
        }
    }
    request->status = status;
    return 0;
}

/**
 * Net socket send.
 * @param request Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_socket_send(struct leonos_net_socket_io *request, uint32_t owner_pid)
{
    struct net_socket *socket;
    const uint8_t *data;
    uint32_t timeout_ms;

    if (!request) {
        return -1;
    }
    request->transferred = 0;
    request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
    if (request->length && !request->buffer) {
        return 0;
    }
    socket = net_socket_find(request->socket, owner_pid, 1);
    if (!socket) {
        request->status = LEONOS_NET_STATUS_SOCKET_BAD_HANDLE;
        return 0;
    }
    if (socket->shutdown_write) { request->status = LEONOS_NET_STATUS_SOCKET_CLOSED; return 0; }
    if (socket->state == LEONOS_NET_TCP_TIME_WAIT ||
        socket->state == LEONOS_NET_TCP_CLOSED) {
        request->status = LEONOS_NET_STATUS_SOCKET_CLOSED;
        return 0;
    }
    if (socket->state != LEONOS_NET_TCP_ESTABLISHED) {
        request->status = LEONOS_NET_STATUS_SOCKET_NOT_CONNECTED;
        return 0;
    }
    timeout_ms = net_socket_clamp_timeout(request->timeout_ms);
    data = (const uint8_t *)request->buffer;
    while (request->transferred < request->length) {
        uint32_t chunk = request->length - request->transferred;
        uint32_t seq = socket->local_seq;
        uint32_t target_seq;
        uint64_t start;
        uint64_t next_retransmit;
        uint32_t spins = 0;
        if (chunk > NET_TCP_MSS) {
            chunk = NET_TCP_MSS;
        }
        target_seq = seq + chunk;
        if (net_socket_trace_tls(socket)) {
            console_printf("[net] tls tx socket=%d seq=%u ack=%u bytes=%u target=%u\n",
                           socket->handle, seq, socket->remote_seq, chunk,
                           target_seq);
        }
        if (net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                socket->remote_ip, socket->local_port,
                                socket->remote_port, seq,
                                socket->remote_seq,
                                TCP_FLAG_PSH | TCP_FLAG_ACK,
                                data + request->transferred, chunk) < 0) {
            request->status = LEONOS_NET_STATUS_TX_FAILED;
            return 0;
        }
        socket->local_seq = target_seq;
        socket->tx_bytes += chunk;
        net_socket_touch(socket);
        start = time_uptime_ms();
        next_retransmit = start + NET_TCP_DATA_RETRANSMIT_MS;
        while (!net_timeout_expired(start, timeout_ms, spins++)) {
            uint64_t now;
            net_poll_once(0, 0, 0, 0);
            if (socket->state == LEONOS_NET_TCP_CLOSED) {
                request->status = socket->status;
                return 0;
            }
            if (net_tcp_seq_after_or_equal(socket->acked_seq, target_seq)) {
                request->transferred += chunk;
                break;
            }
            now = time_uptime_ms();
            if (now >= next_retransmit) {
                if (net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                        socket->remote_ip, socket->local_port,
                                        socket->remote_port, seq,
                                        socket->remote_seq,
                                        TCP_FLAG_PSH | TCP_FLAG_ACK,
                                        data + request->transferred,
                                        chunk) < 0) {
                    request->status = LEONOS_NET_STATUS_TX_FAILED;
                    return 0;
                }
                next_retransmit = now + NET_TCP_DATA_RETRANSMIT_MS;
            }
            net_cpu_relax();
        }
        if (!net_tcp_seq_after_or_equal(socket->acked_seq, target_seq)) {
            request->status = LEONOS_NET_STATUS_TCP_TIMEOUT;
            if (net_socket_trace_tls(socket)) {
                console_printf("[net] tls tx timeout socket=%d target=%u acked=%u state=%u\n",
                               socket->handle, target_seq, socket->acked_seq,
                               socket->state);
            }
            return 0;
        }
    }
    request->status = LEONOS_NET_STATUS_OK;
    return 0;
}

/**
 * Net socket recv.
 * @param request Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_socket_recv(struct leonos_net_socket_io *request, uint32_t owner_pid)
{
    struct net_socket *socket;
    uint8_t *dst;
    uint32_t timeout_ms;
    uint64_t start;
    uint32_t spins = 0;

    if (!request) {
        return -1;
    }
    request->transferred = 0;
    request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
    if (request->length && !request->buffer) {
        return 0;
    }
    socket = net_socket_find(request->socket, owner_pid, 1);
    if (!socket) {
        request->status = LEONOS_NET_STATUS_SOCKET_BAD_HANDLE;
        return 0;
    }
    if (!request->length || socket->shutdown_read) { request->status = LEONOS_NET_STATUS_OK; return 0; }
    if (socket->state != LEONOS_NET_TCP_ESTABLISHED &&
        socket->state != LEONOS_NET_TCP_TIME_WAIT) {
        request->status = socket->state == LEONOS_NET_TCP_CLOSED
                              ? LEONOS_NET_STATUS_SOCKET_CLOSED
                              : LEONOS_NET_STATUS_SOCKET_NOT_CONNECTED;
        return 0;
    }
    dst = (uint8_t *)request->buffer;
    timeout_ms = net_socket_clamp_timeout(request->timeout_ms);
    start = time_uptime_ms();
    while (socket->rx_len == 0 && !socket->fin_received &&
           socket->state != LEONOS_NET_TCP_CLOSED &&
           !net_timeout_expired(start, timeout_ms, spins++)) {
        net_poll_once(0, 0, 0, 0);
        net_cpu_relax();
    }
    if (socket->rx_len) {
        uint32_t copy_len = request->length;
        if (copy_len > socket->rx_len) {
            copy_len = socket->rx_len;
        }
        if (copy_len) {
            uint32_t first = NET_SOCKET_RX_CAP - socket->rx_head;
            if (first > copy_len) first = copy_len;
            net_memcpy(dst, socket->rx + socket->rx_head, first);
            net_memcpy(dst + first, socket->rx, copy_len - first);
            socket->rx_head = (socket->rx_head + copy_len) % NET_SOCKET_RX_CAP;
            socket->rx_len -= copy_len;
            uint32_t remaining = net_socket_receive_window(socket);
            uint32_t window = net_socket_select_window(socket);
            /* Linux tcp_cleanup_rbuf() sends a window update after a
             * significant increase, including reopening a zero window. */
            if (!socket->fin_received && window > remaining && window >= 2u * remaining) {
                (void)net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                          socket->remote_ip, socket->local_port,
                                          socket->remote_port, socket->local_seq,
                                          socket->remote_seq, TCP_FLAG_ACK, 0, 0);
            }
        }
        request->transferred = copy_len;
        request->status = LEONOS_NET_STATUS_OK;
        return 0;
    }
    if (socket->fin_received || socket->state == LEONOS_NET_TCP_TIME_WAIT) {
        request->status = LEONOS_NET_STATUS_OK;
        return 0;
    }
    if (socket->state == LEONOS_NET_TCP_CLOSED) {
        request->status = socket->status;
        return 0;
    }
    request->status = LEONOS_NET_STATUS_TCP_TIMEOUT;
    if (net_socket_trace_tls(socket)) {
        console_printf("[net] tls rx timeout socket=%d state=%u expected=%u queued=%u fin=%u\n",
                       socket->handle, socket->state, socket->remote_seq,
                       socket->rx_len, socket->fin_received);
    }
    return 0;
}

/**
 * Net socket close.
 * @param request Value supplied by the caller.
 * @param owner_pid Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_socket_close(struct leonos_net_socket_close *request,
                     uint32_t owner_pid)
{
    struct net_socket *socket;
    if (!request) {
        return -1;
    }
    request->status = LEONOS_NET_STATUS_SOCKET_BAD_HANDLE;
    socket = net_socket_find(request->socket, owner_pid, 1);
    if (!socket) {
        return 0;
    }
    if (socket->state == LEONOS_NET_TCP_ESTABLISHED) {
        /**
 * Net socket touch.
 * @param dst_mac Value supplied by the caller.
 * @param local_ip Value supplied by the caller.
 * @param remote_ip Value supplied by the caller.
 * @param local_port Value supplied by the caller.
 * @param remote_port Value supplied by the caller.
 * @param local_seq Value supplied by the caller.
 * @param remote_seq Value supplied by the caller.
 * @param TCP_FLAG_ACK Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
        (void)net_send_tcp_to_mac(socket->dst_mac, net_config.local_ip,
                                  socket->remote_ip, socket->local_port,
                                  socket->remote_port, socket->local_seq,
                                  socket->remote_seq,
                                  TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
        ++socket->local_seq;
        socket->state = LEONOS_NET_TCP_TIME_WAIT;
        socket->status = LEONOS_NET_STATUS_OK;
        net_socket_touch(socket);
    } else if (socket->state == LEONOS_NET_TCP_SYN_SENT) {
        net_socket_mark_closed(socket, LEONOS_NET_STATUS_SOCKET_CLOSED);
    } else if (socket->state == LEONOS_NET_TCP_CLOSED) {
        socket->status = LEONOS_NET_STATUS_SOCKET_CLOSED;
        net_socket_touch(socket);
    }
    request->status = LEONOS_NET_STATUS_OK;
    return 0;
}

/**
 * Net connection visible.
 * @param socket Value supplied by the caller.
 * @param viewer Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static int net_connection_visible(const struct net_socket *socket,
                                  const struct task *viewer)
{
    if (!socket || !viewer) {
        return 0;
    }
    if (viewer->role == LEONOS_AUTH_ROLE_ADMIN) {
        return 1;
    }
    if ((viewer->flags & TASK_FLAG_SERVICE) &&
        !(viewer->flags & TASK_FLAG_WINDOW_SERVER)) {
        return 1;
    }
    return viewer->uid != 0 && socket->owner_uid == viewer->uid;
}

/**
 * Net connections.
 * @param request Value supplied by the caller.
 * @param viewer Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_connections(struct leonos_net_connection_list *request,
                    const struct task *viewer)
{
    uint32_t now = net_now32();
    uint32_t count = 0;
    if (!request) {
        return -1;
    }
    net_socket_gc();
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *socket = &net_sockets[i];
        if (!socket->used || !net_connection_visible(socket, viewer)) {
            continue;
        }
        if (request->entries && count < request->capacity) {
            request->entries[count] = (struct leonos_net_connection_info){
                .socket = socket->handle,
                .owner_pid = socket->owner_pid,
                .state = socket->state,
                .status = socket->status,
                .local_ip = socket->local_ip,
                .remote_ip = socket->remote_ip,
                .local_port = socket->local_port,
                .remote_port = socket->remote_port,
                .age_ms = now - socket->created_ms,
                .tx_bytes = socket->tx_bytes,
                .rx_bytes = socket->rx_bytes,
            };
        }
        ++count;
    }
    request->count = count;
    return 0;
}

/**
 * Net close owner sockets.
 * @param owner_pid Value supplied by the caller.
 */
short net_socket_poll_fd(int32_t handle, uint32_t owner_pid, short events)
{
    net_poll_packets();
    struct net_socket *socket = net_socket_find(handle, owner_pid, 1);
    short result = 0;
    if (!socket) return POLLNVAL;
    if (socket->state == LEONOS_NET_TCP_SYN_SENT && socket->syn_deadline) {
        uint64_t now = time_uptime_ms();
        if (now >= socket->syn_deadline) {
            socket->error = LINUX_ETIMEDOUT;
            net_socket_mark_closed(socket, LEONOS_NET_STATUS_TCP_TIMEOUT);
        } else if (now >= socket->syn_retransmit) {
            (void)net_send_tcp_to_mac(socket->dst_mac, socket->local_ip, socket->remote_ip,
                socket->local_port, socket->remote_port, socket->local_seq - 1, 0, TCP_FLAG_SYN, 0, 0);
            socket->syn_retransmit = now + NET_TCP_SYN_RETRANSMIT_MS;
        }
    }
    if (socket->state == LEONOS_NET_TCP_ESTABLISHED) {
        if ((events & POLLIN) && (socket->rx_len || socket->fin_received || socket->shutdown_read)) result |= POLLIN;
        if (events & POLLOUT) result |= POLLOUT;
    } else if (socket->state == LEONOS_NET_TCP_TIME_WAIT) {
        result |= POLLIN | POLLHUP;
    } else if (socket->state == LEONOS_NET_TCP_CLOSED) {
        result |= POLLHUP | (events & POLLOUT);
    }
    if (socket->error) result |= POLLERR;
    return result;
}

/* The descriptor table owns one reference per open file description. PID
 * filtering belongs to the management API, not inherited Linux descriptors. */
void net_socket_pin_fd(int32_t handle)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    if (s) s->fd_owned = true;
}

void net_socket_release_fd(int32_t handle)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    if (!s) return;
    struct leonos_net_socket_close request = {.socket = handle};
    (void)net_socket_close(&request, 0);
    s->fd_owned = false;
}

int net_socket_error(int32_t handle, bool clear)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    if (!s) return LINUX_EBADF;
    int error = s->error;
    if (clear) s->error = 0;
    return error;
}

int net_socket_available(int32_t handle)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    return s ? (int)s->rx_len : -LINUX_EBADF;
}

int net_socket_connect_fd(int32_t handle, uint32_t ip, uint16_t port, bool nonblock)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    if (!s) return -LINUX_EBADF;
    if (s->state == LEONOS_NET_TCP_SYN_SENT) return -LINUX_EALREADY;
    if (s->state != LEONOS_NET_TCP_CLOSED) return -LINUX_EISCONN;
    if (!e1000_is_ready()) return -LINUX_ENETDOWN;
    if (!ip || !port) return -LINUX_EINVAL;
    if (net_ensure_ipv4_config(3000, 0) != LEONOS_NET_STATUS_OK) return -LINUX_ENETUNREACH;
    s->error = 0;
    uint32_t status = net_socket_connect_ip(s, ip, port, 10000, nonblock);
    if (s->state == LEONOS_NET_TCP_SYN_SENT) return -LINUX_EINPROGRESS;
    if (status == LEONOS_NET_STATUS_OK) return 0;
    if (s->error) return -s->error;
    return status == LEONOS_NET_STATUS_ARP_TIMEOUT ? -LINUX_EHOSTUNREACH : -LINUX_ETIMEDOUT;
}

int net_socket_shutdown_fd(int32_t handle, int how)
{
    struct net_socket *s = net_socket_find(handle, 0, 1);
    if (!s) return -LINUX_EBADF;
    if (how < 0 || how > 2) return -LINUX_EINVAL;
    if (s->state != LEONOS_NET_TCP_ESTABLISHED && s->state != LEONOS_NET_TCP_TIME_WAIT)
        return -LINUX_ENOTCONN;
    if (how != 1) { s->shutdown_read = true; s->rx_len = 0; }
    if (how != 0 && !s->shutdown_write) {
        if (net_send_tcp_to_mac(s->dst_mac, s->local_ip, s->remote_ip, s->local_port,
            s->remote_port, s->local_seq, s->remote_seq, TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0) < 0)
            return -LINUX_EIO;
        ++s->local_seq;
        s->shutdown_write = true;
    }
    return 0;
}

int net_socket_address(int32_t handle, uint32_t owner_pid,
                       uint32_t *local_ip, uint16_t *local_port,
                       uint32_t *remote_ip, uint16_t *remote_port)
{
    struct net_socket *socket = net_socket_find(handle, owner_pid, 1);
    if (!socket) return -9;
    if (local_ip) *local_ip = socket->local_ip;
    if (local_port) *local_port = socket->local_port;
    if (remote_ip) *remote_ip = socket->remote_ip;
    if (remote_port) *remote_port = socket->remote_port;
    return 0;
}

void net_close_owner_sockets(uint32_t owner_pid)
{
    if (!owner_pid) {
        return;
    }
    for (uint32_t i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *socket = &net_sockets[i];
        if (socket->used && !socket->fd_owned && socket->owner_pid == owner_pid &&
            socket->state != LEONOS_NET_TCP_CLOSED) {
            net_socket_mark_closed(socket, LEONOS_NET_STATUS_SOCKET_CLOSED);
        }
    }
}

/**
 * Net driver detached.
 */
void net_driver_detached(void)
{
    for (unsigned i = 0; i < LEONOS_NET_SOCKET_MAX; ++i) {
        struct net_socket *s = &net_sockets[i];
        if (s->used) { s->error = LINUX_ENETDOWN; net_socket_mark_closed(s, LEONOS_NET_STATUS_NO_DEVICE); }
    }
    net_arp_cache_clear();
    net_set_static_fallback();
    net_update_config_flags();
}

/**
 * Net http build request.
 * @param dst Value supplied by the caller.
 * @param cap Maximum number of elements available in the related buffer.
 * @param host Value supplied by the caller.
 * @param path NUL-terminated text supplied by the caller.
 * @param port Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_http_build_request(char *dst, uint32_t cap,
                                       const char *host, const char *path,
                                       uint32_t port)
{
    uint32_t pos = 0;
    if (!dst || cap == 0 || !host || !host[0]) {
        return 0;
    }
    dst[0] = 0;
    net_append_text(dst, &pos, cap, "GET ");
    if (!path || !path[0]) {
        net_append_char(dst, &pos, cap, '/');
    } else {
        if (path[0] != '/') {
            net_append_char(dst, &pos, cap, '/');
        }
        net_append_text(dst, &pos, cap, path);
    }
    net_append_text(dst, &pos, cap, " HTTP/1.0\r\nHost: ");
    net_append_text(dst, &pos, cap, host);
    if (port && port != NET_HTTP_PORT) {
        net_append_char(dst, &pos, cap, ':');
        net_append_u32(dst, &pos, cap, port);
    }
    net_append_text(dst, &pos, cap,
                    "\r\nConnection: close\r\nUser-Agent: LeonOS/4\r\n\r\n");
    if (pos + 1u >= cap) {
        return 0;
    }
    return pos;
}

/**
 * Net http parse status.
 * @param response Value supplied by the caller.
 * @param len Maximum number of elements available in the related buffer.
 * @return The value or status produced by the operation.
 */
static uint32_t net_http_parse_status(const char *response, uint32_t len)
{
    uint32_t pos = 0;
    uint32_t status = 0;
    if (!response || len < 12) {
        return 0;
    }
    if (response[0] != 'H' || response[1] != 'T' ||
        response[2] != 'T' || response[3] != 'P' ||
        response[4] != '/') {
        return 0;
    }
    while (pos < len && response[pos] != ' ' &&
           response[pos] != '\r' && response[pos] != '\n') {
        ++pos;
    }
    while (pos < len && response[pos] == ' ') {
        ++pos;
    }
    for (uint32_t i = 0; i < 3 && pos < len; ++i, ++pos) {
        if (response[pos] < '0' || response[pos] > '9') {
            return 0;
        }
        status = status * 10u + (uint32_t)(response[pos] - '0');
    }
    return status;
}

/**
 * Net http exchange ip.
 * @param request Value supplied by the caller.
 * @param http_request Value supplied by the caller.
 * @param http_len Value supplied by the caller.
 * @param remote_ip Value supplied by the caller.
 * @param port Value supplied by the caller.
 * @param timeout_ms Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
static uint32_t net_http_exchange_ip(struct leonos_net_http_get *request,
                                     const char *http_request,
                                     uint32_t http_len,
                                     uint32_t remote_ip,
                                     uint32_t port,
                                     uint32_t timeout_ms)
{
    uint8_t dst_mac[6];
    struct net_tcp_wait wait;
    uint32_t arp_ip;
    uint32_t syn_seq;
    uint32_t local_seq;
    uint32_t acked_remote_seq;
    uint16_t local_port;
    uint64_t start;
    uint64_t next_retransmit;
    uint32_t spins;
    int ret;

    request->status = LEONOS_NET_STATUS_TCP_FAILED;
    request->remote_ip = remote_ip;
    request->http_status = 0;
    request->response_len = 0;
    request->response[0] = 0;

    arp_ip = net_route_arp_ip(remote_ip);
    ret = net_resolve_mac(arp_ip, timeout_ms, dst_mac);
    if (ret < 0) {
        request->status = ret == -1 ? LEONOS_NET_STATUS_TX_FAILED
                                    : LEONOS_NET_STATUS_ARP_TIMEOUT;
        return request->status;
    }

    local_seq = 0x4c4e0000u ^ (net_sequence++ << 8) ^
                (uint32_t)time_uptime_ms();
    local_port = (uint16_t)(49152u + (local_seq & 0x3fffu));
    syn_seq = local_seq;
    wait = (struct net_tcp_wait){
        .src_ip = remote_ip,
        .src_port = (uint16_t)port,
        .dst_port = local_port,
        .payload = (uint8_t *)request->response,
        .capacity = LEONOS_NET_HTTP_RESPONSE_MAX - 1u,
    };

    if (net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                            local_port, (uint16_t)port, local_seq, 0,
                            TCP_FLAG_SYN, 0, 0) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return request->status;
    }
    ++local_seq;
    start = time_uptime_ms();
    next_retransmit = start + NET_TCP_SYN_RETRANSMIT_MS;
    spins = 0;
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        uint64_t now;
        net_poll_once(0, 0, 0, &wait);
        if (wait.reset) {
            request->status = LEONOS_NET_STATUS_TCP_RESET;
            return request->status;
        }
        if ((wait.flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                (TCP_FLAG_SYN | TCP_FLAG_ACK) &&
            wait.acked_seq == local_seq && wait.remote_seq) {
            break;
        }
        now = time_uptime_ms();
        if (now >= next_retransmit) {
            if (net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                                    local_port, (uint16_t)port, syn_seq, 0,
                                    TCP_FLAG_SYN, 0, 0) < 0) {
                request->status = LEONOS_NET_STATUS_TX_FAILED;
                return request->status;
            }
            next_retransmit = now + NET_TCP_SYN_RETRANSMIT_MS;
        }
        net_cpu_relax();
    }
    if (!wait.remote_seq || wait.acked_seq != local_seq) {
        request->status = LEONOS_NET_STATUS_TCP_TIMEOUT;
        return request->status;
    }
    if (net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                            local_port, (uint16_t)port, local_seq,
                            wait.remote_seq, TCP_FLAG_ACK, 0, 0) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return request->status;
    }

    if (net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                            local_port, (uint16_t)port, local_seq,
                            wait.remote_seq, TCP_FLAG_PSH | TCP_FLAG_ACK,
                            (const uint8_t *)http_request, http_len) < 0) {
        request->status = LEONOS_NET_STATUS_TX_FAILED;
        return request->status;
    }
    local_seq += http_len;
    acked_remote_seq = wait.remote_seq;
    start = time_uptime_ms();
    next_retransmit = start + NET_TCP_DATA_RETRANSMIT_MS;
    spins = 0;
    while (!net_timeout_expired(start, timeout_ms, spins++)) {
        uint32_t before = wait.changed;
        uint64_t now;
        net_poll_once(0, 0, 0, &wait);
        if (wait.reset) {
            request->status = LEONOS_NET_STATUS_TCP_RESET;
            return request->status;
        }
        if (wait.changed != before && wait.remote_seq != acked_remote_seq) {
            (void)net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                                      local_port, (uint16_t)port, local_seq,
                                      wait.remote_seq, TCP_FLAG_ACK, 0, 0);
            acked_remote_seq = wait.remote_seq;
        }
        if (wait.overflow) {
            request->status = LEONOS_NET_STATUS_HTTP_TOO_LARGE;
            break;
        }
        if (wait.fin) {
            request->status = LEONOS_NET_STATUS_OK;
            break;
        }
        now = time_uptime_ms();
        if (wait.length == 0 && !wait.fin && wait.acked_seq != local_seq &&
            now >= next_retransmit) {
            if (net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                                    local_port, (uint16_t)port,
                                    local_seq - http_len, wait.remote_seq,
                                    TCP_FLAG_PSH | TCP_FLAG_ACK,
                                    (const uint8_t *)http_request,
                                    http_len) < 0) {
                request->status = LEONOS_NET_STATUS_TX_FAILED;
                break;
            }
            next_retransmit = now + NET_TCP_DATA_RETRANSMIT_MS;
        }
        net_cpu_relax();
    }

    request->response_len = wait.length;
    if (request->response_len >= LEONOS_NET_HTTP_RESPONSE_MAX) {
        request->response_len = LEONOS_NET_HTTP_RESPONSE_MAX - 1u;
    }
    request->response[request->response_len] = 0;
    if (request->status != LEONOS_NET_STATUS_HTTP_TOO_LARGE &&
        request->status != LEONOS_NET_STATUS_OK) {
        request->status = wait.length ? LEONOS_NET_STATUS_OK
                                      : LEONOS_NET_STATUS_TCP_TIMEOUT;
    }
    if (request->status == LEONOS_NET_STATUS_OK ||
        request->status == LEONOS_NET_STATUS_HTTP_TOO_LARGE) {
        request->http_status =
            net_http_parse_status(request->response, request->response_len);
        if (request->status == LEONOS_NET_STATUS_OK && !request->http_status) {
            request->status = LEONOS_NET_STATUS_HTTP_FAILED;
        }
    }
    /**
 * Net http get.
 * @param dst_mac Value supplied by the caller.
 * @param local_ip Value supplied by the caller.
 * @param remote_ip Value supplied by the caller.
 * @param local_port Value supplied by the caller.
 * @param port Value supplied by the caller.
 * @param local_seq Value supplied by the caller.
 * @param remote_seq Value supplied by the caller.
 * @param TCP_FLAG_ACK Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
    (void)net_send_tcp_to_mac(dst_mac, net_config.local_ip, remote_ip,
                              local_port, (uint16_t)port, local_seq,
                              wait.remote_seq, TCP_FLAG_FIN | TCP_FLAG_ACK,
                              0, 0);
    return request->status;
}

/**
 * Net http get.
 * @param request Value supplied by the caller.
 * @return The value or status produced by the operation.
 */
int net_http_get(struct leonos_net_http_get *request)
{
    char http_request[NET_HTTP_REQUEST_MAX];
    uint32_t remote_ips[LEONOS_NET_DNS_MAX_ADDRESSES];
    uint32_t remote_count = 0;
    uint32_t timeout_ms;
    uint32_t host_len;
    uint32_t path_len;
    uint32_t port;
    uint32_t http_len;
    uint32_t last_status = LEONOS_NET_STATUS_HTTP_FAILED;

    if (!request) {
        return -1;
    }
    host_len = net_strlen(request->host, LEONOS_NET_HOSTNAME_LEN);
    path_len = net_strlen(request->path, LEONOS_NET_HTTP_PATH_LEN);
    request->status = LEONOS_NET_STATUS_HTTP_FAILED;
    request->remote_ip = 0;
    request->http_status = 0;
    request->response_len = 0;
    request->response[0] = 0;
    if (host_len == 0 || host_len >= LEONOS_NET_HOSTNAME_LEN ||
        path_len >= LEONOS_NET_HTTP_PATH_LEN ||
        request->port > 65535u) {
        request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return 0;
    }
    if (!e1000_is_ready()) {
        request->status = LEONOS_NET_STATUS_NO_DEVICE;
        return 0;
    }
    timeout_ms = request->timeout_ms ? request->timeout_ms
                                     : NET_HTTP_DEFAULT_TIMEOUT_MS;
    if (timeout_ms > NET_HTTP_MAX_TIMEOUT_MS) {
        timeout_ms = NET_HTTP_MAX_TIMEOUT_MS;
    }
    {
        uint32_t config_status = net_ensure_ipv4_config(timeout_ms, 1);
        if (config_status != LEONOS_NET_STATUS_OK) {
            request->status = config_status;
            return 0;
        }
    }
    port = request->port ? request->port : NET_HTTP_PORT;
    http_len = net_http_build_request(http_request, sizeof(http_request),
                                      request->host, request->path, port);
    if (!http_len) {
        request->status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return 0;
    }
    if (net_http_resolve_hosts(request->host, host_len, timeout_ms,
                               remote_ips, &remote_count,
                               &request->status) < 0) {
        return 0;
    }
    for (uint32_t i = 0; i < remote_count; ++i) {
        last_status = net_http_exchange_ip(request, http_request, http_len,
                                          remote_ips[i], port, timeout_ms);
        if (last_status == LEONOS_NET_STATUS_OK ||
            last_status == LEONOS_NET_STATUS_HTTP_TOO_LARGE ||
            last_status == LEONOS_NET_STATUS_HTTP_FAILED ||
            last_status == LEONOS_NET_STATUS_BAD_ARGUMENT ||
            last_status == LEONOS_NET_STATUS_TX_FAILED ||
            last_status == LEONOS_NET_STATUS_ARP_TIMEOUT) {
            return 0;
        }
    }
    request->status = last_status;
    return 0;
}

/**
 * Net device info.
 * @param flags Identifier or flags controlling the operation.
 * @param mac_value Value supplied by the caller.
 * @param local_ip Value supplied by the caller.
 */
void net_device_info(uint32_t *flags, uint64_t *mac_value, uint32_t *local_ip)
{
    struct e1000_info info;
    uint64_t mac = 0;
    e1000_get_info(&info);
    for (uint32_t i = 0; i < 6; ++i) {
        mac |= (uint64_t)info.mac[i] << (i * 8u);
    }
    if (flags) {
        *flags = info.present ? LEONOS_DEVICE_FLAG_PRESENT : 0;
        if (info.active) {
            *flags |= LEONOS_DEVICE_FLAG_ACTIVE;
        }
    }
    if (mac_value) {
        *mac_value = mac;
    }
    if (local_ip) {
        *local_ip = net_config.local_ip;
    }
}

/** @brief Apply/query eth0 through the Linux native interface ioctl ABI.
 * Only the real Ethernet interface and the supported default route are exposed.
 * Unsupported routing/features return an error rather than retaining inert state.
 */
int net_interface_ioctl(uint32_t request, uint64_t address)
{
    bool change = request == SIOCSIFFLAGS || request == SIOCSIFADDR ||
        request == SIOCSIFNETMASK || request == SIOCSIFBRDADDR ||
        request == SIOCSIFMTU || request == SIOCADDRT || request == SIOCDELRT;
    struct task *task = sched_current_task();
    if (change && (!task || !(task->cap_effective & (1ULL << CAP_NET_ADMIN))))
        return -LINUX_EPERM;
    if (request == SIOCADDRT || request == SIOCDELRT) {
        if (!user_range_ok(address, sizeof(struct rtentry))) return -LINUX_EFAULT;
        struct rtentry route;
        net_memcpy(&route, (void *)(uintptr_t)address, sizeof(route));
        if (route.rt_dst.sa_family != AF_INET) return -LINUX_EAFNOSUPPORT;
        uint32_t route_mask = net_get_u32((uint8_t *)&route.rt_genmask + 4);
        if (route.rt_genmask.sa_family != AF_INET && (route.rt_genmask.sa_family || route_mask))
            return -LINUX_EAFNOSUPPORT;
        if (route.rt_dev) {
            char name[IFNAMSIZ] = {0};
            for (unsigned i = 0; i < sizeof(name); ++i) {
                if (!user_range_ok(route.rt_dev + i, 1)) return -LINUX_EFAULT;
                name[i] = *(char *)(uintptr_t)(route.rt_dev + i);
                if (!name[i]) break;
            }
            if (__builtin_memcmp(name, "eth0", 5)) return -LINUX_ENODEV;
        }
        if (!e1000_is_ready()) return -LINUX_ENODEV;
        uint32_t dst = net_get_u32((uint8_t *)&route.rt_dst + 4);
        uint32_t mask = net_get_u32((uint8_t *)&route.rt_genmask + 4);
        uint32_t gateway = route.rt_gateway.sa_family == AF_INET ?
            net_get_u32((uint8_t *)&route.rt_gateway + 4) : 0;
        if (dst || mask || (route.rt_flags & ~3u) || route.rt_metric > 1)
            return -LINUX_EOPNOTSUPP;
        if (request == SIOCDELRT) {
            if (!net_config.gateway_ip || (gateway && gateway != net_config.gateway_ip))
                return -LINUX_ESRCH;
            net_config.gateway_ip = 0;
        } else {
            if ((route.rt_flags & 2) && !gateway) return -LINUX_EINVAL;
            if (!(route.rt_flags & 2)) return -LINUX_EOPNOTSUPP;
            if (!net_interface_up || !net_config.local_ip ||
                (gateway & net_config.subnet_mask) != (net_config.local_ip & net_config.subnet_mask))
                return -LINUX_ENETUNREACH;
            if (net_config.gateway_ip) return -LINUX_EEXIST;
            net_config.gateway_ip = gateway;
        }
        net_arp_cache_clear();
        return 0;
    }
    if (request == SIOCGIFCONF) {
        struct ifconf conf;
        if (!user_range_ok(address, sizeof(conf)) || !user_range_writable(address, sizeof(conf)))
            return -LINUX_EFAULT;
        net_memcpy(&conf, (void *)(uintptr_t)address, sizeof(conf));
        if (conf.ifc_len < 0) return -LINUX_EINVAL;
        int available = e1000_is_ready() && net_config.local_ip ? sizeof(struct ifreq) : 0;
        if (!conf.ifc_buf) conf.ifc_len = available;
        else {
            conf.ifc_len = conf.ifc_len >= available ? available : 0;
            if (conf.ifc_len) {
                if (!user_range_writable(conf.ifc_buf, sizeof(struct ifreq))) return -LINUX_EFAULT;
                struct ifreq result = { .ifr_name = "eth0", .data.addr.sa_family = AF_INET };
                net_put_u32((uint8_t *)&result.data.addr + 4, net_config.local_ip);
                net_memcpy((void *)(uintptr_t)conf.ifc_buf, &result, sizeof(result));
            }
        }
        net_memcpy((void *)(uintptr_t)address, &conf, sizeof(conf));
        return 0;
    }
    switch (request) {
    case SIOCGIFNAME: case SIOCGIFINDEX: case SIOCGIFHWADDR: case SIOCGIFFLAGS:
    case SIOCSIFFLAGS: case SIOCGIFADDR: case SIOCSIFADDR: case SIOCGIFNETMASK:
    case SIOCSIFNETMASK: case SIOCGIFBRDADDR: case SIOCSIFBRDADDR: case SIOCGIFMTU:
    case SIOCSIFMTU: break;
    default: return -LINUX_ENOTTY;
    }
    struct ifreq req;
    if (!user_range_ok(address, sizeof(req)) || (!change && !user_range_writable(address, sizeof(req))))
        return -LINUX_EFAULT;
    net_memcpy(&req, (void *)(uintptr_t)address, sizeof(req));
    if (!e1000_is_ready()) return -LINUX_ENODEV;
    if (request == SIOCGIFNAME) {
        if (req.data.value != 2) return -LINUX_ENODEV;
        net_memzero(req.ifr_name, sizeof(req.ifr_name));
        net_memcpy(req.ifr_name, "eth0", 5);
    } else {
        req.ifr_name[IFNAMSIZ - 1] = 0;
        if (__builtin_memcmp(req.ifr_name, "eth0", 5)) return -LINUX_ENODEV;
        switch (request) {
        case SIOCGIFINDEX: req.data.value = 2; break;
        case SIOCGIFHWADDR:
            net_memzero(&req.data, sizeof(req.data));
            req.data.addr.sa_family = 1; /* ARPHRD_ETHER */
            net_memcpy(req.data.addr.sa_data, e1000_mac(), 6); break;
        case SIOCGIFFLAGS:
            req.data.flags = IFF_BROADCAST | IFF_MULTICAST |
                (net_interface_up ? IFF_UP | IFF_RUNNING : 0); break;
        case SIOCSIFFLAGS:
            if (req.data.flags & ~(IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_MULTICAST))
                return -LINUX_EOPNOTSUPP;
            net_interface_up = !!(req.data.flags & IFF_UP); break;
        case SIOCGIFMTU: req.data.value = 1500; break;
        case SIOCSIFMTU:
            if (req.data.value != 1500) return -LINUX_EOPNOTSUPP;
            break;
        default: {
            uint32_t *value = (request == SIOCGIFADDR || request == SIOCSIFADDR) ? &net_config.local_ip :
                (request == SIOCGIFNETMASK || request == SIOCSIFNETMASK) ? &net_config.subnet_mask : &net_interface_broadcast;
            if (change) {
                if (req.data.addr.sa_family != AF_INET) return -LINUX_EAFNOSUPPORT;
                uint32_t ip = net_get_u32((uint8_t *)&req.data.addr + 4);
                if (request == SIOCSIFNETMASK && ((~ip) & ((~ip) + 1))) return -LINUX_EINVAL;
                if (request == SIOCSIFADDR && ip >= 0xe0000000u) return -LINUX_EINVAL;
                *value = ip;
                if (request == SIOCSIFADDR || request == SIOCSIFNETMASK)
                    net_interface_broadcast = net_config.local_ip | ~net_config.subnet_mask;
                if (!net_config.local_ip) net_config.gateway_ip = 0;
                net_config.source = net_config.local_ip ? LEONOS_NET_CONFIG_SOURCE_STATIC : LEONOS_NET_CONFIG_SOURCE_NONE;
                net_config.lease_seconds = net_config.dhcp_server_ip = 0;
                net_arp_cache_clear();
            } else {
                if (!net_config.local_ip) return -LINUX_EADDRNOTAVAIL;
                net_memzero(&req.data, sizeof(req.data));
                req.data.addr.sa_family = AF_INET;
                net_put_u32((uint8_t *)&req.data.addr + 4, *value);
            }
        } break;
        }
    }
    if (!change) net_memcpy((void *)(uintptr_t)address, &req, sizeof(req));
    return 0;
}

/** @brief Report the real administrative and hardware state of eth0. */
bool net_interface_ready(void) { return net_interface_up && e1000_is_ready(); }
