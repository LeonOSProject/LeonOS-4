#include <reliefos/openrc.h>
#include <reliefos/sudo.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int reliefos_openrc_run(const char *service, const char *action)
{
    if (!service || !action || strchr(service, '/') || strlen(service) > 63) { errno = EINVAL; return -1; }
    char *args[] = {"/usr/lib/leonos/apps/rcctl/rcctl.elf", (char *)service, (char *)action, NULL};
    uint32_t child;
    int status;
    if (geteuid() && strcmp(action, "status")) {
        if (reliefos_sudo_run(NULL, NULL, args, &child) < 0 || reliefos_sudo_wait_command(child, &status) < 0) return -1;
    } else {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (!pid) { execv(args[0], args); _exit(127); }
        pid_t waited;
        do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited < 0) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

int reliefos_openrc_enabled(const char *service)
{
    if (!service || strchr(service, '/') || strlen(service) > 63) { errno = EINVAL; return -1; }
    char path[128];
    struct stat st;
    snprintf(path, sizeof(path), "/etc/runlevels/default/%s", service);
    if (!lstat(path, &st)) return S_ISLNK(st.st_mode);
    return errno == ENOENT ? 0 : -1;
}

int reliefos_openrc_spawn(const char *service, const char *action)
{
    pid_t child = fork();
    if (child != 0) return child;
    int result = reliefos_openrc_run(service, action);
    _exit(result < 0 ? 125 : result);
}

int reliefos_openrc_poll(int child, int *result)
{
    if (child <= 0 || !result) { errno = EINVAL; return -1; }
    int status;
    pid_t waited = waitpid(child, &status, WNOHANG);
    if (waited < 0) return errno == EINTR ? 0 : -1;
    if (!waited) return 0;
    *result = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return 1;
}
/* Published libleonos.so.2 aliases; keep these in the defining translation unit. */
extern __typeof__(reliefos_openrc_enabled) leonos_openrc_enabled __attribute__((alias("reliefos_openrc_enabled")));
extern __typeof__(reliefos_openrc_poll) leonos_openrc_poll __attribute__((alias("reliefos_openrc_poll")));
extern __typeof__(reliefos_openrc_run) leonos_openrc_run __attribute__((alias("reliefos_openrc_run")));
extern __typeof__(reliefos_openrc_spawn) leonos_openrc_spawn __attribute__((alias("reliefos_openrc_spawn")));
