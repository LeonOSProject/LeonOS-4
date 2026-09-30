#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
work=$(mktemp -d "${TMPDIR:-/tmp}/reliefos-desktop-backend.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

grep -q '^config DESKTOP_BACKEND_RELIEFOS$' "$root/Kconfig"
grep -q '^config DESKTOP_BACKEND_XORG$' "$root/Kconfig"
grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' "$root/configs/default.conf"
! grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' "$root/configs/default.conf"

make -s -C "$root" O="$work/default" defconfig >/dev/null
grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' "$work/default/config/.config"
! grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' "$work/default/config/.config"

mkdir -p "$work/xorg/config"
sed 's/^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$/# CONFIG_DESKTOP_BACKEND_RELIEFOS is not set/' \
    "$work/default/config/.config" > "$work/xorg/config/.config"
printf '%s\n' 'CONFIG_DESKTOP_BACKEND_XORG=y' >> "$work/xorg/config/.config"
make -s -C "$root" O="$work/xorg" olddefconfig >/dev/null
grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' "$work/xorg/config/.config"
! grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' "$work/xorg/config/.config"

printf '%s\n' 'desktop backend kconfig: default and mutually exclusive xorg selection pass'
