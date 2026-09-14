#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <leonos/net_control.h>

static int kernel_error;
static int control_ioctl(int fd, unsigned long command, ...);
#define ioctl control_ioctl
#include "../../userland/apps/serviced/netmand.c"
#undef ioctl
#include "../../userland/libc/src/netsock.c"

/* Host Linux has no LeonOS management ioctl. Keep the actual IPC, permission
 * checks and client code, with a deterministic kernel boundary underneath. */
static int control_ioctl(int fd, unsigned long command, ...)
{
    assert(fd >= 0 && command == LEONOS_NET_CONTROL_IOCTL);
    va_list args;
    va_start(args, command);
    struct leonos_net_control *request = va_arg(args, void *);
    va_end(args);
    if (kernel_error) { errno = kernel_error; return -1; }
    request->result = 0;
    if (request->operation == LEONOS_NET_CONTROL_CONFIG)
        request->data.config = (struct leonos_net_config){.local_ip = 0xc0a8a581};
    else if (request->operation == LEONOS_NET_CONTROL_DHCP)
        request->data.dhcp = (struct leonos_net_dhcp){.status = LEONOS_NET_STATUS_OK,
            .config = {.local_ip = 0xc0a8a581}};
    else if (request->operation == LEONOS_NET_CONTROL_DNS_POLICY)
        request->data.dns_policy.status = LEONOS_NET_STATUS_OK;
    else abort();
    return 0;
}

static pid_t server(unsigned uid, int error, unsigned count)
{
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, pair) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (!pid) {
        close(pair[0]);
        kernel_error = error;
        struct netmand_client client = {.fd = pair[1], .pid = getppid(), .uid = uid};
        while (count--) {
            struct pollfd ready = {.fd = client.fd, .events = POLLIN};
            if (poll(&ready, 1, 3000) <= 0) _exit(2);
            handle_client(&client);
            if (client.fd < 0) _exit(3);
        }
        leonos_ipc_close(client.fd);
        if (control_fd >= 0) close(control_fd);
        _exit(0);
    }
    close(pair[1]);
    netmand_fd = pair[0];
    return pid;
}

static void finish(pid_t child)
{
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    if (netmand_fd >= 0) leonos_ipc_close(netmand_fd);
    netmand_fd = -1;
    netmand_retry_after_ms = 0;
}

int main(void)
{
    alarm(15);
    struct leonos_net_dhcp dhcp;
    struct leonos_net_config config;
    struct leonos_net_dns_policy policy;
    pid_t child = server(1000, 0, 3);
    assert(leonos_net_dhcp_renew(4000, &dhcp) == -1);
    assert(errno == EACCES);
    assert(leonos_net_config(&config) == 0 && config.local_ip == 0xc0a8a581);
    assert(leonos_net_set_dns_policy(LEONOS_NET_DNS_MODE_DHCP, 0, &policy) == -1 && errno == EACCES);
    finish(child);
    child = server(0, 0, 1);
    assert(leonos_net_dhcp_renew(4000, &dhcp) == 0 && dhcp.status == 0 && dhcp.config.local_ip == 0xc0a8a581);
    finish(child);
    child = server(0, EPERM, 1);
    assert(leonos_net_dhcp_renew(4000, &dhcp) == -1 && errno == EPERM);
    finish(child);
    puts("PASS netmand: denied mutations, subsequent read, root renew and kernel errno propagation");
}
