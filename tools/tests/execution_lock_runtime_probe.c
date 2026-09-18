#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BYTES (8u * 1024u * 1024u)
#define ROUNDS 4

static double now(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void touch_pages(unsigned seed)
{
    for (unsigned round = 0; round < ROUNDS; ++round) {
        volatile unsigned char *p = mmap(NULL, BYTES, PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(p != MAP_FAILED);
        for (unsigned i = 0; i < BYTES; i += 4096) {
            assert(p[i] == 0 && p[i + 4095] == 0);
            p[i] = (unsigned char)(seed + round + i / 4096);
            p[i + 4095] = (unsigned char)~p[i];
        }
        for (unsigned i = 0; i < BYTES; i += 4096) {
            assert(p[i] == (unsigned char)(seed + round + i / 4096));
            assert(p[i + 4095] == (unsigned char)~p[i]);
        }
        assert(munmap((void *)p, BYTES) == 0);
    }
}

static void *thread_touch(void *argument)
{
    touch_pages((unsigned)(unsigned long)argument);
    return NULL;
}

static void children(unsigned count)
{
    double start = now();
    pid_t pids[2];
    for (unsigned i = 0; i < count; ++i) {
        pids[i] = fork();
        assert(pids[i] >= 0);
        if (!pids[i]) { touch_pages(i + 11); _exit(0); }
    }
    for (unsigned i = 0; i < count; ++i) {
        int status;
        assert(waitpid(pids[i], &status, 0) == pids[i]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    printf("[execution-bench] processes=%u pages=%u seconds=%.3f\n",
           count, count * ROUNDS * BYTES / 4096, now() - start);
    fflush(stdout);
}

int main(void)
{
    children(1);
    children(2);
    /* CLONE_VM must retain the serialized fallback while two threads fault. */
    pthread_t threads[2];
    for (unsigned long i = 0; i < 2; ++i)
        assert(pthread_create(&threads[i], NULL, thread_touch, (void *)(i + 41)) == 0);
    for (unsigned i = 0; i < 2; ++i) assert(pthread_join(threads[i], NULL) == 0);
    puts("[apk-probe] DONE failures=0 (private and shared address-space faults)");
    return 0;
}
