#ifndef LEONOS_UAPI_LINUX_TIMEX_H
#define LEONOS_UAPI_LINUX_TIMEX_H
#include <linux/types.h>
/* Linux v6.12 native x86-64 timex; not the i386/x32 ABI. */
struct linux_timex {
    uint32_t modes, pad0;
    int64_t offset, freq, maxerror, esterror;
    int32_t status, pad1;
    int64_t constant, precision, tolerance;
    struct { int64_t tv_sec, tv_usec; } time;
    int64_t tick, ppsfreq, jitter;
    int32_t shift, pad2;
    int64_t stabil, jitcnt, calcnt, errcnt, stbcnt;
    int32_t tai, reserved[11];
};
#define ADJ_OFFSET 0x0001
#define ADJ_FREQUENCY 0x0002
#define ADJ_MAXERROR 0x0004
#define ADJ_ESTERROR 0x0008
#define ADJ_STATUS 0x0010
#define ADJ_TIMECONST 0x0020
#define ADJ_TAI 0x0080
#define ADJ_SETOFFSET 0x0100
#define ADJ_MICRO 0x1000
#define ADJ_NANO 0x2000
#define ADJ_TICK 0x4000
#define ADJ_OFFSET_SINGLESHOT 0x8001
#define ADJ_OFFSET_SS_READ 0xa001
#define STA_PLL 0x0001
#define STA_PPSFREQ 0x0002
#define STA_PPSTIME 0x0004
#define STA_FLL 0x0008
#define STA_INS 0x0010
#define STA_DEL 0x0020
#define STA_UNSYNC 0x0040
#define STA_FREQHOLD 0x0080
#define STA_PPSSIGNAL 0x0100
#define STA_CLOCKERR 0x1000
#define STA_NANO 0x2000
#define STA_MODE 0x4000
#define STA_RONLY 0xff00
#define TIME_OK 0
#define TIME_INS 1
#define TIME_DEL 2
#define TIME_OOP 3
#define TIME_WAIT 4
#define TIME_ERROR 5
_Static_assert(sizeof(struct linux_timex) == 208, "native Linux timex");
#endif
