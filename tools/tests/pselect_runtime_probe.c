#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "[pselect] FAIL line=%d errno=%d: %s\n", \
    __LINE__, errno, #c); exit(1); } } while (0)

static volatile sig_atomic_t caught;
static void handler(int signal) { caught = signal; }

static void check_restored(void)
{
    sigset_t current;
    CHECK(sigprocmask(SIG_SETMASK, NULL, &current) == 0);
    CHECK(sigismember(&current, SIGUSR1) == 1);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    sigset_t blocked, empty;
    sigemptyset(&empty);
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    CHECK(sigprocmask(SIG_SETMASK, &blocked, NULL) == 0);
    struct sigaction action = {.sa_handler = handler, .sa_flags = SA_RESTART};
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
    struct timespec timeout = {0, 20000000};
    CHECK(pselect(0, NULL, NULL, NULL, &timeout, &empty) == 0);
    check_restored();
    CHECK(raise(SIGUSR1) == 0 && !caught);
    timeout = (struct timespec){1, 0};
    CHECK(pselect(0, NULL, NULL, NULL, &timeout, &empty) == -1 && errno == EINTR);
    CHECK(caught == SIGUSR1);
    check_restored();

    int pipefd[2];
    CHECK(pipe(pipefd) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        usleep(50000);
        _exit(write(pipefd[1], "x", 1) == 1 ? 0 : 1);
    }
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(pipefd[0], &readset);
    CHECK(pselect(pipefd[0] + 1, &readset, NULL, NULL, &timeout, &empty) == 1);
    CHECK(FD_ISSET(pipefd[0], &readset));
    check_restored();
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    close(pipefd[0]);
    close(pipefd[1]);
    FD_ZERO(&readset);
    FD_SET(pipefd[0], &readset);
    CHECK(pselect(pipefd[0] + 1, &readset, NULL, NULL, &timeout, &empty) == -1 && errno == EBADF);
    check_restored();
    puts("[pselect] DONE failures=0 (timeout, SA_RESTART interruption, pipe readiness, error restoration)");
    return 0;
}
