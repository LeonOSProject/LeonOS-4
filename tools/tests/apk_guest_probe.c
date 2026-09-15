#define _GNU_SOURCE
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>

static unsigned failures;
static void check(int ok, const char *label)
{
    printf("[apk-probe] %s %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
}
static int contains(const char *path, const char *text)
{
    char buffer[16384];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t size = read(fd, buffer, sizeof(buffer)-1);
    close(fd);
    if (size < 0) return 0;
    buffer[size] = 0;
    return strstr(buffer, text) != NULL;
}
static void command(const char *label, int success, ...)
{
    char *argv[32];
    va_list ap;
    va_start(ap, success);
    unsigned n = 0;
    while (n + 1 < sizeof(argv)/sizeof(argv[0]) && (argv[n] = va_arg(ap, char *))) ++n;
    va_end(ap);
    argv[n] = NULL;
    pid_t child = fork();
    if (!child) {
        int fd = open("/tmp/apk-command.log", O_WRONLY|O_CREAT|O_TRUNC, 0600);
        if (fd < 0 || dup2(fd, 1) < 0 || dup2(fd, 2) < 0) _exit(126);
        if (fd > 2) close(fd);
        execv(argv[0], argv);
        _exit(127);
    }
    int status = 0, timeout = 0;
    if (child > 0) {
        time_t deadline = time(NULL) + (!strcmp(label, "Alpine GCC installation") ? 300 : 90);
        pid_t done;
        while ((done = waitpid(child, &status, WNOHANG)) == 0 || (done < 0 && errno == EINTR)) {
            if (time(NULL) > deadline) {
                kill(child, SIGKILL); waitpid(child, &status, 0); timeout = 1; break;
            }
            usleep(50000);
        }
        if (done < 0) timeout = 1;
    }
    printf("[apk-probe] BEGIN %s status=%d timeout=%d\n", label, status, timeout);
    int fd = open("/tmp/apk-command.log", O_RDONLY);
    if (fd >= 0) {
        char buffer[4096]; ssize_t size;
        while ((size = read(fd, buffer, sizeof(buffer))) > 0) write(1, buffer, (size_t)size);
        close(fd);
    }
    puts("[apk-probe] END output");
    check(child > 0 && !timeout && WIFEXITED(status) &&
          ((WEXITSTATUS(status) == 0) == success), label);
}
#define APK "/sbin/apk", "--repositories-file", "/dev/null"
#define FIXTURE "/usr/lib/leonos/tests/apk"
static int configure_proxy(void)
{
    int fd = open(FIXTURE "/proxy", O_RDONLY);
    if (fd < 0) return errno == ENOENT ? 0 : -1;
    char proxy[1024];
    ssize_t size = read(fd, proxy, sizeof(proxy));
    close(fd);
    if (size <= 0 || size >= (ssize_t)sizeof(proxy)) return -1;
    proxy[size] = 0;
    return setenv("https_proxy", proxy, 1) || setenv("http_proxy", proxy, 1) ? -1 : 0;
}
static void null_fault_handler(int sig, siginfo_t *info, void *context)
{
    ucontext_t *uc = context;
    _exit(sig == SIGSEGV && info->si_addr == NULL && uc->uc_mcontext.gregs[REG_RIP] == 0 ? 0 : 88);
}

static void fault_delivery_test(void)
{
    for (int caught = 0; caught < 2; ++caught) {
        pid_t child = fork();
        if (!child) {
            struct sigaction action = {.sa_sigaction = null_fault_handler, .sa_flags = SA_SIGINFO};
            if (caught && sigaction(SIGSEGV, &action, NULL)) _exit(89);
            __asm__ volatile("xor %%eax, %%eax; jmp *%%rax" ::: "rax", "memory");
            _exit(90);
        }
        int status = 0;
        pid_t done = -1;
        for (unsigned i = 0; child > 0 && i < 200; ++i) {
            done = waitpid(child, &status, WNOHANG);
            if (done == child) break;
            usleep(50000);
        }
        check(done == child && (caught ? WIFEXITED(status) && WEXITSTATUS(status) == 0 :
              WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV),
              caught ? "NULL RIP caught SIGSEGV with ucontext" : "NULL RIP terminates and is reaped");
        if (child > 0 && done != child) { kill(child, SIGKILL); waitpid(child, &status, 0); }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("[apk-probe] START");
#ifdef APK_TESTING_ONLY
    extern unsigned test_linux_map_stack(void);
    failures += test_linux_map_stack();
    if (configure_proxy() < 0) return 1;
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    /* This QEMU user-network fixture gets its resolver from the OpenRC udhcpc service. */
    for (unsigned i = 0; i < 100 && !contains("/etc/resolv.conf", "nameserver 10.0.2.3"); ++i)
        usleep(100000);
    check(contains("/etc/resolv.conf", "nameserver 10.0.2.3"), "DHCP resolver ready before APK");
    command("HTTPS signed testing indexes", 1, "/sbin/apk", "--timeout", "30", "update", NULL);
    check(!contains("/tmp/apk-command.log", "WARNING") && !contains("/tmp/apk-command.log", "ERROR"),
          "testing repository has no trust or fetch errors");
    command("tagged HyFetch installation", 1, "/sbin/apk", "--timeout", "60", "add", "hyfetch@testing", NULL);
    check(!contains("/tmp/apk-command.log", "ERROR") && !contains("/tmp/apk-command.log", "not found"),
          "HyFetch transaction and Bash scripts have no errors");
    check(contains("/etc/apk/world", "hyfetch@testing\n"), "testing tag recorded in world");
    check(contains("/etc/shells", "/bin/bash\n"), "Bash post-install registered shell");
    command("Bash package ownership", 1, APK, "info", "--who-owns", "/bin/bash", NULL);
    check(contains("/tmp/apk-command.log", "owned by bash-"), "Bash owns its executable");
    command("Bash execution", 1, "/bin/bash", "-c",
            "a=(17 25); test $((a[0]+a[1])) -eq 42", NULL);
    command("HyFetch version", 1, "/usr/bin/hyfetch", "--version", NULL);
    check(contains("/tmp/apk-command.log", "Version: "), "HyFetch version output");
    command("HyFetch help", 1, "/usr/bin/hyfetch", "--help", NULL);
    check(contains("/tmp/apk-command.log", "Usage:"), "HyFetch help output");
    command("custom Fastfetch retained", 1, APK, "info", "--who-owns", "/usr/bin/fastfetch", NULL);
    check(contains("/tmp/apk-command.log", "leonos-fastfetch-"), "custom Fastfetch still owns executable");
    command("remove HyFetch", 1, "/sbin/apk", "--no-network", "del", "hyfetch", NULL);
    check(!contains("/etc/shells", "/bin/bash\n"), "Bash pre-deinstall removed shell");
    printf("[apk-probe] DONE failures=%u\n", failures);
    return failures ? 1 : 0;
#endif
    fault_delivery_test();
#ifdef ALPINE_RUNTIME_ONLY
    goto runtime;
#endif
    command("version", 1, APK, "--version", NULL);
    command("file owner", 1, APK, "info", "--who-owns", "/usr/bin/fastfetch", NULL);
    check(contains("/tmp/apk-command.log", "leonos-fastfetch-"), "real Fastfetch owner");
    command("local signed index", 1, APK, "--repository",
            "/usr/share/leonos/apk/repository/packages.adb", "update", NULL);
    command("signed install with scripts", 1, APK, "--repository", FIXTURE "/v1/packages.adb",
            "add", "leonos-apk-probe", NULL);
    check(contains("/usr/share/apk-probe/value", "version one"), "extracted payload");
    check(contains("/tmp/apk-post-install", "post-install"), "post-install script ran");
    check(contains("/tmp/apk-trigger", "trigger"), "trigger script ran");
    struct stat metadata;
    check(stat("/usr/share/apk-probe/value", &metadata) == 0 && metadata.st_uid == 0 &&
          metadata.st_gid == 0 && (metadata.st_mode & 0777) == 0644, "package owner and permissions");
    int fd = open("/etc/apk-probe.conf", O_WRONLY|O_TRUNC);
    check(fd >= 0 && write(fd, "administrator\n", 14) == 14, "edit configuration");
    if (fd >= 0) close(fd);
    command("signed upgrade", 1, APK, "--repository", FIXTURE "/v2/packages.adb",
            "upgrade", "leonos-apk-probe", NULL);
    check(contains("/usr/share/apk-probe/value", "version two"), "upgraded payload");
    check(contains("/etc/apk-probe.conf", "administrator") &&
          contains("/etc/apk-probe.conf.apk-new", "new default"), "preserved edited configuration");
    command("remove", 1, APK, "del", "leonos-apk-probe", NULL);
    check(access("/usr/share/apk-probe/value", F_OK) < 0, "removed owned file");
    command("Fastfetch conflict", 0, APK, "--repository", FIXTURE "/conflict/packages.adb",
            "add", "fastfetch", NULL);
    check(contains("/tmp/apk-command.log", "!fastfetch"), "dependency conflict is the rejection reason");
    command("unsigned rejection", 0, APK, "--cache-dir", "/var/cache/apk", "add", FIXTURE "/unsigned.apk", NULL);
    check(contains("/tmp/apk-command.log", "UNTRUSTED"), "signature verification is active");
    command("Alpine zlib install", 1, APK, "--cache-dir", "/var/cache/apk", "add", FIXTURE "/zlib.apk", NULL);
    command("Alpine library execution", 1, "/usr/lib/leonos/tests/apk-zlib.elf", NULL);
    command("Alpine zlib removal", 1, APK, "del", "zlib", NULL);
    if (configure_proxy() < 0) return 1;
    command("HTTPS signed Alpine indexes", 1, "/sbin/apk", "--timeout", "20", "update", NULL);
    check(!contains("/tmp/apk-command.log", "WARNING") && !contains("/tmp/apk-command.log", "ERROR"),
          "default repositories have no fetch or signature warnings");
    DIR *cache = opendir("/var/cache/apk");
    if (cache) {
        struct dirent *entry;
        while ((entry = readdir(cache))) {
            size_t size = strlen(entry->d_name);
            if (strstr(entry->d_name, "APKINDEX") || strstr(entry->d_name, "Packages")) {
                char path[512]; struct stat st;
                snprintf(path, sizeof(path), "/var/cache/apk/%s", entry->d_name);
                int result = stat(path, &st);
                printf("[apk-probe] cache=%s mtime=%lld now=%lld\n", entry->d_name,
                       result ? -1LL : (long long)st.st_mtime, (long long)time(NULL));
                check(result == 0 && time(NULL) - st.st_mtime < 14400,
                      "index timestamp is fresh");
            }
            if (size > 4 && !strcmp(entry->d_name + size - 4, ".apk")) {
                char path[512];
                snprintf(path, sizeof(path), "/var/cache/apk/%s", entry->d_name);
                check(unlink(path) == 0, "clear cached package before HTTPS download");
            }
        }
        closedir(cache);
    }
    command("named Alpine HTTPS installation", 1, "/sbin/apk", "--timeout", "20", "add", "zlib@alpine", NULL);
    command("repeat Alpine installation", 1, "/sbin/apk", "--no-network", "add", "zlib@alpine", NULL);
    check(!contains("/tmp/apk-command.log", "fetch ") &&
          !contains("/tmp/apk-command.log", "WARNING") && !contains("/tmp/apk-command.log", "ERROR"),
          "fresh indexes are reused without network");
    command("downloaded Alpine library execution", 1, "/usr/lib/leonos/tests/apk-zlib.elf", NULL);
    command("named Alpine removal", 1, APK, "del", "zlib", NULL);
    command("tagged Alpine Fastfetch conflict", 0, "/sbin/apk", "--timeout", "20", "add", "fastfetch@alpine", NULL);
    check(contains("/tmp/apk-command.log", "!fastfetch"), "tagged repository cannot replace custom Fastfetch");
    if (access("/usr/lib/leonos/tests/apk-cache", F_OK) == 0) {
        command("populate test package cache", 1, "/bin/busybox", "sh", "-c",
                "cp /usr/lib/leonos/tests/apk-cache/*.apk /var/cache/apk/", NULL);
        command("Alpine GCC installation", 1, "/sbin/apk", "--cache-dir", "/var/cache/apk",
                "--no-network", "add", "gcc", NULL);
    } else {
        command("Alpine GCC installation", 1, "/sbin/apk", "--cache-dir", "/var/cache/apk",
                "--timeout", "60", "add", "gcc", NULL);
    }
    check(!contains("/tmp/apk-command.log", "ERROR"), "GCC has no ownership or hardlink errors");
#ifdef ALPINE_RUNTIME_ONLY
runtime:
#endif
    command("Alpine GCC execution", 1, "/usr/bin/gcc", "--version", NULL);
    command("Alpine ar execution", 1, "/usr/bin/ar", "--version", NULL);
    struct stat ar, alias;
    check(!stat("/usr/bin/ar", &ar) && !stat("/usr/x86_64-alpine-linux-musl/bin/ar", &alias) &&
          ar.st_dev == alias.st_dev && ar.st_ino == alias.st_ino, "binutils hardlink identity");
    fd = open("/tmp/apk-gcc.c", O_WRONLY|O_CREAT|O_TRUNC, 0644);
#ifdef ALPINE_RUNTIME_ONLY
    const char source[] =
        "#include <pthread.h>\n#include <stdio.h>\n"
        "static _Thread_local int value = 17;\n"
        "static void *worker(void *p) { (void)p; value++; return (void *)(long)value; }\n"
        "int main(void) { pthread_t t; void *result;\n"
        "if (pthread_create(&t, 0, worker, 0) || pthread_join(t, &result)) return 1;\n"
        "if ((long)result != 18 || value != 17) return 2;\n"
        "puts(\"ALPINE_GCC_PTHREAD_OK\"); return 0; }\n";
#else
    const char source[] = "int main(void) { return 0; }\n";
#endif
    check(fd >= 0 && write(fd, source, sizeof(source)-1) == sizeof(source)-1, "GCC source file");
    if (fd >= 0) close(fd);
    command("Alpine GCC object compilation", 1, "/usr/bin/gcc", "-c", "/tmp/apk-gcc.c", "-o", "/tmp/apk-gcc.o", NULL);
#ifdef ALPINE_RUNTIME_ONLY
    command("Alpine GCC dynamic link", 1, "/usr/bin/gcc", "-pthread", "/tmp/apk-gcc.c",
            "-o", "/tmp/apk-gcc-dynamic", NULL);
    command("GCC generated dynamic pthread program", 1, "/tmp/apk-gcc-dynamic", NULL);
    check(contains("/tmp/apk-command.log", "ALPINE_GCC_PTHREAD_OK"), "dynamic stdio and thread-local storage");
    command("Alpine GCC static link", 1, "/usr/bin/gcc", "-static", "-pthread", "/tmp/apk-gcc.c",
            "-o", "/tmp/apk-gcc-static", NULL);
    command("GCC generated static pthread program", 1, "/tmp/apk-gcc-static", NULL);
    check(contains("/tmp/apk-command.log", "ALPINE_GCC_PTHREAD_OK"), "static stdio and thread-local storage");
    command("Alpine make", 1, "/usr/bin/make", "--version", NULL);
    command("Alpine nano", 1, "/usr/bin/nano", "--version", NULL);
    command("Alpine jq", 1, "/usr/bin/jq", "-en", "[range(1;11)] | add == 55", NULL);
    command("Alpine OpenSSL", 1, "/usr/bin/openssl", "dgst", "-sha256", "/tmp/apk-gcc.c", NULL);
    command("Alpine curl", 1, "/usr/bin/curl", "--fail", "file:///tmp/apk-gcc.c", NULL);
#endif
    printf("[apk-probe] DONE failures=%u\n", failures);
    return failures ? 1 : 0;
}
