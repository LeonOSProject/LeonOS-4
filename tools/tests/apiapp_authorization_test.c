#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int log_open(const char *path, int flags, ...)
{ (void)path; (void)flags; errno = EACCES; return -1; }
static int log_mkdir(const char *path, mode_t mode)
{ (void)path; (void)mode; errno = EACCES; return -1; }
#define open log_open
#define mkdir log_mkdir
#define main apiapp_main
#include "../../userland/apps/apiapp/main.c"
#undef main
#undef open
#undef mkdir

static unsigned relaunches, workers;
static int child_result, spawn_error, send_progress = 1;
int leonos_admin_elevate(void) { ++relaunches; errno = EINPROGRESS; return 0; }
int leonos_sudo_run_stdout(const char *user, char *const argv[], int output, uint32_t *pid)
{
    assert(!user && !strcmp(argv[0], APIAPP_PATH));
    assert(!strcmp(argv[1], "--install-worker") && !argv[5]);
    assert(!strcmp(argv[2], "/api/example.api") && !strcmp(argv[3], "/usr/lib/leonos/apps/example"));
    assert(!strcmp(argv[4], "1"));
    ++workers;
    if (spawn_error) { errno = spawn_error; return -1; }
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        if (send_progress) {
            uint32_t progress[2] = {4096, 4096};
            assert(write(output, progress, 3) == 3);
            usleep(20000);
            assert(write(output, (char *)progress + 3, sizeof(progress) - 3) == sizeof(progress) - 3);
        }
        _exit(child_result);
    }
    *pid = (uint32_t)child;
    return 0;
}
int leonos_sudo_wait(uint32_t pid, int *status)
{
    int result = waitpid((pid_t)pid, status, WNOHANG);
    if (!result) { errno = EAGAIN; return -1; }
    return result < 0 ? -1 : 0;
}
unsigned long leonos_uptime_ms(void)
{ struct timespec now; assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0); return now.tv_sec * 1000UL + now.tv_nsec / 1000000UL; }
uint32_t leonos_ui_color(uint32_t role) { return role; }
void leonos_ui_bind(struct leonos_ui_surface *s, uint32_t *p, uint32_t w, uint32_t h, uint32_t stride)
{ (void)s; (void)p; (void)w; (void)h; (void)stride; }
void leonos_ui_rect(struct leonos_ui_surface *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t c)
{ (void)s; (void)x; (void)y; (void)w; (void)h; (void)c; }
void leonos_ui_text(struct leonos_ui_surface *s, uint32_t x, uint32_t y, const char *t, uint32_t fg, uint32_t bg)
{ (void)s; (void)x; (void)y; (void)t; (void)fg; (void)bg; }
void leonos_ui_progress(struct leonos_ui_surface *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t value, uint32_t max)
{ (void)s; (void)x; (void)y; (void)w; (void)h; (void)value; (void)max; }
void leonos_ui_activity_bar(struct leonos_ui_surface *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t phase)
{ (void)s; (void)x; (void)y; (void)w; (void)h; (void)phase; }
int leonos_gui_present_window(uint32_t id, uint32_t w, uint32_t h, uint32_t stride, const uint32_t *p)
{ (void)id; (void)w; (void)h; (void)stride; (void)p; return 0; }
int leonos_gui_wait_app_event(struct leonos_gui_app_event *event, uint32_t timeout)
{ (void)event; (void)timeout; usleep(1000); return 0; }
int leonos_spawn_argv(const char *p, char *const a[]) { (void)p; (void)a; abort(); }
int leonos_task_snapshot(struct leonos_task_info *t, uint32_t c, uint64_t *tick)
{ (void)t; (void)c; (void)tick; abort(); }
int leonos_task_kill(uint32_t pid) { (void)pid; abort(); }

int main(void)
{
    alarm(10);
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        child_result = attempt == 0 ? 0 : 1;
        send_progress = attempt < 2;
        spawn_error = attempt == 3 ? ENOENT : 0;
        int result = install_api_with_progress(1, "/api/example.api", "/usr/lib/leonos/apps/example", 1);
        assert(relaunches == 0);
        assert(workers == attempt + 1);
        assert(result == (attempt == 0));
    }
    puts("PASS apiapp: one headless worker, fragmented progress, real exit status, cancellation and spawn failure");
}
