#define _POSIX_C_SOURCE 200809L
#include <leonos/system.h>
#include <leonos/net.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ntp_protocol.h"

static int64_t ntp_now_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
    return now.tv_sec * 1000000000LL + now.tv_nsec;
}

int leonos_time_ntp_sync(uint32_t timeout_ms, struct leonos_time_sync *result)
{
    if (!result) { errno = EINVAL; return -1; }
    memset(result, 0, sizeof(*result));
    result->timeout_ms = timeout_ms ? timeout_ms : 4000;
    if (result->timeout_ms > 10000) result->timeout_ms = 10000;
    snprintf(result->server, sizeof(result->server), "%s", "pool.ntp.org");
    FILE *config = fopen("/etc/ntp.conf", "re");
    if (config) {
        char line[256], hostname[128];
        while (fgets(line, sizeof(line), config)) {
            if (sscanf(line, " server %127s", hostname) == 1 || sscanf(line, " pool %127s", hostname) == 1) {
                snprintf(result->server, sizeof(result->server), "%s", hostname);
                break;
            }
        }
        fclose(config);
    }
    struct addrinfo hint = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM}, *list = NULL;
    result->status = LEONOS_NET_STATUS_DNS_FAILED;
    if (getaddrinfo(result->server, "123", &hint, &list)) return 0;
    int64_t now = ntp_now_ns();
    if (now < 0) { freeaddrinfo(list); return -1; }
    int64_t deadline = now + (int64_t)result->timeout_ms * 1000000;
    for (struct addrinfo *peer = list; peer && ntp_now_ns() < deadline; peer = peer->ai_next) {
        int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) continue;
        if (connect(fd, peer->ai_addr, peer->ai_addrlen)) { close(fd); continue; }
        unsigned char request[48] = {0}, response[512];
        struct timespec wall;
        if (clock_gettime(CLOCK_REALTIME, &wall)) { close(fd); continue; }
        request[0] = 0x23;
        ntp_put32(request + 40, (uint32_t)(wall.tv_sec + 2208988800LL));
        uint32_t random;
        if (getrandom(&random, sizeof(random), 0) != sizeof(random)) { close(fd); continue; }
        ntp_put32(request + 44, random);
        int64_t start = ntp_now_ns();
        if (send(fd, request, sizeof(request), 0) != sizeof(request)) { close(fd); continue; }
        result->status = LEONOS_NET_STATUS_NTP_TIMEOUT;
        while (ntp_now_ns() < deadline) {
            int64_t remaining = deadline - ntp_now_ns();
            struct pollfd waiter = {.fd = fd, .events = POLLIN};
            int ready = poll(&waiter, 1, remaining > 0 ? (remaining + 999999) / 1000000 : 0);
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) break;
            ssize_t size = recv(fd, response, sizeof(response), MSG_DONTWAIT);
            if (size < 0) {
                if (errno == EINTR || (errno == EAGAIN && !(waiter.revents & (POLLERR | POLLHUP)))) continue;
                break;
            }
            if (size < 48) { result->status = LEONOS_NET_STATUS_NTP_INVALID; continue; }
            struct timespec corrected;
            if (ntp_decode(response, size, request, wall.tv_sec, ntp_now_ns() - start, &corrected)) {
                result->status = LEONOS_NET_STATUS_NTP_INVALID;
                continue;
            }
            int ret = clock_settime(CLOCK_REALTIME, &corrected);
            int saved = errno;
            close(fd);
            result->server_ip = ntohl(((struct sockaddr_in *)peer->ai_addr)->sin_addr.s_addr);
            freeaddrinfo(list);
            if (ret) { errno = saved; return -1; }
            result->unix_seconds = corrected.tv_sec;
            result->valid = 1;
            result->status = LEONOS_NET_STATUS_OK;
            return 0;
        }
        close(fd);
    }
    freeaddrinfo(list);
    return 0;
}
