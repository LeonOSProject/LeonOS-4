/* Convert sorted GNU Unifont HEX records into an immutable kernel lookup. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned hex(char c)
{
    if (c >= '0' && c <= '9') return (unsigned)(c - '0');
    if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A') + 10U;
    if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a') + 10U;
    fprintf(stderr, "cjk-font: invalid hex digit\n");
    exit(1);
}

int main(void)
{
    char line[256];
    uint32_t previous = 0;
    unsigned count = 0;
    puts("/* Generated from GNU Unifont; see resources/licenses/unifont-LICENSE. */");
    puts("static const struct { uint32_t cp; uint8_t width; uint8_t bits[32]; } cjk_glyphs[] = {");
    while (fgets(line, sizeof(line), stdin)) {
        char *colon = strchr(line, ':');
        uint32_t cp = 0;
        size_t digits;
        if (!colon || colon - line < 4 || colon - line > 6) return 1;
        for (char *p = line; p < colon; p++) cp = cp * 16U + hex(*p);
        digits = strcspn(colon + 1, "\r\n");
        if ((digits != 32 && digits != 64) || cp > 0x10ffffU ||
            (count && cp <= previous)) return 1;
        previous = cp;
        count++;
        printf("{0x%x,%u,{", cp, digits == 64 ? 2U : 1U);
        for (size_t i = 0; i < digits; i += 2)
            printf("0x%02x,", hex(colon[1+i]) * 16U + hex(colon[2+i]));
        puts("}},");
    }
    puts("};");
    return !count || ferror(stdin) || fflush(stdout) || ferror(stdout);
}
