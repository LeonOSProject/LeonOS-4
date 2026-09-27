/* Guest probe for the text-VM re-render boundary. Run as root from a real
 * tty shell; never on the host. Manual verification (e.g. in VMware):
 *   python3 tools/prepare_vt_fixture.py --probe-source tools/tests/vt_strip_probe.c \
 *       --output /tmp/vt-strip-fixture
 *   boot /tmp/vt-strip-fixture/disk.raw, log in on a tty, run /tmp/vt-probe,
 *   then look at the bottom 8px strip of the screen: red = the switch kept
 *   the graphical picture (bug), black = the text repaint covered it (fixed).
 *
 * The probe manufactures the "desktop remnants": fill the whole scanout red
 * in a graphical VT, then switch the controlling VT back to text. The text
 * console repaint must cover the whole scanout (black), not only the
 * character grid: the strip below the last font row is text-unreachable
 * (600 / 16 = 37 rows, bottom 8px) and used to keep the graphical picture.
 * QEMU cannot observe this: its vmware-svga emulation never surfaces guest
 * scanout updates (even the boot log stays black), so the check is visual on
 * a real VMware display. */
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <leonos/fb.h>

int main(void)
{
    int report = open("/dev/ttyS0", O_WRONLY);
    assert(report >= 0);
    dprintf(report, "VT-STRIP-PROBE-BEGIN\n");
    assert(isatty(0) && isatty(1) && isatty(2));
    int fb = open("/dev/fb0", O_RDWR);
    assert(fb >= 0);
    assert(ioctl(0, KDSETMODE, KD_GRAPHICS) == 0);
    struct fb_var_screeninfo info = {0};
    assert(ioctl(fb, FBIOGET_VSCREENINFO, &info) == 0);
    /* 600 is not a multiple of the 16px font: the bottom 8 scan lines are
     * text-unreachable and must still be repainted black on the switch. */
    info.xres = 1024;
    info.yres = 600;
    assert(ioctl(fb, FBIOPUT_VSCREENINFO, &info) == 0);
    struct leonos_fb_present fill = {
        .x = 0, .y = 0, .width = 1024, .height = 600, .color = 0xFF0000u,
    };
    assert(ioctl(fb, LEONOS_FBIOBLIT, &fill) == 0);
    dprintf(report, "VT-STRIP-PAINTED\n");
    assert(ioctl(0, KDSETMODE, KD_TEXT) == 0);
    dprintf(report, "VT-STRIP-READY\n");
    for (;;) pause();
    return 0;
}
