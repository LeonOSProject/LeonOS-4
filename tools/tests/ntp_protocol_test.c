#include <assert.h>
#include <stdio.h>
#include "../../userland/runtime/src/ntp_protocol.h"
int main(void)
{
    unsigned char request[48] = {0}, response[48] = {0};
    ntp_put32(request + 40, 12345); ntp_put32(request + 44, 67890);
    response[0] = 0x24; response[1] = 2;
    memcpy(response + 24, request + 40, 8);
    const int64_t reference = 2208988800LL; /* 2040: after the NTP era rollover. */
    ntp_put32(response + 32, (uint32_t)(reference + 2208988800LL));
    ntp_put32(response + 40, (uint32_t)(reference + 2208988800LL));
    struct timespec result;
    assert(ntp_decode(response, 48, request, reference, 200000000, &result) == 0);
    assert(result.tv_sec == reference && result.tv_nsec == 100000000);
    response[24] ^= 1;
    assert(ntp_decode(response, 48, request, reference, 0, &result) < 0);
    response[24] ^= 1;
    for (unsigned mode = 0; mode < 8; ++mode) {
        response[0] = 0x20 | mode;
        assert((ntp_decode(response, 48, request, reference, 0, &result) == 0) == (mode == 4));
    }
    response[0] = 0xe4;
    assert(ntp_decode(response, 48, request, reference, 0, &result) < 0);
    response[0] = 0x24; response[1] = 0;
    assert(ntp_decode(response, 48, request, reference, 0, &result) < 0);
    response[1] = 16;
    assert(ntp_decode(response, 48, request, reference, 0, &result) < 0);
    response[1] = 2;
    for (unsigned len = 0; len < 48; ++len) assert(ntp_decode(response, len, request, reference, 0, &result) < 0);
    puts("PASS NTP origin, mode, stratum, leap, short packets, era unfolding and delay calculation");
}
