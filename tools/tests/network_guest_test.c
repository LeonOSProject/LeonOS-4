#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <leonos/http.h>
#include <leonos/net_control.h>
#include <leonos/system.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;
#define CHECK(test, text) do { int ok = !!(test); printf("[network] %s %s (errno=%d)\n", ok ? "PASS" : "FAIL", text, errno); if (!ok) ++failures; } while (0)

static int run(char *const command[])
{
    pid_t child = fork();
    if (child == 0) { execv(command[0], command); _exit(127); }
    int status;
    if (child < 0 || waitpid(child, &status, 0) != child) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int unprivileged_management(void)
{
    if (setgroups(0, NULL) < 0 || setgid(1000) < 0 || setuid(1000) < 0) return 1;
    struct leonos_net_dhcp dhcp;
    struct leonos_net_config config;
    CHECK(leonos_net_config(&config) == 0 && config.local_ip == 0x0a25000f,
          "ordinary user reads network service");
    CHECK(leonos_net_dhcp_renew(4000, &dhcp) < 0 && errno == EACCES,
          "ordinary user DHCP requires authorization");
    CHECK(leonos_net_config(&config) == 0 && config.local_ip == 0x0a25000f,
          "network service remains usable after denied update");
    char *direct[] = {"/usr/lib/leonos/apps/netctl/netctl.elf", "--renew-dhcp", NULL};
    CHECK(run(direct) == 128 + EACCES, "DHCP helper rejects direct unprivileged execution");
    char *authorized[] = {"/usr/bin/sudo", "-n", "--", "/usr/lib/leonos/apps/netctl/netctl.elf",
                          "--renew-dhcp", NULL};
    CHECK(run(authorized) == 0, "upstream sudo authorizes actual DHCP worker");
    return failures != 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--netmand-unprivileged")) return unprivileged_management();
    puts("[network] START");
    alarm(100);
    int control = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    struct leonos_net_control request = {.version = LEONOS_NET_CONTROL_VERSION,
                                        .operation = LEONOS_NET_CONTROL_CONFIG};
    CHECK(control >= 0 && ioctl(control, LEONOS_NET_CONTROL_IOCTL, &request) == 0 && !request.result,
          "real interface query");
    struct leonos_net_config *config = &request.data.config;
    printf("[network] address=%08x gateway=%08x dns=%08x flags=%x\n", config->local_ip,
           config->gateway_ip, config->dns_ip, config->flags);
    CHECK(config->local_ip == 0x0a25000f && config->gateway_ip == 0x0a250002 &&
          config->dns_ip == 0x0a250003, "DHCP custom subnet and DNS");
    close(control);
    struct leonos_net_config managed;
    int management = -1;
    for (unsigned i = 0; i < 20; ++i) {
        management = leonos_net_config(&managed);
        if (!management) break;
        usleep(500000);
    }
    CHECK(management == 0 && managed.local_ip == 0x0a25000f, "production netmand configuration IPC");
    struct leonos_net_dhcp renewed;
    CHECK(leonos_net_dhcp_renew(4000, &renewed) == 0 && renewed.status == 0 &&
          renewed.config.local_ip == 0x0a25000f, "root DHCP update through production service");
    char *ordinary[] = {"/usr/lib/leonos/tests/linux-inventory.elf", "--netmand-unprivileged", NULL};
    CHECK(run(ordinary) == 0, "network controller authorization workflow");
    int datagrams[2] = {socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0), socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)};
    struct sockaddr_in loop = {.sin_family=AF_INET, .sin_port=htons(53000), .sin_addr={.s_addr=htonl(0x7f000001)}};
    CHECK(bind(datagrams[1], (void *)&loop, sizeof(loop)) == 0, "UDP loopback bind");
    struct iovec outgoing[2] = {{"abc", 3}, {"def", 3}};
    struct msghdr message = {.msg_name=&loop, .msg_namelen=sizeof(loop), .msg_iov=outgoing, .msg_iovlen=2};
    CHECK(sendmsg(datagrams[0], &message, 0) == 6, "UDP sendmsg gathers one datagram");
    char first[2], second[2];
    struct iovec incoming[2] = {{first, 2}, {second, 2}};
    struct sockaddr_in from;
    message = (struct msghdr){.msg_name=&from, .msg_namelen=sizeof(from), .msg_iov=incoming, .msg_iovlen=2};
    CHECK(recvmsg(datagrams[1], &message, 0) == 4 && message.msg_flags == MSG_TRUNC &&
          !memcmp(first, "ab", 2) && !memcmp(second, "cd", 2) && from.sin_family == AF_INET,
          "UDP recvmsg source, scatter and MSG_TRUNC");
    close(datagrams[0]); close(datagrams[1]);
    /* Wait for serviced to publish the actual DHCP resolver. */
    for (unsigned i = 0; i < 100; ++i) {
        FILE *file = fopen("/etc/resolv.conf", "r");
        char text[256] = {0};
        if (file) { fread(text, 1, sizeof(text)-1, file); fclose(file); }
        if (strstr(text, "10.37.0.3")) break;
        usleep(100000);
    }
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *addresses = NULL;
    int ret = getaddrinfo("fixture.test", "18080", &hints, &addresses);
    CHECK(!ret && addresses && ((struct sockaddr_in *)addresses->ai_addr)->sin_addr.s_addr == htonl(0x0a250002),
          "unmodified musl DNS over UDP");
    if (addresses) {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        ret = connect(fd, addresses->ai_addr, addresses->ai_addrlen);
        CHECK(ret == 0 || (ret < 0 && errno == EINPROGRESS), "nonblocking connect result");
        struct pollfd p = {.fd=fd, .events=POLLOUT};
        int error = -1;
        socklen_t length = sizeof(error);
        CHECK(poll(&p, 1, 15000) > 0 && (p.revents & POLLOUT) &&
              getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0 && length == sizeof(error),
              "poll and SO_ERROR handshake completion");
        CHECK(fcntl(fd, F_SETFL, 0) == 0, "TCP blocking flag change");
        pid_t child = fork();
        if (!child) {
            const char request[] = "GET /payload HTTP/1.0\r\nHost: fixture.test\r\n\r\n";
            char byte;
            usleep(100000);
            int ok = write(fd, request, sizeof(request)-1) == sizeof(request)-1 && read(fd, &byte, 1) == 1 && byte == 'H';
            close(fd);
            _exit(ok ? 0 : 1);
        }
        close(fd);
        int child_status = -1;
        CHECK(child > 0 && waitpid(child, &child_status, 0) == child && WIFEXITED(child_status) && !WEXITSTATUS(child_status),
              "inherited TCP descriptor survives parent close");
        freeaddrinfo(addresses);
    }
    struct leonos_time_sync sync;
    ret = leonos_time_ntp_sync(4000, &sync);
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    CHECK(ret == 0 && sync.valid && sync.status == LEONOS_NET_STATUS_OK &&
          now.tv_sec >= 2208988800LL && now.tv_sec < 2208988900LL,
          "NTP era 2040 and actual CLOCK_REALTIME update");
    printf("[network] clock=%lld.%09ld ntp-status=%u\n", (long long)now.tv_sec, now.tv_nsec, sync.status);
    char *wget[] = {"/bin/busybox", "wget", "-O", "/tmp/net-download", "http://fixture.test:18080/payload", NULL};
    CHECK(run(wget) == 0, "official BusyBox HTTP download");
    FILE *download = fopen("/tmp/net-download", "rb");
    unsigned count = 0;
    int byte, valid = !!download;
    if (download) {
        while ((byte = fgetc(download)) != EOF) { if (byte != (int)(count % 251)) valid = 0; ++count; }
        if (ferror(download)) valid = 0;
        fclose(download);
    }
    CHECK(valid && count == 1024*1024, "one MiB download exact content");
    char *python[] = {"/usr/bin/python3", "/tmp/net-https.py", NULL};
    CHECK(run(python) == 0, "unmodified Python HTTPS and certificate verification");
    {
        const size_t expected_size = 1024U * 1024U;
        char *body = malloc(expected_size + 1U);
        struct leonos_http_response https_response = {0};
        int native_ok = 0;
        if (body) {
            int request_result = leonos_http_get(
                "https://fixture.test:18443/payload", 20000U, body,
                (uint32_t)(expected_size + 1U), NULL, 0, &https_response);
            native_ok = request_result == 0 &&
                        https_response.net_status == LEONOS_NET_STATUS_OK &&
                        https_response.http_status == 200U &&
                        https_response.body_len == expected_size;
            for (size_t i = 0; native_ok && i < expected_size; ++i) {
                if ((unsigned char)body[i] != (unsigned char)(i % 251U)) {
                    native_ok = 0;
                }
            }
        }
        CHECK(native_ok, "native libc HTTPS GET verifies CA and exact payload");
        free(body);

        struct leonos_http_response download_response = {0};
        int download_result = leonos_http_download(
            "https://fixture.test:18443/payload", "/tmp/net-native-download",
            20000U, NULL, NULL, &download_response);
        FILE *native_download = fopen("/tmp/net-native-download", "rb");
        size_t downloaded = 0;
        int download_ok = download_result == 0 && native_download != NULL;
        if (native_download) {
            int byte_value;
            while ((byte_value = fgetc(native_download)) != EOF) {
                if (byte_value != (int)(downloaded % 251U)) download_ok = 0;
                ++downloaded;
            }
            if (ferror(native_download)) download_ok = 0;
            fclose(native_download);
        }
        CHECK(download_ok && downloaded == expected_size,
              "native HTTPS streaming download preserves exact payload");

        struct leonos_http_response mismatch = {0};
        char small_body[256];
        int mismatch_result = leonos_http_get(
            "https://10.37.0.2:18443/payload", 20000U, small_body,
            sizeof(small_body), NULL, 0, &mismatch);
        CHECK(mismatch_result == 0 &&
              mismatch.net_status == LEONOS_NET_STATUS_TLS_FAILED,
              "native HTTPS rejects certificate hostname mismatch");

        struct leonos_http_response downgrade = {0};
        int downgrade_result = leonos_http_get(
            "https://fixture.test:18443/downgrade", 20000U, small_body,
            sizeof(small_body), NULL, 0, &downgrade);
        CHECK(downgrade_result == 0 &&
              downgrade.net_status == LEONOS_NET_STATUS_TLS_FAILED,
              "native HTTPS rejects cleartext downgrade redirect");
    }
    control = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    puts("[network] LINK_DOWN_REQUEST");
    for (unsigned i = 0; i < 70; ++i) {
        request = (struct leonos_net_control){.version=LEONOS_NET_CONTROL_VERSION, .operation=LEONOS_NET_CONTROL_CONFIG};
        if (ioctl(control, LEONOS_NET_CONTROL_IOCTL, &request) < 0) break;
        if (!(request.data.config.flags & LEONOS_NET_CONFIG_FLAG_ACTIVE)) break;
        usleep(100000);
    }
    CHECK((request.data.config.flags & (LEONOS_NET_CONFIG_FLAG_ACTIVE | LEONOS_NET_CONFIG_FLAG_PRESENT)) ==
          LEONOS_NET_CONFIG_FLAG_PRESENT, "disconnected link remains present but inactive");
    puts("[network] LINK_UP_REQUEST");
    for (unsigned i = 0; i < 70; ++i) {
        if (ioctl(control, LEONOS_NET_CONTROL_IOCTL, &request) < 0) break;
        if (request.data.config.flags & LEONOS_NET_CONFIG_FLAG_ACTIVE) break;
        usleep(100000);
    }
    CHECK(request.data.config.flags & LEONOS_NET_CONFIG_FLAG_ACTIVE, "carrier recovers after reconnect");
    request = (struct leonos_net_control){.version=LEONOS_NET_CONTROL_VERSION, .operation=LEONOS_NET_CONTROL_DHCP,
        .data.dhcp={.timeout_ms=3000}};
    CHECK(ioctl(control, LEONOS_NET_CONTROL_IOCTL, &request) == 0 && !request.result &&
        !request.data.dhcp.status && request.data.dhcp.config.local_ip == 0x0a25000f, "DHCP reacquires after reconnect");
    close(control);
    alarm(0);
    printf("[network] DONE failures=%d\n", failures);
    return failures != 0;
}
