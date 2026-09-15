/* libnet: credential-checked query adapter plus the standard AF_INET data plane.
 * Exports the historical leonos_net_* and leonos_socket_* entry points. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <leonos/http.h>
#include <leonos/net.h>
#include <leonos/net_control.h>
#include <leonos/openrc.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <leonos/unix_ipc.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

static void net_copy_text(char *dst, uint32_t capacity, const char *src)
{
    uint32_t i = 0;
    if (!dst || !capacity) return;
    while (src && src[i] && i + 1u < capacity) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

/* Read-only queries use the kernel's credential-checked interface. Network
 * lifecycle requests go exclusively to OpenRC's standard DHCP service. */
static int net_control(struct leonos_net_control *control)
{
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    control->version = LEONOS_NET_CONTROL_VERSION;
    int ret = ioctl(fd, LEONOS_NET_CONTROL_IOCTL, control), error = errno;
    close(fd);
    if (!ret && control->result < 0) { error = -control->result; ret = -1; }
    errno = error;
    return ret;
}

/* The lease is data published atomically by the root-owned udhcpc hook.
 * Verify it against the current interface; it is not a source of kernel state. */
static void net_merge_dhcp_lease(struct leonos_net_config *config)
{
    int fd = open("/run/leonos/dhcp-lease", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid || (st.st_mode & 022)) {
        close(fd); return;
    }
    FILE *stream = fdopen(fd, "r");
    if (!stream) { close(fd); return; }
    char line[512];
    uint32_t ip = 0, mask = 0, dns = 0;
    unsigned long long lease = 0, acquired = ~0ULL;
    while (fgets(line, sizeof(line), stream)) {
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals++ = 0;
        equals[strcspn(equals, "\r\n")] = 0;
        if (!strcmp(line, "lease") || !strcmp(line, "acquired")) {
            char *end;
            errno = 0;
            unsigned long long value = strtoull(equals, &end, 10);
            if (errno || !*equals || *equals == '-' || *end) continue;
            if (!strcmp(line, "lease")) lease = value;
            else acquired = value;
        } else {
            /* Display the first DNS server; libc still consumes resolv.conf. */
            equals[strcspn(equals, " \t")] = 0;
            struct in_addr address;
            if (inet_pton(AF_INET, equals, &address) != 1) continue;
            if (!strcmp(line, "ip")) ip = ntohl(address.s_addr);
            else if (!strcmp(line, "subnet")) mask = ntohl(address.s_addr);
            else if (!strcmp(line, "dns")) dns = ntohl(address.s_addr);
        }
    }
    bool failed = ferror(stream);
    fclose(stream);
    struct timespec now;
    if (failed || clock_gettime(CLOCK_MONOTONIC, &now) || now.tv_sec < 0 ||
        acquired > (unsigned long long)now.tv_sec || !lease || lease > UINT32_MAX ||
        (lease != UINT32_MAX && (unsigned long long)now.tv_sec - acquired >= lease) ||
        !ip || ip != config->local_ip || mask != config->subnet_mask) return;
    config->source = LEONOS_NET_CONFIG_SOURCE_DHCP;
    config->flags |= LEONOS_NET_CONFIG_FLAG_DHCP;
    config->lease_seconds = (uint32_t)lease;
    if (dns && !config->dns_ip) config->dns_ip = dns;
}

int leonos_net_config(struct leonos_net_config *config)
{
    if (!config) { errno = EINVAL; return -1; }
    struct leonos_net_control control = {.operation = LEONOS_NET_CONTROL_CONFIG};
    if (net_control(&control) < 0) return -1;
    *config = control.data.config;
    int saved_errno = errno;
    net_merge_dhcp_lease(config);
    errno = saved_errno;
    return 0;
}

int leonos_net_get_dns_policy(struct leonos_net_dns_policy *result)
{
    if (!result) { errno = EINVAL; return -1; }
    struct leonos_net_control control = {.operation = LEONOS_NET_CONTROL_DNS_POLICY,
        .data.dns_policy.mode = LEONOS_NET_DNS_MODE_QUERY};
    if (net_control(&control) < 0) return -1;
    *result = control.data.dns_policy;
    return 0;
}

int leonos_net_set_dns_policy(uint32_t mode, uint32_t custom_dns_ip,
                              struct leonos_net_dns_policy *result)
{
    if (!result || mode > LEONOS_NET_DNS_MODE_CUSTOM) { errno = EINVAL; return -1; }
    if (geteuid()) { errno = EPERM; return -1; }
    char temporary[] = "/etc/udhcpc/.dns.XXXXXX";
    int fd = mkstemp(temporary);
    if (fd < 0) return -1;
    char text[64];
    int size = snprintf(text, sizeof(text), "%u %u.%u.%u.%u\n", mode,
        custom_dns_ip >> 24, (custom_dns_ip >> 16) & 255, (custom_dns_ip >> 8) & 255, custom_dns_ip & 255);
    int ret = write(fd, text, size) == size && fchmod(fd, 0644) == 0 && fsync(fd) == 0 ? 0 : -1;
    if (close(fd) < 0) ret = -1;
    if (!ret) ret = rename(temporary, "/etc/udhcpc/leonos-dns");
    if (ret) { unlink(temporary); return -1; }
    pid_t child = fork();
    if (child < 0) return -1;
    if (!child) { execl("/usr/lib/leonos/publish-resolver", "publish-resolver", (char *)NULL); _exit(127); }
    int status;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) return -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) { errno = EIO; return -1; }
    struct leonos_net_control control = {.operation = LEONOS_NET_CONTROL_DNS_POLICY,
        .data.dns_policy = {.mode = mode, .custom_dns_ip = custom_dns_ip}};
    if (net_control(&control) < 0) return -1;
    *result = control.data.dns_policy;
    return 0;
}

int leonos_net_dhcp_renew(uint32_t timeout_ms, struct leonos_net_dhcp *result)
{
    if (!result) { errno = EINVAL; return -1; }
    *result = (struct leonos_net_dhcp){.timeout_ms = timeout_ms, .status = LEONOS_NET_STATUS_DHCP_FAILED};
    if (leonos_openrc_run("leonos-dhcp", "restart")) { errno = EIO; return -1; }
    struct timespec started, now;
    if (clock_gettime(CLOCK_MONOTONIC, &started)) return -1;
    uint64_t budget = timeout_ms ? timeout_ms : 3000;
    for (;;) {
        if (leonos_net_config(&result->config) < 0) return -1;
        if (result->config.local_ip && result->config.source == LEONOS_NET_CONFIG_SOURCE_DHCP) {
            result->status = LEONOS_NET_STATUS_OK;
            break;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
        int64_t elapsed = (now.tv_sec - started.tv_sec) * 1000 +
            (now.tv_nsec - started.tv_nsec) / 1000000;
        if (elapsed >= (int64_t)budget) {
            result->status = LEONOS_NET_STATUS_DHCP_TIMEOUT;
            break;
        }
        struct timespec interval = {.tv_nsec = 50000000};
        if (nanosleep(&interval, NULL) && errno != EINTR) return -1;
    }
    return 0;
}

int leonos_net_ping(uint32_t target_ip, uint32_t timeout_ms, struct leonos_net_ping *result)
{
    if (!result) { errno = EINVAL; return -1; }
    struct leonos_net_control control = {.operation = LEONOS_NET_CONTROL_PING,
        .data.ping = {.target_ip = target_ip, .timeout_ms = timeout_ms}};
    if (net_control(&control) < 0) return -1;
    *result = control.data.ping;
    return 0;
}

int leonos_net_dns_resolve(const char *name, uint32_t timeout_ms, struct leonos_net_dns *result)
{
    if (!name || !result) { errno = EINVAL; return -1; }
    *result = (struct leonos_net_dns){.timeout_ms = timeout_ms};
    net_copy_text(result->name, sizeof(result->name), name);
    struct addrinfo hint = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *list = NULL;
    int error = getaddrinfo(name, NULL, &hint, &list);
    for (struct addrinfo *entry = list; entry && result->address_count < LEONOS_NET_DNS_MAX_ADDRESSES; entry = entry->ai_next)
        if (entry->ai_family == AF_INET)
            result->addresses[result->address_count++] = ntohl(((struct sockaddr_in *)entry->ai_addr)->sin_addr.s_addr);
    if (list) freeaddrinfo(list);
    result->status = !error && result->address_count ? LEONOS_NET_STATUS_OK : LEONOS_NET_STATUS_DNS_NO_ANSWER;
    return 0;
}

int leonos_net_connections(struct leonos_net_connection_info *entries, uint32_t capacity, uint32_t *out_count)
{
    if (!out_count || (!entries && capacity)) { errno = EINVAL; return -1; }
    struct leonos_net_control control = {.operation = LEONOS_NET_CONTROL_CONNECTIONS};
    if (net_control(&control) < 0) return -1;
    *out_count = control.data.connections.count;
    uint32_t count = *out_count < capacity ? *out_count : capacity;
    if (count) memcpy(entries, control.data.connections.entries, count * sizeof(*entries));
    return 0;
}

static int parse_ipv4_literal(const char *text, uint32_t *network_value)
{
    uint32_t host = 0;
    if (!text || !network_value) return 0;
    for (uint32_t octet = 0; octet < 4u; ++octet) {
        uint32_t value = 0;
        uint32_t digits = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10u + (uint32_t)(*text - '0');
            if (value > 255u) return 0;
            ++text;
            ++digits;
        }
        if (!digits) return 0;
        host = (host << 8) | value;
        if (octet != 3u) {
            if (*text != '.') return 0;
            ++text;
        }
    }
    if (*text) return 0;
    *network_value = ((host & 0xffu) << 24) | ((host & 0xff00u) << 8) |
                     ((host & 0xff0000u) >> 8) | ((host & 0xff000000u) >> 24);
    return 1;
}

int leonos_socket_tcp(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return fd;
}

int leonos_socket_connect(int socket_fd, const char *host, uint32_t port,
                          uint32_t timeout_ms,
                          struct leonos_net_socket_connect *result)
{
    struct sockaddr_in address;
    uint32_t network_ip = 0;
    if (!host || !result || socket_fd < 0) {
        errno = EINVAL;
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->socket = socket_fd;
    result->port = port;
    result->timeout_ms = timeout_ms;
    result->status = LEONOS_NET_STATUS_TCP_FAILED;
    net_copy_text(result->host, sizeof(result->host), host);
    if (!parse_ipv4_literal(host, &network_ip)) {
        struct leonos_net_dns dns;
        uint32_t i = 0;
        if (leonos_net_dns_resolve(host, timeout_ms, &dns) < 0 ||
            dns.status != LEONOS_NET_STATUS_OK || dns.address_count == 0) {
            result->status = dns.status ? dns.status : LEONOS_NET_STATUS_DNS_FAILED;
            return -1;
        }
        network_ip = htonl(dns.addresses[0]);
        (void)i;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = network_ip;
    if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        result->status = LEONOS_NET_STATUS_TCP_FAILED;
        return -1;
    }
    result->status = LEONOS_NET_STATUS_OK;
    result->remote_ip = ntohl(network_ip);
    return 0;
}

long leonos_socket_send(int socket_fd, const void *buffer, uint32_t length,
                        uint32_t timeout_ms, uint32_t *status)
{
    ssize_t sent;
    (void)timeout_ms;
    if (status) *status = LEONOS_NET_STATUS_TCP_FAILED;
    if (length && !buffer) {
        if (status) *status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return -1;
    }
    sent = send(socket_fd, buffer, length, 0);
    if (sent < 0) {
        if (status) *status = LEONOS_NET_STATUS_TCP_FAILED;
        return -1;
    }
    if (status) *status = LEONOS_NET_STATUS_OK;
    return (long)sent;
}

long leonos_socket_recv(int socket_fd, void *buffer, uint32_t length,
                        uint32_t timeout_ms, uint32_t *status)
{
    ssize_t got;
    (void)timeout_ms;
    if (status) *status = LEONOS_NET_STATUS_TCP_FAILED;
    if (length && !buffer) {
        if (status) *status = LEONOS_NET_STATUS_BAD_ARGUMENT;
        return -1;
    }
    got = recv(socket_fd, buffer, length, 0);
    if (got < 0) {
        if (status) *status = LEONOS_NET_STATUS_TCP_FAILED;
        return -1;
    }
    if (status) *status = LEONOS_NET_STATUS_OK;
    return (long)got;
}

int leonos_socket_close(int socket_fd)
{
    return close(socket_fd);
}

int leonos_net_http_get(const char *host, const char *path, uint32_t port,
                        uint32_t timeout_ms, struct leonos_net_http_get *result)
{
    char url[LEONOS_NET_HOSTNAME_LEN + LEONOS_NET_HTTP_PATH_LEN + 16];
    struct leonos_http_response response;
    memset(url, 0, sizeof(url));
    (void)snprintf(url, sizeof(url), "http://%s:%u%s", host, port ? port : 80,
                   path && path[0] ? path : "/");
    memset(&response, 0, sizeof(response));
    if (leonos_http_get(url, timeout_ms, result->response,
                        sizeof(result->response), 0, 0, &response) < 0) {
        return -1;
    }
    result->status = response.net_status;
    result->http_status = response.http_status;
    result->response_len = response.body_len < sizeof(result->response)
                               ? response.body_len
                               : sizeof(result->response) - 1u;
    result->response[result->response_len] = 0;
    return response.net_status == LEONOS_NET_STATUS_OK ? 0 : -1;
}
