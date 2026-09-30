/* Standard Linux fbdev/evdev probe for the ReliefOS Xorg desktop backend.
 *
 * Every operation here is public Linux UAPI (<linux/fb.h>, <linux/input.h>)
 * and POSIX (open, ioctl, poll, read) on the fixed device paths /dev/fb0,
 * /dev/input/event0 and /dev/input/event1. No ReliefOS-private framebuffer or
 * VT ioctl is used or required: this probe exists to prove the Xorg session
 * stack works through the standard interfaces alone. Run it in the target
 * guest; each step reports its own result and errno diagnostics.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static unsigned int failures;

static void step_ok(const char *id, const char *detail)
{
    if (detail != NULL) printf("xorg-probe ok %s %s\n", id, detail);
    else printf("xorg-probe ok %s\n", id);
}

static void step_fail(const char *id, int error)
{
    if (error != 0) printf("xorg-probe fail %s errno=%d (%s)\n", id, error, strerror(error));
    else printf("xorg-probe fail %s (assertion failed)\n", id);
    failures++;
}

static void query_evdev(int fd, const char *tag, unsigned int wanted_types)
{
    char name[256];
    unsigned char evbits[(EV_MAX + 1) / 8];
    char detail[320];
    char id[96];
    unsigned int found = 0u;
    size_t i;

    memset(name, 0, sizeof name);
    snprintf(id, sizeof id, "evdev-EVIOCGNAME-%s", tag);
    if (ioctl(fd, EVIOCGNAME(sizeof name), name) < 0) {
        step_fail(id, errno);
    } else {
        snprintf(detail, sizeof detail, "name=%s", name);
        step_ok(id, detail);
    }

    memset(evbits, 0, sizeof evbits);
    snprintf(id, sizeof id, "evdev-EVIOCGBIT-%s", tag);
    if (ioctl(fd, EVIOCGBIT(0, (int)sizeof evbits), evbits) < 0) {
        step_fail(id, errno);
        return;
    }
    for (i = 0; i < sizeof evbits; ++i) found |= (unsigned int)evbits[i] << (8u * i);
    snprintf(detail, sizeof detail, "event-types=0x%x", found);
    if ((found & wanted_types) == 0u) {
        step_fail(id, 0);
        printf("xorg-probe info %s missing-event-types wanted=0x%x %s\n", tag, wanted_types, detail);
        return;
    }
    step_ok(id, detail);
}

static void probe_reads(int fd, const char *tag)
{
    struct input_event events[16];
    char detail[96];
    char id[96];
    ssize_t got;

    memset(events, 0, sizeof events);
    snprintf(id, sizeof id, "evdev-read-%s", tag);
    got = read(fd, events, sizeof events);
    if (got < 0 && errno == EAGAIN) {
        step_ok(id, "no pending events (EAGAIN)");
        return;
    }
    if (got > 0 && (size_t)got % sizeof events[0] == 0u) {
        snprintf(detail, sizeof detail, "events=%zu", (size_t)got / sizeof events[0]);
        step_ok(id, detail);
        return;
    }
    if (got < 0) {
        step_fail(id, errno);
        return;
    }
    step_fail(id, 0);
    printf("xorg-probe info %s partial-record bytes=%zu record=%zu\n", tag,
           (size_t)got, sizeof events[0]);
}

int main(void)
{
    struct fb_var_screeninfo vary;
    struct fb_fix_screeninfo fix;
    struct pollfd fds[2];
    char detail[320];
    int fb, key, mouse, ready;

    fb = open("/dev/fb0", O_RDWR);
    if (fb < 0) {
        step_fail("fbdev-open-fb0", errno);
    } else {
        step_ok("fbdev-open-fb0", NULL);
        memset(&vary, 0, sizeof vary);
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vary) < 0) {
            step_fail("fbdev-FBIOGET_VSCREENINFO", errno);
        } else {
            snprintf(detail, sizeof detail, "%ux%u %ubpp", vary.xres, vary.yres,
                     vary.bits_per_pixel);
            step_ok("fbdev-FBIOGET_VSCREENINFO", detail);
        }
        memset(&fix, 0, sizeof fix);
        if (ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0) {
            step_fail("fbdev-FBIOGET_FSCREENINFO", errno);
        } else {
            snprintf(detail, sizeof detail, "id=%.16s smem_len=%u line_length=%u",
                     fix.id, fix.smem_len, fix.line_length);
            step_ok("fbdev-FBIOGET_FSCREENINFO", detail);
        }
        close(fb);
    }

    if (offsetof(struct input_event, type) == sizeof(struct timeval) &&
        offsetof(struct input_event, code) == sizeof(struct timeval) + 2u &&
        offsetof(struct input_event, value) == sizeof(struct timeval) + 4u &&
        sizeof(struct input_event) == sizeof(struct timeval) + 8u) {
        snprintf(detail, sizeof detail, "sizeof=%zu timeval=%zu",
                 sizeof(struct input_event), sizeof(struct timeval));
        step_ok("evdev-input-event-layout", detail);
    } else {
        step_fail("evdev-input-event-layout", 0);
    }

    key = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (key < 0) {
        step_fail("evdev-open-event0", errno);
    } else {
        step_ok("evdev-open-event0", NULL);
        query_evdev(key, "event0", 1u << EV_KEY);
    }
    mouse = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    if (mouse < 0) {
        step_fail("evdev-open-event1", errno);
    } else {
        step_ok("evdev-open-event1", NULL);
        query_evdev(mouse, "event1",
                    (1u << EV_REL) | (1u << EV_ABS) | (1u << EV_KEY));
    }

    if (key >= 0 && mouse >= 0) {
        fds[0].fd = key;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = mouse;
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        ready = poll(fds, 2, 250);
        if (ready < 0) {
            step_fail("evdev-poll", errno);
        } else if (ready == 0) {
            step_ok("evdev-poll", "timeout with no pending events");
        } else {
            snprintf(detail, sizeof detail, "ready=%d revents=%d/%d", ready,
                     fds[0].revents, fds[1].revents);
            step_ok("evdev-poll", detail);
        }
        if (key >= 0) probe_reads(key, "event0");
        if (mouse >= 0) probe_reads(mouse, "event1");
    }
    if (key >= 0) close(key);
    if (mouse >= 0) close(mouse);

    printf("xorg-probe result failures=%u\n", failures);
    return failures == 0u ? 0 : 1;
}
