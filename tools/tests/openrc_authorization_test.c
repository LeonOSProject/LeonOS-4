#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uid_t caller_uid, caller_euid;
static int calls;
static char command[256];
static uid_t test_getuid(void) { return caller_uid; }
static uid_t test_geteuid(void) { return caller_euid; }
static int test_execl(const char *path, const char *arg, ...)
{
    assert(!getenv("LD_PRELOAD") && !getenv("RC_SVCNAME"));
    assert(!strcmp(getenv("PATH"), "/usr/sbin:/usr/bin:/sbin:/bin"));
    ++calls;
    strcpy(command, path);
    va_list ap; va_start(ap, arg);
    while (arg) {
        assert(strlen(command) + strlen(arg) + 2 < sizeof(command));
        strcat(command, " "); strcat(command, arg);
        arg = va_arg(ap, const char *);
    }
    va_end(ap);
    return -1;
}
#define getuid test_getuid
#define geteuid test_geteuid
#define execl test_execl
#define main rcctl_main
#include "../../userland/apps/rcctl/main.c"
#undef main
static int invoke(const char *service, const char *action)
{
    char *args[] = {"rcctl", (char *)service, (char *)action, NULL};
    calls = 0; command[0] = 0;
    setenv("LD_PRELOAD", "/tmp/evil.so", 1);
    setenv("RC_SVCNAME", "evil", 1);
    return rcctl_main(3, args);
}
int main(void)
{
    caller_uid = caller_euid = 1000;
    assert(invoke("leonos-dhcp", "restart") == 1 && !calls);
    caller_euid = 0;
    assert(invoke("leonos-dhcp", "restart") == 1 && !calls);
    caller_uid = caller_euid = 0;
    assert(invoke("../../tmp/evil", "start") == 2 && !calls);
    assert(invoke("leonos-dhcp;id", "start") == 2 && !calls);
    assert(invoke("leonos-dhcp", "start;id") == 2 && !calls);
    assert(invoke("leonos-dhcp", "enable") == 127 && calls == 1);
    assert(!strcmp(command, "/sbin/rc-update rc-update add leonos-dhcp default"));
    assert(invoke("leonos-ntp", "stop") == 127 && calls == 1);
    assert(!strcmp(command, "/sbin/rc-service rc-service leonos-ntp stop"));
    caller_uid = caller_euid = 1000;
    assert(invoke("leonos-device", "status") == 127 && calls == 1);
    assert(!strcmp(command, "/sbin/rc-service rc-service leonos-device status"));
    puts("PASS production rcctl authorization, argv whitelist and environment boundary");
}
