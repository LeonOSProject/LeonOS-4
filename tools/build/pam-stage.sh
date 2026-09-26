#!/bin/sh
# Parent adapter: verified archive/patches, isolated upstream work, then publish.
set -eu
case ${MAKEFLAGS%% *} in *n*) exit 0 ;; esac
[ "$#" = 10 ] || { echo 'usage: pam-stage.sh SRC DEPS LOCK CACHE WORK STAGE SYSROOT AUTH CC TARGET' >&2; exit 2; }
src=$1 deps=$2 lock=$3 cache=$4 work=$5 stage=$6 sysroot=$7 auth=$8 cc=$9
shift 9
target=$1
directory=$("$deps" --lock "$lock" --id linux-pam --print directory)
url=$("$deps" --lock "$lock" --id linux-pam --print url)
digest=$("$deps" --lock "$lock" --id linux-pam --print sha256)
archive=$cache/${url##*/}
[ -f "$archive" ] || { echo 'missing linux-pam archive: run make fetch' >&2; exit 1; }
[ "$(sha256sum "$archive" | cut -d' ' -f1)" = "$digest" ] || exit 1
mkdir -p "$work" "$(dirname "$stage")"
"$deps" --lock "$lock" --id linux-pam --list-patches > "$work/patches"
{ printf '%s\n' "$digest"; cat "$work/patches"; } > "$work/source-key.new"
if ! cmp -s "$work/source-key.new" "$work/source-key"; then
    rm -rf "$work/src"
    mkdir -p "$work/src"
    tar -xf "$archive" -C "$work/src" --no-same-owner
    while read -r expected path; do
        [ -n "$path" ] || continue
        [ "$(sha256sum "$src/$path" | cut -d' ' -f1)" = "$expected" ] || exit 1
        patch --batch -p1 -d "$work/src/$directory" -i "$src/$path"
    done < "$work/patches"
    mv "$work/source-key.new" "$work/source-key"
fi
tmp=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
make -f "$src/tools/build/pam/Makefile" SRC="$work/src/$directory" \
    BUILD="$work/build" DESTDIR="$tmp" SYSROOT="$sysroot" DEPS="$auth" \
    CC="$cc" TARGET="$target" install
test -e "$tmp/lib/libpam.so.0"
test -f "$tmp/usr/include/security/pam_appl.h"
# Build the product password-policy module against the same target libc/PAM.
# pam_unix continues to own authentication and password storage.
resource=$("$cc" -print-resource-dir)
"$cc" --target="$target" --sysroot="$sysroot" --gcc-toolchain=/nonexistent \
    -fuse-ld=lld --rtlib=compiler-rt --unwindlib=none -O2 -fPIC -shared \
    -nostdinc -isystem "$sysroot/include" -isystem "$resource/include" \
    -I"$tmp/usr/include" -I"${UPSTREAM_UAPI:-$src/include/uapi}" -I"$src/include" \
    -ffile-prefix-map="$src"=. -Wl,-z,relro,-z,now,--no-undefined \
    "$src/userland/pam/pam_leonos_password.c" "$src/userland/runtime/src/auth_password.c" \
    -L"$tmp/lib" -lpam -o "$tmp/lib/security/pam_leonos_password.so"
mkdir -p "$tmp/share/licenses/linux-pam"
cp "$work/src/$directory/Copyright" "$tmp/share/licenses/linux-pam/LICENSE"
# Preserve the last good tree when upstream build/install failed. Consumers only
# run after this recipe under the same O lock. Directory swap is not a multi-
# reader transaction; image assembly must consume it as a Make dependency.
if [ -d "$stage.previous" ] && [ ! -e "$stage" ]; then mv "$stage.previous" "$stage"; fi
rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$tmp" "$stage"
rm -rf "$stage.previous"
