#ifndef LEONOS_MOTD_FORMAT_H
#define LEONOS_MOTD_FORMAT_H
#include <stdio.h>
struct motd_info {
    char system[128], kernel[256], timestamp[128], uptime[96];
    char load[32], memory[32], root[32], address[128];
};
void motd_wrap(FILE *output, const char *text, size_t columns);
void motd_render(FILE *output, const struct motd_info *info, int chinese, size_t columns);
#endif
