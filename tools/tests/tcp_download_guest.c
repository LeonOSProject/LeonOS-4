#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    /* The kernel autospawn hook has no login environment. */
    if (setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1)) return 1;
    struct timespec start, end;
    FILE *input = fopen("/tmp/download-package", "r");
    char package[128];
    if (!input || fscanf(input, "%127s", package) != 1) return 1;
    fclose(input);
    int install = access("/tmp/download-install", F_OK) == 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("[tcp-download] START %s\n", package);
    pid_t child = fork();
    if (child == 0) {
        if (install) {
            execl("/sbin/apk", "apk", "--timeout", "30", "add", "gcc", "leonos-musl-dev", "make", (char *)NULL);
            _exit(127);
        }
        execl("/sbin/apk", "apk", "--timeout", "30", "fetch", "--output", "/tmp",
              package, (char *)NULL);
        _exit(127);
    }
    int status = -1;
    if (child < 0 || waitpid(child, &status, 0) != child) return 1;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seconds = end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("[tcp-download] DONE status=%d seconds=%.3f\n", status, seconds);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
    if (install) {
        execl("/bin/busybox", "sh", "-ec",
              "gcc --version; make --version; "
              "printf '#include <stdio.h>\nint main(void) { puts(\"APK_GCC_NETWORK_OK\"); return 0; }\n' > /tmp/network-gcc.c; "
              "gcc /tmp/network-gcc.c -o /tmp/network-gcc; /tmp/network-gcc; "
              "sha256sum /var/cache/apk/gcc-*.apk", (char *)NULL);
        return 1;
    }
    execl("/bin/busybox", "sh", "-c", "sha256sum /tmp/*.apk", (char *)NULL);
    return 1;
}
