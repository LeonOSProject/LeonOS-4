#ifndef RELIEFOS_CMD_SYS_IOCTL_H
#define RELIEFOS_CMD_SYS_IOCTL_H

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
};

int ioctl(int fd, unsigned long request, void *arg);

#define TIOCGWINSZ 0x5413UL

#endif
