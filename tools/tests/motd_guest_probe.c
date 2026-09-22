#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

static int failures;
#define CHECK(test) do { if (!(test)) { printf("[motd-test] FAIL line=%d errno=%d\n", __LINE__, errno); ++failures; } } while (0)

static void run_motd(int columns, int zh, int hush)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    CHECK(master >= 0);
    if (master < 0) return;
    CHECK(grantpt(master) == 0 && unlockpt(master) == 0);
    char tty[128]; snprintf(tty, sizeof(tty), "%s", ptsname(master));
    int slave = open(tty, O_RDWR | O_NOCTTY);
    CHECK(slave >= 0);
    struct winsize size = {.ws_row = 24, .ws_col = (unsigned short)columns};
    CHECK(ioctl(slave, TIOCSWINSZ, &size) == 0);
    int pipefd[2]; CHECK(pipe(pipefd) == 0);
    fflush(stdout);
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
        close(slave); close(master);
        setsid();
        setenv("PAM_USER", "root", 1); setenv("PAM_TTY", tty, 1);
        setenv("LC_ALL", zh ? "zh_CN.UTF-8" : "en_US.UTF-8", 1);
        setenv("COLUMNS", "999", 1);
        execl("/usr/lib/leonos/motd", "motd", (char *)0);
        _exit(127);
    }
    close(pipefd[1]);
    char output[8192]; size_t used = 0; ssize_t n;
    while ((n = read(pipefd[0], output + used, sizeof(output) - used - 1)) > 0) used += (size_t)n;
    output[used] = 0;
    close(pipefd[0]); close(slave); close(master);
    int status; CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (hush) { CHECK(used == 0); return; }
    CHECK(strstr(output, zh ? "欢迎使用" : "Welcome to") != NULL);
    CHECK(strstr(output, "hushlogin") != NULL);
    if (columns >= 80) {
        CHECK(strstr(output, "NTCLKS") != NULL);
        CHECK(strstr(output, zh ? "内存:" : "Memory:") != NULL);
        CHECK(strstr(output, zh ? "不可用" : "N/A") == NULL);
    }
    mbstate_t state = {0}; size_t cells = 0;
    for (const char *p = output; *p;) {
        wchar_t wc; size_t len = mbrtowc(&wc, p, strlen(p), &state);
        CHECK(len != (size_t)-1 && len != (size_t)-2 && len > 0);
        if (!len || len == (size_t)-1 || len == (size_t)-2) break;
        if (wc == '\n') { CHECK(cells < (size_t)columns); cells = 0; }
        else { int width = wcwidth(wc); CHECK(width >= 0); if (width > 0) cells += (size_t)width; }
        p += len;
    }
    printf("[motd-test] snapshot columns=%d language=%s\n%s", columns, zh ? "zh" : "en", output);
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    puts("[motd-test] BEGIN");
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do { clock_gettime(CLOCK_MONOTONIC, &now); } while (now.tv_sec - start.tv_sec < 6);
    struct sysinfo info;
    CHECK(sysinfo(&info) == 0 && info.loads[0] > 0 && info.totalram > 0);
    printf("[motd-test] sysinfo load=%lu uptime=%ld\n", info.loads[0], info.uptime);
    run_motd(80, 0, 0); run_motd(40, 1, 0);
    run_motd(80, 1, 0);
    int fd = open("/root/.hushlogin", O_CREAT | O_EXCL | O_WRONLY, 0600);
    CHECK(fd >= 0);
    if (fd >= 0) {
        close(fd); run_motd(80, 0, 1); CHECK(unlink("/root/.hushlogin") == 0);
    }
    printf("[motd-test] DONE failures=%d\n", failures);
    return failures ? 1 : 0;
}
