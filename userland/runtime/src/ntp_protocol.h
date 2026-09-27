#ifndef LEONOS_NTP_PROTOCOL_H
#define LEONOS_NTP_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

static uint32_t ntp_u32(const unsigned char *p)
{ return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void ntp_put32(unsigned char *p, uint32_t value)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (24 - 8 * i)); }

static int64_t ntp_timestamp_ns(const unsigned char *p, int64_t reference)
{
    const int64_t epoch = 2208988800LL, era = 1LL << 32;
    if (reference < 946684800LL || reference > 4102444800LL) reference = 1704067200LL;
    int64_t seconds = ((reference + epoch) & ~(era - 1)) + ntp_u32(p) - epoch;
    if (seconds - reference > era / 2) seconds -= era;
    else if (reference - seconds > era / 2) seconds += era;
    return seconds * 1000000000LL + ((uint64_t)ntp_u32(p + 4) * 1000000000ULL >> 32);
}

/* RFC 5905: match origin, reject unsynchronised/KoD/broadcast responses,
 * unfold the era, and account for server processing before estimating delay. */
static int ntp_decode(const unsigned char *packet, size_t length,
                       const unsigned char *request, int64_t reference,
                       int64_t elapsed_ns, struct timespec *out)
{
    if (length < 48) return -1;
    unsigned version = (packet[0] >> 3) & 7;
    if ((packet[0] >> 6) == 3 || (version != 3 && version != 4) ||
        (packet[0] & 7) != 4 || packet[1] == 0 || packet[1] > 15 ||
        memcmp(packet + 24, request + 40, 8) ||
        !(ntp_u32(packet + 32) | ntp_u32(packet + 36)) ||
        !(ntp_u32(packet + 40) | ntp_u32(packet + 44)) || elapsed_ns < 0) return -1;
    int64_t receive = ntp_timestamp_ns(packet + 32, reference);
    int64_t transmit = ntp_timestamp_ns(packet + 40, reference);
    int64_t processing = transmit - receive;
    if (processing < 0 || processing > elapsed_ns + 100000000LL || transmit < 0) return -1;
    int64_t estimate = transmit + (elapsed_ns > processing ? (elapsed_ns - processing) / 2 : 0);
    out->tv_sec = estimate / 1000000000LL;
    out->tv_nsec = estimate % 1000000000LL;
    return 0;
}
#endif
