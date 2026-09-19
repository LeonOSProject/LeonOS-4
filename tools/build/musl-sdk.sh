#!/bin/sh
# Build a relocatable musl SDK from new-chain artifacts only.
set -eu
[ "$#" = 9 ] || { echo 'usage: musl-sdk.sh SRC SYSROOT RUNTIME ARCHIVE PAM AUTH DRIVER STAGE EPOCH' >&2; exit 2; }
src=$1 sysroot=$2 runtime=$3 archive=$4 pam=$5 auth=$6 driver=$7 stage=$8 epoch=$9
mkdir -p "$(dirname "$stage")"
tmp=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cp -a "$sysroot/include" "$sysroot/lib" "$sysroot/share" "$tmp/"
cp -a "$src/include/uapi/." "$tmp/include/"
mkdir -p "$tmp/include/leonos" "$tmp/bin"
cp -a "$src/include/leonos/." "$src/userland/libc/include/leonos/." "$tmp/include/leonos/"
cp -a "$pam/usr/include/security" "$tmp/include/"
cp "$auth/usr/include/crypt.h" "$tmp/include/"
cp -P "$pam"/lib/libpam*.so* "$auth"/lib/libcrypt.so* "$tmp/lib/"
cp "$runtime" "$tmp/lib/libleonos.so.2"
cp "$archive" "$tmp/lib/libleonos.a"
: "${RUNTIME_BUILTINS:?target compiler-rt archive required}"
cp "$RUNTIME_BUILTINS" "$tmp/lib/libclang_rt.builtins.a"
cp "$driver" "$tmp/bin/leonos-musl-cc"
chmod 0755 "$tmp/bin/leonos-musl-cc"
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
