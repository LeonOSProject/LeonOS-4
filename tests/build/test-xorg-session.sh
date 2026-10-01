#!/bin/sh
# Contract tests for the minimal Xorg session: fixed fbdev/evdev configuration,
# POSIX session wrappers, and the console-session desktop backend branches.
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d "${TMPDIR:-/tmp}/reliefos-xorg-session.XXXXXX")
trap 'rm -rf "$w"' EXIT HUP INT TERM

fail() { echo "FAIL - $*" >&2; exit 1; }
expect_grep() { grep -q "$2" "$1" || fail "$1 lacks: $2"; }
expect_no_grep() { ! grep -q "$2" "$1" || fail "$1 still has: $2"; }
expect_order() {
    file=$1 first=$2 second=$3
    before=$(grep -n "$first" "$file" | head -n 1 | cut -d: -f1)
    after=$(grep -n "$second" "$file" | head -n 1 | cut -d: -f1)
    [ -n "$before" ] && [ -n "$after" ] && [ "$before" -lt "$after" ] ||
        fail "$first must precede $second in $file"
}

conf=$root/system/xorg/xorg.conf
session=$root/system/xorg/reliefos-xorg-session
client=$root/system/xorg/reliefos-xorg-client
twmrc=$root/system/xorg/twmrc
console=$root/system/rootfs/usr/lib/reliefos/console-session
inittab=$root/system/rootfs/etc/inittab
profile=$root/system/xorg/reliefos-xorg-profile

# --- Xorg configuration pins standard Linux fbdev/evdev devices ---
expect_grep "$conf" 'Driver[[:space:]]*"fbdev"'
expect_grep "$conf" '/dev/fb0'
expect_grep "$conf" '/dev/input/event0'
expect_grep "$conf" '/dev/input/event1'
expect_grep "$conf" 'Driver[[:space:]]*"evdev"'
expect_grep "$conf" 'AutoAddDevices'
expect_grep "$conf" 'tty1'
# Active configuration (comments excluded) must not reach for DRM/KMS/Mesa
# modesetting, libinput, Wayland or udev enumeration.
! sed 's/#.*//' "$conf" | grep -qi 'libinput\|drm\|kms\|modesetting\|wayland\|udev' ||
    fail "$conf must not depend on DRM/KMS/Mesa/libinput/Wayland/udev"

# --- session and client keep the xinit -> Xorg -> urxvt -> shell contract ---
expect_grep "$session" 'xinit'
expect_grep "$session" '/usr/bin/xinit /usr/lib/reliefos/reliefos-xorg-client --'
expect_grep "$session" '/usr/bin/Xorg :0 -config /etc/X11/xorg.conf vt1 -keeptty'
expect_no_grep "$session" 'novtswitch'
expect_grep "$session" '/dev/tty1'
expect_grep "$profile" '/dev/tty1'
expect_grep "$profile" '/etc/reliefos/desktop-backend'
expect_grep "$profile" 'reliefos-xorg-session'
expect_no_grep "$profile" '/bin/login'
expect_grep "$client" '/usr/bin/twm'
expect_grep "$client" 'urxvt'
! grep -q 'xterm' "$client" || fail "$client must not use xterm"
expect_grep "$conf" 'GrabDevice.*"true"'
expect_grep "$conf" 'Emulate3Buttons.*"false"'
expect_no_grep "$client" '/bin/login'
expect_grep "$client" '/bin/sh -l'
expect_order "$client" 'twm window manager started' 'urxvt client started'
# twm without a config shows an interactive placement outline and wants
# Helvetica fonts; the shipped twmrc must avoid both.
expect_grep "$client" '/etc/X11/twm/twmrc'
expect_grep "$twmrc" 'RandomPlacement'
expect_grep "$twmrc" 'MenuFont[[:space:]]*"fixed"'
! sed 's/#.*//' "$twmrc" | grep -qi 'helvetica' ||
    fail "$twmrc must not require Helvetica fonts"
expect_no_grep "$client" 'exec /bin/login'
expect_no_grep "$client" 'graphical-session'
for script in "$session" "$client" "$console" "$profile"; do
    [ "$(head -n 1 "$script")" = '#!/bin/sh' ] || fail "$script must use /bin/sh"
    sh -n "$script" || fail "$script has invalid POSIX shell syntax"
done

# --- console-session branches: installer first, then exact backend marker ---
expect_grep "$console" '/etc/reliefos/desktop-backend'
expect_grep "$console" 'missing or invalid'
expect_no_grep "$console" '/etc/reliefos/desktop-session'
expect_order "$console" 'installer-runtime' 'desktop-backend'
expect_no_grep "$console" 'reliefos-xorg-session >>'
expect_grep "$inittab" 'tty1::respawn:/usr/lib/reliefos/console-session tty1'
expect_no_grep "$console" 'startx'

# --- behavioral branches run the real console-session in a private root ---
unshare -Ur true >/dev/null 2>&1 || fail 'user namespaces are required for the boot branch tests'

make_fixture() {
    fx=$1
    mkdir -p "$fx/bin" "$fx/usr/lib/reliefos/apps/login" "$fx/sbin" \
        "$fx/etc/reliefos" "$fx/run/reliefos" "$fx/var/log" "$fx/root"
    cp /bin/sh "$fx/bin/sh"
    ldd /bin/sh | while IFS= read -r line; do
        for word in $line; do
            case $word in /*) cp --parents "$word" "$fx" ;; esac
        done
    done
    cp "$console" "$fx/usr/lib/reliefos/console-session"
    printf '#!/bin/sh\necho "login.elf $*" >> /var/log/calls.log\n' \
        > "$fx/usr/lib/reliefos/apps/login/login.elf"
    printf '#!/bin/sh\necho "xorg-session $*" >> /var/log/calls.log\n' \
        > "$fx/usr/lib/reliefos/reliefos-xorg-session"
    printf '#!/bin/sh\necho "getty $*" >> /var/log/calls.log\n' > "$fx/sbin/getty"
    chmod 755 "$fx/usr/lib/reliefos/apps/login/login.elf" \
        "$fx/usr/lib/reliefos/reliefos-xorg-session" "$fx/sbin/getty"
}

run_console() {
    unshare -Ur chroot "$1" /bin/sh /usr/lib/reliefos/console-session tty1 \
        > "$1/console.out" 2>&1 || true
}

# reliefos backend: the native graphical session, then text login.
fx=$w/reliefos
make_fixture "$fx"
printf 'reliefos\n' > "$fx/etc/reliefos/desktop-backend"
run_console "$fx"
expect_grep "$fx/var/log/calls.log" 'login.elf --graphical-session'
expect_grep "$fx/var/log/calls.log" 'getty'
expect_no_grep "$fx/var/log/calls.log" 'xorg-session'
expect_order "$fx/var/log/calls.log" 'login.elf --graphical-session' 'getty'

# xorg backend: tty1 first authenticates through getty. The authenticated
# login shell starts Xorg from the profile.
fx=$w/xorg
make_fixture "$fx"
printf 'xorg\n' > "$fx/etc/reliefos/desktop-backend"
run_console "$fx"
expect_grep "$fx/var/log/calls.log" 'getty'
expect_no_grep "$fx/var/log/calls.log" 'xorg-session'

# installer runtime: the native path wins over the installed system marker.
fx=$w/installer
make_fixture "$fx"
printf 'xorg\n' > "$fx/etc/reliefos/desktop-backend"
printf 'installer\n' > "$fx/etc/reliefos/installer-runtime"
run_console "$fx"
expect_grep "$fx/var/log/calls.log" 'login.elf --graphical-session'
expect_grep "$fx/var/log/calls.log" 'login.elf --installer-shell'
expect_no_grep "$fx/var/log/calls.log" 'xorg-session'

# unknown, empty, extra-content and missing markers log an error and stay
# in text login without starting either graphical backend.
newline='
'
case_number=0
for marker in 'bogus' '' "reliefos${newline}xorg${newline}" 'reliefos extra' '@@missing@@'; do
    case_number=$((case_number + 1))
    fx=$w/marker$case_number
    make_fixture "$fx"
    [ "$marker" = '@@missing@@' ] || printf '%s' "$marker" > "$fx/etc/reliefos/desktop-backend"
    run_console "$fx"
    expect_grep "$fx/var/log/calls.log" 'getty'
    expect_no_grep "$fx/var/log/calls.log" 'xorg-session'
    expect_no_grep "$fx/var/log/calls.log" 'graphical-session'
    expect_grep "$fx/var/log/desktop.log" 'missing or invalid'
done

printf '%s\n' 'xorg session: fixed fbdev/evdev contract and desktop backend branches pass'
