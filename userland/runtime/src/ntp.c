#include <leonos/system.h>
#include <leonos/net.h>
#include <leonos/openrc.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/timex.h>
#include <time.h>

/* Read only a fresh notification from the root-owned standard ntpd hook.
 * A restarted process alone is not evidence of a selected NTP peer. */
static int ntp_notification(void)
{
    int fd = open("/run/leonos/ntp-state", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid || (st.st_mode & 022)) {
        close(fd); return 0;
    }
    FILE *stream = fdopen(fd, "r");
    if (!stream) { close(fd); return 0; }
    unsigned long long acquired;
    unsigned stratum;
    int count = fscanf(stream, "acquired=%llu\nstratum=%u", &acquired, &stratum);
    fclose(stream);
    struct timespec now;
    if (count != 2 || !stratum || stratum >= 16 || clock_gettime(CLOCK_MONOTONIC, &now) ||
        now.tv_sec < 0 || acquired > (unsigned long long)now.tv_sec ||
        (unsigned long long)now.tv_sec - acquired > 120) return 0;
    struct timex tx = {0};
    /* BusyBox does not maintain Linux's maxerror field. Its selected-peer
     * notification and an active PLL are separate from TIME_OK certification. */
    return adjtimex(&tx) >= 0 && (tx.status & STA_PLL);
}

int leonos_time_ntp_sync(uint32_t timeout_ms, struct leonos_time_sync *result)
{
    if (!result) { errno = EINVAL; return -1; }
    memset(result, 0, sizeof(*result));
    result->timeout_ms = timeout_ms;
    result->status = LEONOS_NET_STATUS_NTP_TIMEOUT;
    if (leonos_openrc_run("leonos-ntp", "restart")) { errno = EIO; return -1; }
    struct timespec started, now;
    if (clock_gettime(CLOCK_MONOTONIC, &started)) return -1;
    uint64_t budget = timeout_ms ? timeout_ms : 15000;
    for (;;) {
        if (ntp_notification()) {
            struct timespec wall;
            if (clock_gettime(CLOCK_REALTIME, &wall)) return -1;
            result->unix_seconds = wall.tv_sec;
            result->valid = 1;
            result->status = LEONOS_NET_STATUS_OK;
            return 0;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
        int64_t elapsed = (now.tv_sec - started.tv_sec) * 1000 +
            (now.tv_nsec - started.tv_nsec) / 1000000;
        if (elapsed >= (int64_t)budget) { errno = ETIMEDOUT; return -1; }
        struct timespec interval = {.tv_nsec = 100000000};
        if (nanosleep(&interval, NULL) && errno != EINTR) return -1;
    }
}
