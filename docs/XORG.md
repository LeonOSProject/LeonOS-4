# Xorg Desktop Backend

ReliefOS can run one of two mutually exclusive desktop backends. The default
is the native ReliefOS Desktop stack (`windowd`, `desktop.elf`, `sessiond`,
advertised to users as *ReliefOS Desktop + desktopd*); the alternative is a
minimal X11 session built from the stock Alpine `xorg-server`, `xinit`, `twm`
and `rxvt-unicode` (`urxvt`) binaries. This document describes the Xorg option, its boundaries
and how to verify it.

## Selecting the backend

The choice lives in the top-level Kconfig and is saved in
`configs/default.conf`:

```
CONFIG_DESKTOP_BACKEND_RELIEFOS=y
# CONFIG_DESKTOP_BACKEND_XORG is not set
```

`make menuconfig` (Desktop backend choice) switches between the two symbols;
they are mutually exclusive and exactly one must be enabled. A fresh
`make defconfig` always selects the native ReliefOS backend - existing builds
keep their current behavior until the choice is changed on purpose.

Staging compiles the choice into the plain single-line marker
`/etc/reliefos/desktop-backend` with the exact content `reliefos` or `xorg`.
Every reader (package staging, `console-session`) matches those two values
exactly and falls back with a logged error otherwise; the file is never
executed or sourced.

## What the Xorg backend provides

A minimal, bootable and loggable X11 session on `tty1`:

1. After `CONFIG_DESKTOP_BACKEND_XORG=y` is selected and the image is
   rebuilt, the existing `tty1` inittab entry starts the ordinary text
   `getty` and `/bin/login`; do not run `startx` manually.
2. After tty1 authentication succeeds, the login shell sources the Xorg-only
   `/etc/profile.d/reliefos-xorg.sh` hook and starts
   `/usr/lib/reliefos/reliefos-xorg-session`.
3. The wrapper checks that it owns `/dev/tty1`, then runs
   `xinit /usr/lib/reliefos/reliefos-xorg-client --
   /usr/bin/Xorg :0 -config /etc/X11/xorg.conf vt1 -keeptty`
   (`vt1` is the X server's positional VT argument; the server activates and
   waits for the VT through the standard handshake).
4. The client starts `twm` as the default desktop window manager with the
   shipped `/etc/X11/twm/twmrc` (`RandomPlacement`, core `fixed` fonts - bare
   twm would otherwise show its interactive placement outline and ask for
   Helvetica), then runs `urxvt` with the already authenticated user's
   `/bin/sh -l`. The X terminal keeps that user's uid, gid, `HOME` and
   environment; it never creates a second login prompt or a root shell.
   Windows the user starts from that shell (for example `xeyes`) are managed
   by twm on the same desktop.
5. When the authenticated Xorg session ends, `tty1` returns to the ordinary
   text login and the other VTs are unaffected.

### VT switching and input isolation

Ctrl+Alt+F1..F6 use the standard Linux VT process-ownership handshake (see
[ABI.md](ABI.md)): the kernel sends the X server its release signal and the
display switches only after `VT_RELDISP`, and switching back reacquires
through the acquire signal. While tty2-tty6 own the display, keyboard and
mouse events stay on those consoles - each evdev descriptor is scoped to the
opening session's VT, so the X session never consumes another console's
input. Leaving the authenticated X terminal shell (logout or timeout) closes
urxvt, xinit and the X server in order; the wrapper reaps any orphaned X11
process and the kernel restores the text console even if the server was killed.

Existing ReliefOS GUI applications are **not** X11 clients and are not
migrated to X11; they remain native-only. No `desktopd` daemon exists - the
name is the user-visible label of the native stack.

## Package source and offline builds

Xorg and its complete runtime dependency closure are Alpine v3.24 x86_64/musl
packages from the official `dl-cdn.alpinelinux.org` repositories. Every APK is
pinned in `configs/dependencies.lock.json` with version, URL, SHA-256 and
`.PKGINFO` license provenance, and each archive carries the Alpine RSA
signature (`alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub`).

- `make fetch` is the only network entry point. It verifies and fills
  `cache/downloads`.
- Ordinary builds consume the verified cache only and never download. Missing
  entries stop the build with the exact lock ids and the `make fetch` command
  to run.
- `make reliefos-verify-cache` performs the verify-only cache audit used
  before builds (for example by `tools/test_xorg_qemu.py`).

Xorg packages are staged only when `CONFIG_DESKTOP_BACKEND_XORG=y`. Package
selection is driven by the lock's `feature` field (`base` vs `xorg`) queried
per package - never by package-name matching - so native builds keep zero
Xorg APKs in their transaction while cached downloads stay reusable.

The local ReliefOS root remains the ABI authority where Alpine and local
products would both provide one SONAME. Alpine `musl`, `libbsd`, `libmd`,
`libuuid`, `libblkid` and `libmount` are deliberately **not** locked: the
local musl/libbsd/libmd and util-linux builds already provide those SONAMEs
(`so:libc.musl-x86_64.so.1`, `so:libbsd.so.0`, `so:libmd.so.0`,
`so:libuuid.so.1`, `so:libblkid.so.1`, `so:libmount.so.1`), and a second
provider in one rootfs is refused. Packages whose SONAME exists nowhere else
- for example `zlib` and `libpng`, which freetype/libxfont2/mesa need - are
locked normally. If a future closure pulls in another duplicate, exclude the
Alpine package and keep the local provider. The terminal is `rxvt-unicode`
with its terminfo and complete runtime closure (`glib`, `gdk-pixbuf`, `perl`
for the urxvtperl layer, `libptytty`, `startup-notification`); no `xterm`
package is locked or installed.

## Devices and ABI

The backend uses only standard Linux fbdev/evdev interfaces on fixed paths,
with no udev enumeration:

| Role | Device | Driver |
|---|---|---|
| Display | `/dev/fb0` | `xf86-video-fbdev` |
| Keyboard | `/dev/input/event0` | `xf86-input-evdev` |
| Mouse | `/dev/input/event1` | `xf86-input-evdev` |
| VT | `tty1` (VT1) | `Xorg -vt 1` |

Supported calls are documented in [ABI.md](ABI.md) (*Xorg fbdev/evdev ABI*):
`FBIOGET_VSCREENINFO`, `FBIOPUT_VSCREENINFO`, `FBIOGET_FSCREENINFO`,
`FBIOPAN_DISPLAY` plus standard `mmap` on `/dev/fb0`, and `EVIOCGNAME`,
`EVIOCGBIT`, `EVIOCG*` queries plus `poll`/`read` of `struct input_event`
records on the evdev nodes. DRM/KMS, Mesa modesetting, libinput and Wayland
are not used and not required. No ReliefOS-private ioctl is part of the Xorg
contract.

## Installer runtime

The installer runtime always keeps the native ReliefOS graphical session,
whatever desktop backend the installed system selects: `installer-stage`
restores the `reliefos` marker and the native default runlevel links before
packaging the runtime root. Xorg installs only remove the `reliefos-windowd`
and `reliefos-session` default runlevel links from the installed system; the
native service scripts and ELF programs stay in place.

## Session diagnostics

- `/var/log/xorg-session.log` - tty1 authentication handoff, session wrapper
  milestones, server start, twm window manager, urxvt client, exit status and
  text-login restore.
- `/var/log/Xorg.0.log` - the Xorg server's own log.
- `/var/log/desktop.log` - native session log; it stays silent in Xorg mode.

## QEMU verification

Build the Xorg image and verify the whole session in a real QEMU run:

```sh
make fetch                                   # once, to fill the cache
python3 tools/test_xorg_qemu.py --output build/xorg-qemu
```

Options: `--cache` selects the verified download cache, `--qemu` the QEMU
binary, `--boot-timeout` the startup budget, `--skip-build` reuses an
existing `out/xorg-qemu` image. The script:

1. generates `.config` from the defaults, switches only the desktop backend
   to `CONFIG_DESKTOP_BACKEND_XORG=y` and re-runs `olddefconfig`;
2. audits the cache verify-only (missing entries name their lock ids and the
   `make fetch` fix, nothing is downloaded);
3. builds `out/xorg-qemu` (isolated from the default output tree);
4. compiles `tools/tests/xorg_device_probe.c` with the musl SDK and installs
   it into the test disk;
5. boots the image under UEFI/OVMF with VGA, serial and QMP, verifies the
   tty1 text login, authenticates there, waits for the X11 frame, exercises mouse
   input, the Ctrl+Alt+F2/F1 VT round trip with per-console input isolation
   (a marker typed on tty2 must never reach the X terminal), repeated switch
   stress, runs the device probe and leaves the session;
6. collects the serial log, `xorg-session.log`, `Xorg.0.log`, the captured
   X-terminal input log and the probe log, and checks: *Xorg server started*,
   *urxvt client started*, *authenticated tty1 login starting Xorg*,
   *native windowd/sessiond not started*, mouse/keyboard delivery, VT switch and isolation,
   and that Xorg exit restores the tty1 text login. Screenshots are captured
   via QMP and rejected when black or blank.

All artifacts (screenshots, serial log, guest logs, probe output) are kept in
`--output` for inspection, including on failure.

## Not yet verified

- Any physical display path other than the QEMU VGA/fbdev device: real GPU
  framebuffers, DRM/KMS drivers, multi-head and hot-plugged input devices are
  out of scope and untested.
- Suspend/resume while Xorg runs (VT switching itself is covered by the QEMU
  round-trip and stress checks above).
- Localized keyboards beyond the default keymap from `xkeyboard-config`.
- The native ReliefOS desktop is not offered as an X11 client set; only the
   minimal urxvt/login session is verified.
