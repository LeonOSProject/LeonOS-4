/* Management IPC uses SO_PEERCRED; ordinary network traffic uses sockets. */
#include <errno.h>
#include <fcntl.h>
#include <leonos/net_control.h>
#include <leonos/netmand.h>
#include <leonos/unix_ipc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "netmand.h"

#define NETMAND_MAX_CLIENTS 16u
#define NETMAND_FRAME_CAP 4096u
struct netmand_client { int fd; uint32_t pid, uid; };
static struct netmand_client clients[NETMAND_MAX_CLIENTS];
static int listen_fd = -1, control_fd = -1;
static uint32_t published_dns;

static int control_request(struct leonos_net_control *request)
{
    if (control_fd < 0) control_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (control_fd < 0) return -1;
    request->version = LEONOS_NET_CONTROL_VERSION;
    if (ioctl(control_fd, LEONOS_NET_CONTROL_IOCTL, request) < 0) return -1;
    if (request->result < 0) { errno = -request->result; return -1; }
    return 0;
}

static int publish_dns(uint32_t ip)
{
    if (ip == published_dns) return 0;
    char temporary[] = "/etc/.resolv.conf.XXXXXX";
    int fd = mkstemp(temporary);
    if (fd < 0) return -1;
    char text[96];
    int size = ip ? snprintf(text, sizeof(text), "nameserver %u.%u.%u.%u\noptions timeout:2 attempts:2\n",
                         ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255) :
        snprintf(text, sizeof(text), "# No DHCP DNS lease\noptions timeout:2 attempts:2\n");
    int ret = 0;
    for (int done = 0; done < size;) {
        ssize_t wrote = write(fd, text + done, size - done);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) { ret = -1; break; }
        done += wrote;
    }
    if (!ret && fchmod(fd, 0644) < 0) ret = -1;
    if (!ret && fsync(fd) < 0) ret = -1;
    if (close(fd) < 0) ret = -1;
    if (!ret && rename(temporary, "/etc/resolv.conf") < 0) ret = -1;
    if (ret) unlink(temporary);
    else published_dns = ip;
    return ret;
}

int netmand_config(struct leonos_net_config *config)
{
    struct leonos_net_control request = {.operation = LEONOS_NET_CONTROL_CONFIG};
    if (control_request(&request) < 0) return -1;
    *config = request.data.config;
    return publish_dns(config->dns_ip);
}

int netmand_dhcp(uint32_t timeout, struct leonos_net_dhcp *result)
{
    struct leonos_net_control request = {.operation = LEONOS_NET_CONTROL_DHCP,
        .data.dhcp = {.timeout_ms = timeout}};
    *result = (struct leonos_net_dhcp){.status = LEONOS_NET_STATUS_DHCP_FAILED};
    if (control_request(&request) < 0) return -1;
    *result = request.data.dhcp;
    return result->status == LEONOS_NET_STATUS_OK ? publish_dns(result->config.dns_ip) : 0;
}

static int save_policy(const struct leonos_net_dns_policy *policy)
{
    FILE *file = fopen("/etc/leonos/network.conf", "we");
    if (!file) return -1;
    const char *mode = policy->mode == LEONOS_NET_DNS_MODE_DHCP ? "dhcp" :
                       policy->mode == LEONOS_NET_DNS_MODE_CUSTOM ? "custom" : "cloudflare";
    uint32_t ip = policy->custom_dns_ip;
    int ret = fprintf(file, "dns_mode=%s\ndns_custom=%u.%u.%u.%u\n", mode,
                       ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255) < 0 ? -1 : 0;
    if (fclose(file)) ret = -1;
    return ret;
}

static void disconnect(struct netmand_client *client)
{ leonos_ipc_close(client->fd); client->fd = -1; }

static void reply_error(struct netmand_client *client, uint32_t type, int error)
{
    struct leonos_netmand_error response = {.request_type = type, .error = error > 0 ? error : EIO};
    fprintf(stderr, "[netmand] request=%u pid=%u uid=%u failed errno=%d\n",
            type, client->pid, client->uid, response.error);
    if (leonos_ipc_send(client->fd, LEONOS_NET_MSG_ERROR, &response, sizeof(response)) < 0)
        disconnect(client);
}

static void handle_client(struct netmand_client *client)
{
    uint8_t buffer[NETMAND_FRAME_CAP];
    uint32_t type = 0, length = 0;
    struct pollfd descriptor = {.fd = client->fd, .events = POLLIN};
    if (poll(&descriptor, 1, 0) <= 0) return;
    if (leonos_ipc_recv(client->fd, &type, buffer, sizeof(buffer), &length) < 0) {
        if (errno != EAGAIN) disconnect(client);
        return;
    }
    struct leonos_net_control control = {0};
    int ret = 0;
    if (type == LEONOS_NET_MSG_HELLO) {
        struct leonos_netmand_hello hello;
        if (length != sizeof(hello)) { disconnect(client); return; }
        memcpy(&hello, buffer, sizeof(hello));
        if (hello.pid != client->pid) { disconnect(client); return; }
        struct leonos_netmand_ack ack = {.code = 1};
        ret = leonos_ipc_send(client->fd, LEONOS_NET_MSG_ACK, &ack, sizeof(ack));
    } else if (type == LEONOS_NET_MSG_CONFIG) {
        struct leonos_net_config config;
        if (netmand_config(&config) < 0) { reply_error(client, type, errno); return; }
        ret = leonos_ipc_send(client->fd, type, &config, sizeof(config));
    } else if (type == LEONOS_NET_MSG_DNS_POLICY) {
        if (length != sizeof(control.data.dns_policy)) { disconnect(client); return; }
        control.operation = LEONOS_NET_CONTROL_DNS_POLICY;
        memcpy(&control.data.dns_policy, buffer, length);
        bool change = control.data.dns_policy.mode != LEONOS_NET_DNS_MODE_QUERY;
        if (change && client->uid) { reply_error(client, type, EACCES); return; }
        if (control_request(&control) < 0 ||
            (change && control.data.dns_policy.status == LEONOS_NET_STATUS_OK &&
             (save_policy(&control.data.dns_policy) < 0 || publish_dns(control.data.dns_policy.config.dns_ip) < 0))) {
            reply_error(client, type, errno); return;
        }
        ret = leonos_ipc_send(client->fd, type, &control.data.dns_policy, sizeof(control.data.dns_policy));
    } else if (type == LEONOS_NET_MSG_DHCP) {
        struct leonos_net_dhcp request;
        if (length != sizeof(request)) { disconnect(client); return; }
        if (client->uid) { reply_error(client, type, EACCES); return; }
        memcpy(&request, buffer, length);
        if (netmand_dhcp(request.timeout_ms, &request) < 0) { reply_error(client, type, errno); return; }
        ret = leonos_ipc_send(client->fd, type, &request, sizeof(request));
    } else if (type == LEONOS_NET_MSG_PING) {
        if (length != sizeof(control.data.ping)) { disconnect(client); return; }
        control.operation = LEONOS_NET_CONTROL_PING;
        memcpy(&control.data.ping, buffer, length);
        if (control_request(&control) < 0) { reply_error(client, type, errno); return; }
        ret = leonos_ipc_send(client->fd, type, &control.data.ping, sizeof(control.data.ping));
    } else if (type == LEONOS_NET_MSG_DNS) {
        struct leonos_net_dns request;
        if (length != sizeof(request)) { disconnect(client); return; }
        memcpy(&request, buffer, length);
        if (!memchr(request.name, 0, sizeof(request.name))) { disconnect(client); return; }
        struct addrinfo hint = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *list = NULL;
        int error = getaddrinfo(request.name, NULL, &hint, &list);
        request.address_count = 0;
        memset(request.addresses, 0, sizeof(request.addresses));
        for (struct addrinfo *entry = list; entry && request.address_count < LEONOS_NET_DNS_MAX_ADDRESSES;
             entry = entry->ai_next) {
            if (entry->ai_family != AF_INET) continue;
            request.addresses[request.address_count++] = ntohl(((struct sockaddr_in *)entry->ai_addr)->sin_addr.s_addr);
        }
        if (list) freeaddrinfo(list);
        request.status = !error && request.address_count ? LEONOS_NET_STATUS_OK : LEONOS_NET_STATUS_DNS_NO_ANSWER;
        ret = leonos_ipc_send(client->fd, type, &request, sizeof(request));
    } else if (type == LEONOS_NET_MSG_CONNECTIONS) {
        control.operation = LEONOS_NET_CONTROL_CONNECTIONS;
        if (control_request(&control) < 0) { reply_error(client, type, errno); return; }
        struct {
            struct leonos_netmand_connections_ack ack;
            struct leonos_net_connection_info entries[LEONOS_NET_SOCKET_MAX];
        } response = {0};
        for (unsigned i = 0; i < control.data.connections.count && i < LEONOS_NET_SOCKET_MAX; ++i) {
            struct leonos_net_connection_info *entry = &control.data.connections.entries[i];
            if (!client->uid || entry->owner_pid == client->pid) response.entries[response.ack.count++] = *entry;
        }
        ret = leonos_ipc_send(client->fd, type, &response,
                              sizeof(response.ack) + response.ack.count * sizeof(response.entries[0]));
    } else { disconnect(client); return; }
    if (ret < 0) disconnect(client);
}

void netmand_poll(void)
{
    if (listen_fd < 0) {
        static int last_error;
        listen_fd = leonos_ipc_bind_listen_mode(LEONOS_IPC_SOCK_NET, 8, 0666);
        if (listen_fd < 0) {
            if (errno != last_error) {
                last_error = errno;
                fprintf(stderr, "[netmand] listen %s failed errno=%d\n", LEONOS_IPC_SOCK_NET, last_error);
            }
            return;
        }
        last_error = 0;
        for (unsigned i = 0; i < NETMAND_MAX_CLIENTS; ++i) clients[i].fd = -1;
        (void)leonos_ipc_set_nonblock(listen_fd, 1);
        fprintf(stderr, "[netmand] listening on %s\n", LEONOS_IPC_SOCK_NET);
    }
    struct pollfd descriptor = {.fd = listen_fd, .events = POLLIN};
    if (poll(&descriptor, 1, 0) > 0 && (descriptor.revents & POLLIN)) {
        int fd;
        while ((fd = leonos_ipc_accept(listen_fd, 0)) >= 0) {
            struct ucred credentials;
            unsigned slot = 0;
            while (slot < NETMAND_MAX_CLIENTS && clients[slot].fd >= 0) ++slot;
            if (slot == NETMAND_MAX_CLIENTS || leonos_ipc_peer_credentials(fd, &credentials) < 0) { close(fd); continue; }
            (void)leonos_ipc_set_nonblock(fd, 1);
            clients[slot] = (struct netmand_client){fd, (uint32_t)credentials.pid, credentials.uid};
        }
    }
    for (unsigned i = 0; i < NETMAND_MAX_CLIENTS; ++i) if (clients[i].fd >= 0) handle_client(&clients[i]);
}
