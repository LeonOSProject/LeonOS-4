#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static time_t wall_seconds;
static int clock_error;
static int test_gettimeofday(struct timeval *value, void *zone)
{
    (void)zone;
    if (clock_error) { errno = clock_error; return -1; }
    *value = (struct timeval){.tv_sec = wall_seconds, .tv_usec = 987654};
    return 0;
}

#define gettimeofday test_gettimeofday
#include "../../userland/runtime/src/procsys.c"
#undef gettimeofday

unsigned long leonos_uptime_ms(void) { return 123456; }

int main(void)
{
    const struct {
        time_t epoch;
        unsigned year, month, day, hour, minute, second;
    } cases[] = {
        {0, 1970, 1, 1, 0, 0, 0},
        {951827696, 2000, 2, 29, 12, 34, 56},
        {1789254922, 2026, 9, 12, 23, 15, 22},
        {1789254923, 2026, 9, 12, 23, 15, 23},
        {2208988800, 2040, 1, 1, 0, 0, 0},
        {4107542400, 2100, 3, 1, 0, 0, 0},
    };
    /* The SDK's historical calendar fields are UTC, even under a local TZ. */
    assert(setenv("TZ", "UTC-8", 1) == 0);
    tzset();
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        wall_seconds = cases[i].epoch;
        struct leonos_time_info info;
        memset(&info, 0xa5, sizeof(info));
        assert(leonos_time_info(&info) == 0);
        assert(info.valid == 1 && info.reserved == 0);
        assert(info.unix_seconds == (uint64_t)wall_seconds && info.uptime_ms == 123456);
        assert(info.year == cases[i].year && info.month == cases[i].month && info.day == cases[i].day);
        assert(info.hour == cases[i].hour && info.minute == cases[i].minute && info.second == cases[i].second);
    }
    struct leonos_time_info info;
    clock_error = EIO;
    assert(leonos_time_info(&info) == -1 && errno == EIO);
    assert(!info.valid && !info.unix_seconds && !info.year && !info.hour);
    clock_error = 0;
    wall_seconds = -1;
    assert(leonos_time_info(&info) == -1 && errno == EOVERFLOW && !info.valid);
    wall_seconds = INT64_MAX;
    assert(leonos_time_info(&info) == -1 && errno == EOVERFLOW && !info.valid);
    assert(leonos_time_info(NULL) == -1 && errno == EINVAL);
    puts("PASS time info: UTC calendar, ticking seconds, leap/century boundaries, errors");
}
