#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes -I"$root" "$root/tools/host/manifest/leonos-gears.c" "$root/tools/host/common/io.c" "$root/tools/host/common/buffer.c" -o "$tmp/gears"
"$tmp/gears" --input "$root/third_party/portablegl/examples/classic/gears.c" --output "$tmp/output"
! grep '#define PORTABLEGL_IMPLEMENTATION' "$tmp/output"
printf '#define PORTABLEGL_IMPLEMENTATION\n#define PORTABLEGL_IMPLEMENTATION\n' >"$tmp/bad"
cp "$tmp/output" "$tmp/before"
if "$tmp/gears" --input "$tmp/bad" --output "$tmp/output"; then exit 1; fi
cmp "$tmp/before" "$tmp/output"
