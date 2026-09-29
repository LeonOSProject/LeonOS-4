#include <reliefos/blockdev.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>

static int parse_disk(const char *path)
{
    const char *p;
    if (!path || strncmp(path, "/dev/", 5) != 0) return -1;
    p = path + 5;
    if (p[0] == 's' && p[1] == 'd' && p[2] >= 'a' && p[2] <= 'z') {
        p += 2;
        while (*p >= 'a' && *p <= 'z') ++p;
        return *p ? -1 : 0;
    }
    if (strncmp(p, "nvme", 4) != 0) return -1;
    p += 4;
    if (*p < '0' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') ++p;
    if (*p++ != 'n' || *p < '1' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') ++p;
    return *p ? -1 : 0;
}

static int confirm_initialization(const char *path)
{
    char line[16];
    printf("This will replace GPT metadata on %s. Type YES to continue: ", path);
    if (!fgets(line, sizeof(line), stdin)) {
        return 0;
    }
    line[strcspn(line, "\r\n")] = 0;
    return strcmp(line, "YES") == 0;
}

int main(int argc, char **argv)
{
    const char *path;
    int force = 0;
    int ret;

    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--force") == 0) {
        path = argv[2];
        force = 1;
    } else {
        puts("usage: gptinit [--force] /dev/sdX or /dev/nvmeXnY");
        return 2;
    }
    if (parse_disk(path) < 0) {
        puts("gptinit: expected a whole disk such as /dev/sda or /dev/nvme0n1");
        return 2;
    }
    if (!force && !confirm_initialization(path)) {
        puts("gptinit: cancelled");
        return 1;
    }
    ret = reliefos_block_gpt_initialize(path, force);
    if (ret < 0) {
        printf("gptinit: GPT initialization failed for %s (ret=%d)\n", path, ret);
        if (ret == -17) {
            puts("gptinit: disk already has a valid GPT; use --force after checking the device");
        }
        return 1;
    }
    printf("gptinit: initialized an empty GPT on %s\n", path);
    puts("Create partitions next with fdisk.");
    return 0;
}
