# Xorg + TWM + XDM

ReliefOS has two mutually exclusive desktop backends in Kconfig. The default
is `CONFIG_DESKTOP_BACKEND_RELIEFOS=y`, which keeps the existing `windowd`,
`desktop.elf` and `sessiond` startup. Selecting
`CONFIG_DESKTOP_BACKEND_XORG=y` stages the Alpine official Xorg, TWM and XDM
packages and writes the raw-root marker `xorg`.

The Xorg backend is intentionally small. tty1's existing `console-session`
runs `/usr/lib/reliefos/reliefos-xdm` in the foreground. XDM starts
`/usr/bin/Xorg :0 -config /etc/X11/xorg.conf vt1 -keeptty`; the fixed config
uses `/dev/fb0`, `/dev/input/event0` and `/dev/input/event1`. After PAM accepts
credentials, `/usr/lib/reliefos/xdm-session` runs as the authenticated user,
starts `xterm`, and keeps `twm` in the foreground. Exiting TWM returns control
to XDM; exiting XDM lets tty1's console session fall back to text login. tty2
through tty6 remain text consoles.

Packages come from the pinned Alpine v3.24 x86_64/musl main and community
indexes. The lock file marks only the Xorg closure entries with `feature=xorg`.
`make fetch` is the only network operation; ordinary builds use the verified
cache and report the missing dependency ID when an archive is absent. Native
staging selects only `feature=base` entries, while Xorg staging selects both
features and preserves the APK database, signatures and ownership manifests.

The XDM PAM service includes the existing `common-auth`, `common-account` and
`common-session` policy. The user session writes `$HOME/.xsession-errors` (or a
user cache fallback); it never writes root-only logs or records passwords and
authorization data. Installer runtime roots deliberately restore the native
marker, inittab and OpenRC links, while the installed target root keeps the
user-selected Xorg policy.

Xorg consumes public Linux/POSIX interfaces only. ReliefNT exposes `/dev/tty0`
as the active VT and implements `VT_OPENQRY`, `VT_GETMODE/VT_SETMODE`,
`VT_RELDISP`, `KDGKBMODE`, `KDSKBMODE`, and the existing VT/KD calls. The
Xorg ABI probe is `python3 tools/test_xorg_abi.py --source-only`; kernel
compilation and subrepository ABI tests are separate checks.

For QEMU acceptance, capture serial/QMP evidence and run:

```sh
python3 tools/test_xorg_qemu.py --image out/xorg-test/images/reliefos.vmdk \
  --log out/xorg-test/qemu/xorg.log
```

The log must show `desktop-backend=xorg`, XDM on vt1, accepted PAM
authentication, a non-root TWM session, xterm, session exit and tty1 text
recovery in that order. This contract does not certify physical GPUs or
hardware-specific fbdev modes; those require a platform run with captured
evidence.
