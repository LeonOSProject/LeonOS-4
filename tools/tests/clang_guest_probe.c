/* Guest probe for Alpine Clang on NTCLKS.
 *
 * clang --version fails with thousands of "Error relocating" lines, so the
 * decisive evidence is the FIRST stderr line and the largest contiguous mmap
 * the kernel will actually honour.  Both are captured here.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOG_PATH "/tmp/clang-probe.log"
#define HEAD_LINES 14

static unsigned failures;

static void check(int ok, const char *label)
{
    printf("[clang-probe] %s %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
}

/* Run argv, merging stdout and stderr into LOG_PATH, and report the exit
 * status.  Returns the child status so callers can distinguish a signal from a
 * non-zero exit. */
static int run_captured(char *const argv[])
{
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    pid_t child = fork();
    if (child < 0) {
        close(fd);
        return -1;
    }
    if (!child) {
        if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) _exit(126);
        if (fd > 2) close(fd);
        execv(argv[0], argv);
        _exit(127);
    }
    close(fd);
    int status = 0;
    time_t deadline = time(NULL) + 180;
    while (waitpid(child, &status, WNOHANG) != child) {
        if (time(NULL) > deadline) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            return -1;
        }
        usleep(20000);
    }
    return status;
}

/* Echo the head of the capture log.  The relocation flood buries the one line
 * that names the library which failed to load, so print from the front and
 * summarise the rest instead of dumping the tail. */
static void dump_head(const char *label)
{
    FILE *stream = fopen(LOG_PATH, "re");
    char line[512];
    unsigned total = 0;
    if (!stream) {
        printf("[clang-probe] %s: no capture log\n", label);
        return;
    }
    while (fgets(line, sizeof(line), stream)) {
        if (++total <= HEAD_LINES) {
            line[strcspn(line, "\n")] = 0;
            printf("[clang-probe] %s:%02u %s\n", label, total, line);
        }
    }
    fclose(stream);
    printf("[clang-probe] %s: total_output_lines=%u\n", label, total);
}

/* Largest contiguous anonymous reservation the kernel grants, in MiB.  musl's
 * map_library reserves a shared object's whole PT_LOAD span in one mmap, so
 * this bounds which libraries a process can load at all. */
static void probe_mmap_ceiling(void)
{
    static const unsigned steps_mib[] = {8, 16, 32, 64, 96, 128, 160, 192, 224,
                                         256, 288, 320, 384, 448, 512};
    unsigned largest = 0;
    for (unsigned i = 0; i < sizeof(steps_mib) / sizeof(steps_mib[0]); ++i) {
        size_t bytes = (size_t)steps_mib[i] * 1024u * 1024u;
        void *map = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (map == MAP_FAILED) {
            printf("[clang-probe] mmap %u MiB failed errno=%d (%s)\n", steps_mib[i],
                   errno, strerror(errno));
            break;
        }
        largest = steps_mib[i];
        munmap(map, bytes);
    }
    printf("[clang-probe] largest_contiguous_mmap_mib=%u\n", largest);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("[clang-probe] START");

    probe_mmap_ceiling();

    char *const version[] = {"/usr/bin/clang", "--version", NULL};
    int status = run_captured(version);
    dump_head("clang-version");
    check(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "clang --version exits 0");

    int fd = open("/tmp/clang-probe.c", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    static const char source[] =
        "#include <stdio.h>\n"
        "int main(void) { puts(\"CLANG_HOSTED_OK\"); return 0; }\n";
    check(fd >= 0 && write(fd, source, sizeof(source) - 1) == (ssize_t)(sizeof(source) - 1),
          "hosted source file written");
    if (fd >= 0) close(fd);

    char *const compile[] = {"/usr/bin/clang", "/tmp/clang-probe.c", "-o",
                             "/tmp/clang-probe.out", NULL};
    status = run_captured(compile);
    dump_head("clang-compile");
    check(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "clang compiles a hosted C program");

    char *const run[] = {"/tmp/clang-probe.out", NULL};
    status = run_captured(run);
    dump_head("hosted-run");
    check(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "compiled program exits 0");

    char *const link[] = {"/usr/bin/clang", "-dynamic-linker", "/lib/ld-musl-x86_64.so.1",
                          "/tmp/clang-probe.c", "-o", "/tmp/clang-probe-dyn", NULL};
    status = run_captured(link);
    dump_head("clang-dynamic-link");
    check(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "clang links a dynamic executable");

    char *const run_dyn[] = {"/tmp/clang-probe-dyn", NULL};
    status = run_captured(run_dyn);
    dump_head("hosted-dynamic-run");
    check(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "dynamically linked program exits 0");
    check(access("/tmp/clang-probe-dyn", X_OK) == 0, "dynamic output is executable");

    printf("[clang-probe] DONE failures=%u\n", failures);
    return failures ? 1 : 0;
}
