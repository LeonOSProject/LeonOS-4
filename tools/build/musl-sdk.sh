#!/bin/sh
# Build a relocatable musl SDK with the canonical and published compatibility ABIs.
set -eu
[ "$#" = 12 ] || { echo 'usage: musl-sdk.sh SRC SYSROOT RELIEFOS_RUNTIME RELIEFOS_ARCHIVE LEONOS_RUNTIME LEONOS_ARCHIVE PAM AUTH DRIVER STAGE EPOCH EXPORT' >&2; exit 2; }
src=$1 sysroot=$2 reliefos_runtime=$3 reliefos_archive=$4 leonos_runtime=$5 leonos_archive=$6
pam=$7 auth=$8 driver=$9 stage=${10} epoch=${11} export=${12}
mkdir -p "$(dirname "$stage")"
tmp=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cp -a "$sysroot/include" "$sysroot/lib" "$sysroot/share" "$tmp/"
# UAPI comes from the kernel header export (headers_install), never from the
# kernel-owned source tree: the SDK must consume installed ABI, not sources.
cp -a "$export/." "$tmp/include/"
mkdir -p "$tmp/include/leonos" "$tmp/include/reliefos" "$tmp/bin"
cp -a "$src/include/leonos/." "$src/userland/runtime/include/leonos/." "$tmp/include/leonos/"
cp -a "$src/include/reliefos/." "$src/userland/runtime/include/reliefos/." "$tmp/include/reliefos/"
cp -a "$pam/usr/include/security" "$tmp/include/"
cp "$auth/usr/include/crypt.h" "$tmp/include/"
cp -P "$pam"/lib/libpam*.so* "$auth"/lib/libcrypt.so* "$tmp/lib/"
cp "$reliefos_runtime" "$tmp/lib/libreliefos.so.2"
cp "$reliefos_archive" "$tmp/lib/libreliefos.a"
cp "$leonos_runtime" "$tmp/lib/libleonos.so.2"
cp "$leonos_archive" "$tmp/lib/libleonos.a"
: "${RUNTIME_BUILTINS:?target compiler-rt archive required}"
cp "$RUNTIME_BUILTINS" "$tmp/lib/libclang_rt.builtins.a"
cp "$driver" "$tmp/bin/reliefos-musl-cc"
chmod 0755 "$tmp/bin/reliefos-musl-cc"
cp "$src/third_party/zlib/zlib.h" "$src/third_party/zlib/zconf.h" \
    "$src/third_party/libpng/png.h" "$src/third_party/libpng/pngconf.h" "$tmp/include/"
# Caller adds the generated libpng configuration via this script's environment.
: "${PNG_CONFIG:?generated libpng config required}"
cp "$PNG_CONFIG" "$tmp/include/pnglibconf.h"
mkdir -p "$tmp/share/licenses/linux-pam" "$tmp/share/licenses/libxcrypt"
cp "$pam/share/licenses/linux-pam/LICENSE" "$tmp/share/licenses/linux-pam/"
cp "$auth/share/licenses/libxcrypt/LICENSE" "$tmp/share/licenses/libxcrypt/"
printf '%s\n' "$epoch" > "$tmp/.source-date-epoch"
if [ -d "$stage.previous" ] && [ ! -e "$stage" ]; then mv "$stage.previous" "$stage"; fi
rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$tmp" "$stage"
rm -rf "$stage.previous"
