#ifndef NTCLKS_TIME_DISCIPLINE_H
#define NTCLKS_TIME_DISCIPLINE_H
#include <ntclks/types.h>
struct linux_timex;
struct linux_timespec;
/** @brief Reset phase/adjtime after a clock step, retaining oscillator calibration. */
void time_discipline_clear(void);
/** @brief Return the next disciplined tick duration in nanoseconds. Caller holds the clock lock. */
uint64_t time_discipline_tick_ns(void);
/** @brief Advance PLL/error/leap state at a wall-second boundary; return leap correction. */
int time_discipline_second(uint64_t seconds);
/** @brief Read/change native timex state, requiring privilege for mutation. Clock lock held. */
int time_discipline_adjust(struct linux_timex *value, const struct linux_timespec *now, bool privileged);
/** @brief Return the current UTC-to-TAI offset. Clock lock held. */
int time_discipline_tai(void);
#endif
