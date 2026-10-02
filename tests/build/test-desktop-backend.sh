#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root"
work=${TMPDIR:-/tmp}/reliefos-desktop-backend.$$
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work"

grep -q '^config DESKTOP_BACKEND_RELIEFOS$' Kconfig
grep -q '^config DESKTOP_BACKEND_XORG$' Kconfig
grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' configs/default.conf
! grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' configs/default.conf

make -s O="$work/default" defconfig
grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' "$work/default/config/.config"
! grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' "$work/default/config/.config"

mkdir -p "$work/xorg/config"
printf '%s\n' 'CONFIG_DESKTOP_BACKEND_XORG=y' > "$work/xorg/config/.config"
make -s O="$work/xorg" olddefconfig
grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' "$work/xorg/config/.config"
! grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' "$work/xorg/config/.config"

printf '%s\n' 'ok - desktop backend Kconfig choice'
