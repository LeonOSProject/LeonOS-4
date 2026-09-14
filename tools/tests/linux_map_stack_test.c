/* The same raw Linux mmap call and signal-stack checks run on Linux and NTCLKS. */
#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static uintptr_t stack_start, stack_end;
static volatile sig_atomic_t used_altstack;

static void handler(int number)
{
    volatile char marker;
    uintptr_t here = (uintptr_t)&marker;
    used_altstack = number == SIGUSR1 && here >= stack_start && here < stack_end;
}

unsigned test_linux_map_stack(void)
{
    unsigned failures = 0;
#define CHECK(condition, label) do { \
    int ok = (condition); \
    printf("[apk-probe] %s MAP_STACK: %s\n", ok ? "PASS" : "FAIL", label); \
    failures += !ok; \
} while (0)
    size_t page = (size_t)sysconf(_SC_PAGESIZE), size = page * 17;
    char *mapping = (void *)syscall(SYS_mmap, 0, size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    CHECK(mapping != MAP_FAILED, "raw anonymous mapping");
    if (mapping == MAP_FAILED) return failures;
    CHECK(mapping[0] == 0 && mapping[size - 1] == 0, "zero-filled pages");
    mapping[page] = 42;
    CHECK(mprotect(mapping, page, PROT_NONE) == 0, "guard page protection");
    pid_t child = fork();
    if (!child) {
        volatile char value = mapping[0];
        (void)value;
        _exit(99);
    }
    int status = 0;
    CHECK(child > 0 && waitpid(child, &status, 0) == child && WIFSIGNALED(status) &&
          WTERMSIG(status) == SIGSEGV, "guard page causes SIGSEGV");
    CHECK(mapping[page] == 42, "adjacent stack page stays writable");
    stack_t stack = {.ss_sp = mapping + page, .ss_size = size - page}, previous;
    struct sigaction action = {.sa_handler = handler, .sa_flags = SA_ONSTACK}, old_action;
    sigemptyset(&action.sa_mask);
    stack_start = (uintptr_t)stack.ss_sp;
    stack_end = stack_start + stack.ss_size;
    used_altstack = 0;
    int installed = sigaltstack(&stack, &previous) == 0;
    CHECK(installed, "register alternate signal stack");
    if (installed) {
        int configured = sigaction(SIGUSR1, &action, &old_action) == 0;
        CHECK(configured && raise(SIGUSR1) == 0 && used_altstack,
              "handler actually executes on the alternate stack");
        if (configured) CHECK(sigaction(SIGUSR1, &old_action, NULL) == 0, "restore signal action");
        CHECK(sigaltstack(&previous, NULL) == 0, "restore previous signal stack");
    }
    CHECK(munmap(mapping, size) == 0, "unmap stack and guard");
#undef CHECK
    return failures;
}

#ifdef MAP_STACK_STANDALONE
int main(void) { return test_linux_map_stack() ? 1 : 0; }
#endif
