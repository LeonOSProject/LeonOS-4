/* Run as root from a real tty2 shell; never on the host. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <leonos/fb.h>
#include <leonos/device.h>

int main(void)
{
    int report = open("/dev/ttyS0", O_WRONLY);
    assert(report >= 0);
    dprintf(report, "VT-PROBE-BEGIN\n");
    assert(isatty(0) && isatty(1) && isatty(2));
    assert(strcmp(ttyname(0), "/dev/tty2") == 0);
    assert(tcgetpgrp(0) == getpgrp());
    pid_t sid = 0;
    assert(ioctl(0, TIOCGSID, &sid) == 0 && sid == getsid(0));
    struct vt_stat state;
    int mode = -1;
    assert(ioctl(0, VT_GETSTATE, &state) == 0 && state.v_active == 2 && state.v_state == 0x7e);
    assert(ioctl(0, KDGETMODE, &mode) == 0 && mode == KD_TEXT);
    assert(ioctl(0, VT_ACTIVATE, 0) < 0 && errno == EINVAL);
    assert(ioctl(0, VT_WAITACTIVE, 7) < 0 && errno == EINVAL);
    int child = fork(), status;
    assert(child >= 0);
    if (!child) {
        assert(setsid() > 0);
        assert(setuid(1000) == 0);
        assert(ioctl(0, VT_ACTIVATE, 3) < 0 && errno == EPERM);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child && status == 0);
    int ready[2];
    assert(pipe(ready) == 0);
    child = fork();
    assert(child >= 0);
    if (!child) {
        close(ready[0]);
        assert(write(ready[1], "R", 1) == 1);
        assert(ioctl(0, VT_WAITACTIVE, 3) == 0);
        _exit(0);
    }
    close(ready[1]);
    char ch;
    assert(read(ready[0], &ch, 1) == 1);
    usleep(100000);
    assert(waitpid(child, &status, WNOHANG) == 0);
    uint64_t generation, current_generation;
    assert(ioctl(0, LEONOS_VT_GETGENERATION, &generation) == 0);
    assert(ioctl(0, VT_ACTIVATE, 3) == 0);
    assert(waitpid(child, &status, 0) == child && status == 0);
    assert(ioctl(0, VT_ACTIVATE, 2) == 0);
    assert(ioctl(0, LEONOS_VT_GETGENERATION, &current_generation) == 0);
    assert(current_generation == generation + 2);
    close(ready[0]);
    int fb = open("/dev/fb0", O_RDWR);
    assert(fb >= 0);
    struct leonos_fb_present fill = {.width = 1, .height = 1, .color = 0x123456};
    assert(ioctl(fb, LEONOS_FBIOBLIT, &fill) < 0 && errno == EAGAIN);
    assert(ioctl(0, KDSETMODE, KD_GRAPHICS) == 0);
    assert(ioctl(fb, LEONOS_FBIOBLIT, &fill) == 0);
    fill.pixels = 1; fill.stride = 1;
    assert(ioctl(fb, LEONOS_FBIOBLIT, &fill) < 0 && errno == EFAULT);
    assert(ioctl(0, KDSETMODE, KD_TEXT) == 0);
    close(fb);
    dprintf(report, "VT-PROBE-PASS session, foreground group, VT permissions, wait, framebuffer\n");
    close(report);
    return 0;
}
