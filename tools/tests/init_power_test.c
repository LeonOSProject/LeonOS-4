#define _GNU_SOURCE
#include <assert.h>
#include <signal.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include "../../userland/runtime/src/auth_accounts.c"
static int last_signal, sends;
uid_t geteuid(void) { return 0; }
int kill(pid_t pid, int sig) { assert(pid == 1); last_signal = sig; ++sends; return 0; }
int reboot(int command) { (void)command; assert(!"GUI must signal PID 1, never reboot directly"); return -1; }
int leonos_sudo_run(const char *user, const char *password, char *const args[], uint32_t *pid)
{ (void)user; (void)password; (void)args; (void)pid; assert(0); return -1; }
int leonos_sudo_wait_command(uint32_t pid, int *status) { (void)pid; (void)status; assert(0); return -1; }
int main(void)
{
    assert(leonos_auth_request_power(RB_AUTOBOOT) == 0 && last_signal == SIGTERM && sends == 1);
    assert(leonos_auth_request_power(RB_POWER_OFF) == 0 && last_signal == SIGUSR2 && sends == 2);
    assert(leonos_auth_request_power(123) == -1 && errno == EINVAL && sends == 2);
    puts("PASS GUI/installer power requests follow BusyBox PID 1 protocol");
}
