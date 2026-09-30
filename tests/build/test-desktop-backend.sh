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

hostcc=${HOSTCC:-cc}
mkdir -p "$work/src/system" "$work/out"
cp -a "$root/system/rootfs" "$work/src/system/rootfs"
for name in certs config docs fonts resources test-accounts xorg; do
    ln -s "$root/system/$name" "$work/src/system/$name"
done
for name in configs docs resources test third_party tools userland; do
    ln -s "$root/$name" "$work/src/$name"
done
cp "$root/logo.png" "$work/src/logo.png"

mkdir -p "$work/out/upstream/sudo/root/usr/bin" \
    "$work/out/upstream/shadow/root/usr/bin" \
    "$work/out/upstream/util-linux/root/bin" \
    "$work/out/pam/root/sbin" \
    "$work/out/sysroot/musl/lib" \
    "$work/out/sysroot/musl/share/licenses/musl" \
    "$work/out/sysroot/musl/share/licenses/mimalloc" \
    "$work/out/system/lib" "$work/out/generated/system" \
    "$work/out/generated/drivers" "$work/out/generated/fonts" \
    "$work/out/userland"
printf '%s\n' fixture > "$work/out/upstream/sudo/root/usr/bin/sudo"
printf '%s\n' fixture > "$work/out/upstream/shadow/root/usr/bin/passwd"
printf '%s\n' fixture > "$work/out/upstream/util-linux/root/bin/su"
printf '%s\n' fixture > "$work/out/pam/root/sbin/unix_chkpwd"
printf '%s\n' fixture > "$work/out/sysroot/musl/lib/libc.so"
printf '%s\n' fixture > "$work/out/sysroot/musl/lib/libmimalloc.so.3"
printf '%s\n' fixture > "$work/out/sysroot/musl/share/licenses/musl/LICENSE"
printf '%s\n' fixture > "$work/out/sysroot/musl/share/licenses/mimalloc/LICENSE"
printf '%s\n' fixture > "$work/out/system/lib/libreliefos.so.2"
printf '%s\n' fixture > "$work/out/system/lib/libleonos.so.2"
printf '%s\n' fixture > "$work/out/generated/system/kerneldebug.sys"
printf '%s\n' fixture > "$work/out/generated/drivers/fixture.drv"
printf '%s\n' fixture > "$work/out/generated/fonts/leonos-metro.ttf"
printf '%s\n' fixture > "$work/out/generated/fonts/leonos-win95.ttf"
printf '%s\n' fixture > "$work/out/userland/motd.elf"
printf '%s\n' fixture > "$work/out/userland/dynlinkerror.elf"
for locale in $(cat "$root/configs/nls/LINGUAS"); do
    mkdir -p "$work/out/generated/nls/$locale/LC_MESSAGES"
    printf '%s\n' fixture > "$work/out/generated/nls/$locale/LC_MESSAGES/leonos.mo"
done
printf '%s\n' 'component metadata fixture' > "$work/metadata"

"$hostcc" -std=c11 -Wall -Wextra -Werror -Wpedantic -I"$root" \
    "$root/tools/host/manifest/reliefos-stage.c" \
    "$root/tools/host/common/io.c" "$root/tools/host/common/buffer.c" \
    "$root/tools/host/manifest/json.c" -o "$work/stage"
"$hostcc" -std=c11 -Wall -Wextra -Werror -Wpedantic -I"$root" \
    "$root/tools/host/manifest/reliefos-layout.c" -o "$work/layout"

run_rootfs_stage() {
    backend=$1
    config=$work/$backend.config
    dest=$work/$backend-root
    manifest=$work/$backend-manifest.json
    cat > "$config" <<CONFIG
CONFIG_DESKTOP_BACKEND_RELIEFOS=$(test "$backend" = reliefos && printf y || printf n)
CONFIG_DESKTOP_BACKEND_XORG=$(test "$backend" = xorg && printf y || printf n)
CONFIG_RPR_BASE_URL="https://example.invalid/rpr"
CONFIG_VMDK_DEFAULT_LANG="zh_CN.UTF-8"
CONFIG
    sh "$root/tools/build/rootfs-stage.sh" "$work/src" "$work/out" \
        "$config" "$work/metadata" "$work/stage" "$work/layout" \
        "$dest" "$manifest" 1700000000
    printf '%s\n' "$dest"
}

reliefos_root=$(run_rootfs_stage reliefos)
xorg_root=$(run_rootfs_stage xorg)
grep -qx 'reliefos' "$reliefos_root/etc/reliefos/desktop-backend"
grep -qx 'xorg' "$xorg_root/etc/reliefos/desktop-backend"
test -L "$reliefos_root/etc/runlevels/default/reliefos-windowd"
test -L "$reliefos_root/etc/runlevels/default/reliefos-session"
test ! -e "$xorg_root/etc/runlevels/default/reliefos-windowd"
test ! -e "$xorg_root/etc/runlevels/default/reliefos-session"
test -f "$xorg_root/etc/X11/xorg.conf"
test -x "$xorg_root/usr/lib/reliefos/reliefos-xorg-session"
test -x "$xorg_root/usr/lib/reliefos/reliefos-xorg-client"
test ! -e "$reliefos_root/etc/X11/xorg.conf"
test ! -e "$reliefos_root/usr/lib/reliefos/reliefos-xorg-session"
test ! -e "$reliefos_root/usr/lib/reliefos/reliefos-xorg-client"

cat > "$work/invalid.config" <<'CONFIG'
CONFIG_DESKTOP_BACKEND_RELIEFOS=n
CONFIG_DESKTOP_BACKEND_XORG=n
CONFIG_RPR_BASE_URL="https://example.invalid/rpr"
CONFIG_VMDK_DEFAULT_LANG="zh_CN.UTF-8"
CONFIG
if sh "$root/tools/build/rootfs-stage.sh" "$work/src" "$work/out" \
    "$work/invalid.config" "$work/metadata" "$work/stage" "$work/layout" \
    "$work/invalid-root" "$work/invalid-manifest.json" 1700000000 \
    >/dev/null 2>&1; then
    echo 'rootfs-stage accepted an invalid desktop backend' >&2
    exit 1
fi

printf '%s\n' 'desktop backend kconfig: default and mutually exclusive xorg selection pass'
