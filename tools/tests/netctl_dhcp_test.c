#include <assert.h>
#include <unistd.h>
static uid_t caller_uid;
static uid_t test_geteuid(void) { return caller_uid; }
#define geteuid test_geteuid
#define main netctl_main
#include "../../userland/apps/netctl/main.c"
#undef main
#undef geteuid

static int renewals, authorizations, worker_pending, worker_status, service_error;
static uint32_t service_status;
int net_service_config(net_service_config_t *result)
{ *result = (net_service_config_t){.local_ip = 0xc0a8a581}; return 0; }
int net_service_get_dns_policy(net_service_dns_policy_t *result)
{ *result = (net_service_dns_policy_t){.status = 0, .mode = NET_SERVICE_DNS_MODE_DHCP}; return 0; }
int net_service_connections(net_service_connection_info_t *entries, uint32_t cap, uint32_t *count)
{ (void)entries; (void)cap; *count = 0; return 0; }
void leonos_ui_listview_state_set_count(struct leonos_ui_listview_state *view, uint32_t count)
{ (void)view; (void)count; }
int leonos_sudo_run(const char *user, const char *password, char *const args[], uint32_t *pid)
{
    assert(!user && !password);
    assert(!strcmp(args[0], "/usr/lib/leonos/apps/netctl/netctl.elf"));
    assert(!strcmp(args[1], "--renew-dhcp") && !args[2]);
    ++authorizations;
    *pid = 123;
    return 0;
}
int leonos_sudo_wait(uint32_t pid, int *status)
{
    assert(pid == 123);
    if (worker_pending) { errno = EAGAIN; return -1; }
    *status = worker_status;
    return 0;
}

int main(void)
{
    caller_uid = 1000;
    assert(renew_dhcp_command() == 128 + EACCES && renewals == 0);
    renew_dhcp();
    assert(authorizations == 1 && renewals == 0 && dhcp_child == 123);
    renew_dhcp();
    assert(authorizations == 1);
    worker_pending = 1;
    assert(poll_dhcp_command() == 0 && dhcp_child == 123);
    worker_pending = 0;
    worker_status = 1 << 8;
    assert(poll_dhcp_command() == 1 && !dhcp_child && renewals == 0);
    assert(strstr(status_text, "denied") != NULL);
    caller_uid = 0;
    renew_dhcp();
    assert(renewals == 1 && authorizations == 1);
    service_error = EIO;
    assert(renew_dhcp_command() == 128 + EIO);
    service_error = 0;
    service_status = NET_SERVICE_STATUS_DHCP_TIMEOUT;
    assert(renew_dhcp_command() == 64 + NET_SERVICE_STATUS_DHCP_TIMEOUT);
    service_status = 0;
    assert(renew_dhcp_command() == 0);
    caller_uid = 1000;
    renew_dhcp();
    worker_status = 0;
    assert(poll_dhcp_command() == 1 && !dhcp_child && config.local_ip == 0xc0a8a581);
    puts("PASS netctl DHCP: root execution, sudo delegation, cancellation, pending state and actual error/result handling");
}
