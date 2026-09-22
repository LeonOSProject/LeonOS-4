#ifndef NTCLKS_LOADAVG_H
#define NTCLKS_LOADAVG_H
#include <stdint.h>
/**
 * @brief Advance the 1/5/15-minute exponentially weighted runnable averages by five seconds.
 * @param loads Three Q16 averages, matching Linux sysinfo SI_LOAD_SHIFT (16).
 * @param active Number of running/ready non-idle tasks sampled under the scheduler lock.
 * @return None. Integer arithmetic keeps floating point out of the kernel.
 */
static inline void sched_load_update(uint64_t loads[3], uint32_t active)
{
    /* round(exp(-5 / seconds) * 65536), seconds = 60, 300, 900. */
    static const uint32_t decay[3] = {60296, 64453, 65173};
    uint64_t target = (uint64_t)active << 16;
    for (unsigned i = 0; i < 3; ++i)
        loads[i] = (loads[i] * decay[i] + target * (65536 - decay[i]) + 32768) >> 16;
}
#endif
