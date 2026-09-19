#!/bin/sh
# Missing outputs must invalidate the producing rule, even with a valid stamp.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT
make -s -C "$root" O="$w/out" leonos-auth >"$w/build.log" 2>&1 || {
    tail -n 30 "$w/build.log"; exit 1;
}
test -f "$w/out/auth/.leonos-auth.json"
rm "$w/out/auth/root/lib/libcrypt.so.2"
make -s -C "$root" O="$w/out" leonos-auth >>"$w/build.log" 2>&1
test -e "$w/out/auth/root/lib/libcrypt.so.2"
rm "$w/out/sysroot/musl/include/stdio.h"
make -s -C "$root" O="$w/out" "$w/out/sysroot/musl/include/stdio.h" >>"$w/build.log" 2>&1
test -f "$w/out/sysroot/musl/include/stdio.h"
make -s -C "$root" O="$w/out" leonos-auth >>"$w/build.log" 2>&1
before=$(stat -c '%y' "$w/out/auth/.leonos-auth.json")
make -s -C "$root" O="$w/out" leonos-auth >>"$w/build.log" 2>&1
after=$(stat -c '%y' "$w/out/auth/.leonos-auth.json")
test "$before" = "$after"
# Upstream install preserves header timestamps. A newer producing dependency
# must settle after one rebuild instead of invalidating a grouped output forever.
touch "$w/out/host/bin/leonos-deps"
make -s -C "$root" O="$w/out" leonos-auth >>"$w/build.log" 2>&1
before=$(stat -c '%y' "$w/out/auth/.leonos-auth.json")
make -s -C "$root" O="$w/out" leonos-auth >>"$w/build.log" 2>&1
test "$before" = "$(stat -c '%y' "$w/out/auth/.leonos-auth.json")"
echo 'ok - auth stamp, missing auth/musl output recovery and stable no-op'
