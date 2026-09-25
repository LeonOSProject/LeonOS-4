#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT

mkdir -p "$w/src/include/uapi" "$w/src/include/leonos" \
    "$w/src/userland/libc/include/leonos" "$w/src/third_party/zlib" \
    "$w/src/third_party/libpng" "$w/sysroot/include" "$w/sysroot/lib" \
    "$w/sysroot/share/licenses/musl" "$w/pam/usr/include/security" \
    "$w/pam/lib/pkgconfig" "$w/pam/share/licenses/linux-pam" \
    "$w/auth/usr/include" "$w/auth/lib" "$w/auth/share/licenses/libxcrypt" \
    "$w/export/linux"
printf 'stdio fixture\n' > "$w/sysroot/include/stdio.h"
printf 'crt fixture\n' > "$w/sysroot/lib/crt1.o"
for f in libc.so libc.a libmimalloc.so.3 mimalloc.o Scrt1.o crti.o crtn.o rcrt1.o libssp_nonshared.a; do
    printf '%s\n' "$f" > "$w/sysroot/lib/$f"
done
printf 'license\n' > "$w/sysroot/share/licenses/musl/COPYRIGHT"
printf 'header\n' > "$w/src/include/leonos/api.h"
# UAPI fixture: the SDK must take ABI headers from the kernel header export
# (headers_install output), never from the kernel-owned source tree.
printf 'uapi fixture\n' > "$w/export/linux/types.h"
printf 'header\n' > "$w/pam/usr/include/security/pam_appl.h"
printf 'header\n' > "$w/auth/usr/include/crypt.h"
printf 'license\n' > "$w/pam/share/licenses/linux-pam/LICENSE"
printf 'license\n' > "$w/auth/share/licenses/libxcrypt/LICENSE"
printf 'so\n' > "$w/pam/lib/libpam.so.0"
printf 'so\n' > "$w/auth/lib/libcrypt.so.2"
printf 'runtime\n' > "$w/runtime.so"
printf 'archive\n' > "$w/runtime.a"
printf 'builtins\n' > "$w/builtins.a"
printf 'driver\n' > "$w/driver"
printf 'header\n' > "$w/src/third_party/zlib/zlib.h"
printf 'header\n' > "$w/src/third_party/zlib/zconf.h"
printf 'header\n' > "$w/src/third_party/libpng/png.h"
printf 'header\n' > "$w/src/third_party/libpng/pngconf.h"
printf 'header\n' > "$w/pnglibconf.h"

RUNTIME_BUILTINS="$w/builtins.a" PNG_CONFIG="$w/pnglibconf.h" \
    sh "$root/tools/build/musl-sdk.sh" "$w/src" "$w/sysroot" \
    "$w/runtime.so" "$w/runtime.a" "$w/pam" "$w/auth" "$w/driver" \
    "$w/musl-sdk" 1234 "$w/export"
rm "$w/musl-sdk/include/stdio.h" "$w/musl-sdk/lib/crt1.o"
RUNTIME_BUILTINS="$w/builtins.a" PNG_CONFIG="$w/pnglibconf.h" \
    sh "$root/tools/build/musl-sdk.sh" "$w/src" "$w/sysroot" \
    "$w/runtime.so" "$w/runtime.a" "$w/pam" "$w/auth" "$w/driver" \
    "$w/musl-sdk" 1234 "$w/export"
cmp "$w/export/linux/types.h" "$w/musl-sdk/include/linux/types.h"
cmp "$w/sysroot/include/stdio.h" "$w/musl-sdk/include/stdio.h"
cmp "$w/sysroot/lib/crt1.o" "$w/musl-sdk/lib/crt1.o"
echo 'ok - musl SDK stages recover deleted required members and take UAPI from the export'
