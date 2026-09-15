/* Integer NTP phase/frequency discipline for the PIT-backed NTCLKS clock.
 * ABI, limits and PLL/FLL gains follow Linux v6.12 kernel/time/ntp.c and
 * include/linux/timex.h. Hardware PPS is not present on this platform.
 * All state is protected by the timekeeper's IRQ-safe clock lock.
 */
#include <ntclks/time_discipline.h>
#include <ntclks/time.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/errno.h>
#define Q32 4294967296LL
#define MAX_FREQUENCY (500000LL * Q32)
#define PHASE_LIMIT 16000000LL
static int64_t frequency_q32, phase_q32, phase_tick_q32, adjustment_us;
static int64_t adjust_tick_q32, frequency_remainder, maxerror = PHASE_LIMIT, esterror = PHASE_LIMIT;
static int64_t tick_us = 10000, constant = 2, reference_second;
static uint64_t fractional_tick, leap_second = UINT64_MAX;
static int status = STA_UNSYNC, state = TIME_OK, tai;

/** @brief Clamp a signed timekeeping quantity without overflowing intermediate values. */
static int64_t bounded(int64_t n, int64_t low, int64_t high)
{ return n < low ? low : n > high ? high : n; }

void time_discipline_clear(void)
{
    phase_q32 = phase_tick_q32 = adjustment_us = adjust_tick_q32 = 0;
    maxerror = esterror = PHASE_LIMIT;
    status |= STA_UNSYNC;
    state = TIME_OK;
    leap_second = UINT64_MAX;
}

uint64_t time_discipline_tick_ns(void)
{
    int64_t frequency = frequency_q32 + frequency_remainder;
    int64_t correction = frequency / (int64_t)NTCLKS_TICK_HZ;
    frequency_remainder = frequency % (int64_t)NTCLKS_TICK_HZ;
    int64_t total = tick_us * 1000 * Q32 + correction + phase_tick_q32 + adjust_tick_q32;
    /* Limits on tick, oscillator and phase guarantee a positive tick. */
    uint64_t scaled = (uint64_t)total + fractional_tick;
    fractional_tick = scaled & (Q32 - 1);
    return scaled >> 32;
}

int time_discipline_second(uint64_t seconds)
{
    int leap = 0;
    switch (state) {
    case TIME_OK:
        if (status & STA_INS) { state = TIME_INS; leap_second = seconds + 86400 - seconds % 86400; }
        else if (status & STA_DEL) { state = TIME_DEL; leap_second = seconds + 86400 - (seconds + 1) % 86400; }
        break;
    case TIME_INS:
        if (!(status & STA_INS)) { state = TIME_OK; leap_second = UINT64_MAX; }
        else if (seconds == leap_second) { leap = -1; ++tai; state = TIME_OOP; }
        break;
    case TIME_DEL:
        if (!(status & STA_DEL)) { state = TIME_OK; leap_second = UINT64_MAX; }
        else if (seconds == leap_second) { leap = 1; --tai; state = TIME_WAIT; leap_second = UINT64_MAX; }
        break;
    case TIME_OOP: state = TIME_WAIT; leap_second = UINT64_MAX; break;
    case TIME_WAIT: if (!(status & (STA_INS | STA_DEL))) state = TIME_OK; break;
    }
    maxerror += 500;
    if (maxerror > PHASE_LIMIT) { maxerror = PHASE_LIMIT; status |= STA_UNSYNC; }
    phase_tick_q32 = phase_q32 / (1LL << (2 + constant));
    phase_q32 -= phase_tick_q32;
    int64_t step = bounded(adjustment_us, -500, 500);
    adjustment_us -= step;
    adjust_tick_q32 = step * 1000 * Q32 / (int64_t)NTCLKS_TICK_HZ;
    return leap;
}

int time_discipline_tai(void) { return tai; }

int time_discipline_adjust(struct linux_timex *value, const struct linux_timespec *now, bool privileged)
{
    uint32_t mode = value->modes;
    bool single = mode == ADJ_OFFSET_SINGLESHOT || mode == ADJ_OFFSET_SS_READ;
    if (mode && mode != ADJ_OFFSET_SS_READ && !privileged) return -LINUX_EPERM;
    if (!single && (mode & ~(ADJ_OFFSET | ADJ_FREQUENCY | ADJ_MAXERROR | ADJ_ESTERROR |
            ADJ_STATUS | ADJ_TIMECONST | ADJ_TAI | ADJ_MICRO | ADJ_NANO | ADJ_TICK)))
        return -LINUX_EOPNOTSUPP;
    if ((mode & ADJ_TICK) && (value->tick < 9000 || value->tick > 11000)) return -LINUX_EINVAL;
    int64_t old_adjust = adjustment_us;
    if (single) {
        if (mode == ADJ_OFFSET_SINGLESHOT) adjustment_us = value->offset;
    } else {
        if (mode & ADJ_STATUS) {
            if (!(status & STA_PLL) && (value->status & STA_PLL)) reference_second = now->tv_sec;
            if ((status & STA_PLL) && !(value->status & STA_PLL)) { state = TIME_OK; leap_second = UINT64_MAX; }
            status = (status & STA_RONLY) | (value->status & ~STA_RONLY);
        }
        if (mode & ADJ_NANO) status |= STA_NANO;
        if (mode & ADJ_MICRO) status &= ~STA_NANO;
        if (mode & ADJ_FREQUENCY) frequency_q32 = bounded(value->freq, -32768000, 32768000) * 65536000;
        if (mode & ADJ_MAXERROR) maxerror = bounded(value->maxerror, 0, PHASE_LIMIT);
        if (mode & ADJ_ESTERROR) esterror = bounded(value->esterror, 0, PHASE_LIMIT);
        if (mode & ADJ_TIMECONST) constant = bounded(bounded(value->constant, 0, 10) + ((status & STA_NANO) ? 0 : 4), 0, 10);
        if ((mode & ADJ_TAI) && value->constant >= 0 && value->constant <= 100000) tai = value->constant;
        if ((mode & ADJ_OFFSET) && (status & STA_PLL)) {
            int64_t ns = status & STA_NANO ? value->offset : bounded(value->offset, -1000000, 1000000) * 1000;
            ns = bounded(ns, -500000000, 500000000);
            int64_t elapsed = status & STA_FREQHOLD ? 0 : now->tv_sec - reference_second;
            reference_second = now->tv_sec;
            if (elapsed < 0) elapsed = 0;
            int64_t delta = 0;
            status &= ~STA_MODE;
            if (elapsed >= 256 && ((status & STA_FLL) || elapsed > 2048)) {
                delta = ns * (Q32 / 4) / elapsed;
                status |= STA_MODE;
            }
            elapsed = bounded(elapsed, 0, 1LL << (3 + constant));
            delta += ns * elapsed * (1LL << (32 - 2 * (4 + constant)));
            frequency_q32 = bounded(frequency_q32 + delta, -MAX_FREQUENCY, MAX_FREQUENCY);
            phase_q32 = ns * Q32 / (int64_t)NTCLKS_TICK_HZ;
        }
        if (mode & ADJ_TICK) tick_us = value->tick;
    }
    *value = (struct linux_timex){.modes = mode,
        .offset = single ? old_adjust : phase_q32 * (int64_t)NTCLKS_TICK_HZ / Q32 / ((status & STA_NANO) ? 1 : 1000),
        .freq = frequency_q32 / 65536000, .maxerror = maxerror, .esterror = esterror,
        .status = status, .constant = constant, .precision = 1, .tolerance = 32768000,
        .time = {now->tv_sec, now->tv_nsec / ((status & STA_NANO) ? 1 : 1000)},
        .tick = tick_us, .tai = tai};
    return status & (STA_UNSYNC | STA_CLOCKERR | STA_PPSFREQ | STA_PPSTIME) ? TIME_ERROR : state;
}
