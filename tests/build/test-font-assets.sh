#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -I"$src" "$src/tools/host/assets/leonos-font.c" "$src/tools/host/common/io.c" "$src/tools/host/common/buffer.c" -o "$work/font"
"$work/font" "$src/system/fonts/Deng.ttf" "$src/system/fonts/system.psf" "$work/metro.ttf" "$work/win95.ttf"
[ "$(sha256sum "$work/metro.ttf" | cut -d' ' -f1)" = e051009b42b454352a90ae0c9b8adb58bbdbcd168768750b35bc3357510de141 ]
[ "$(sha256sum "$work/win95.ttf" | cut -d' ' -f1)" = 2000e8330dccdb600977fa1f1ce45a3fae5c1d1c6bb2ad7bba5b68d2c8713401 ]
before=$(stat -c '%Y:%y' "$work/win95.ttf")
"$work/font" "$src/system/fonts/Deng.ttf" "$src/system/fonts/system.psf" "$work/metro.ttf" "$work/win95.ttf"
[ "$before" = "$(stat -c '%Y:%y' "$work/win95.ttf")" ]
printf 'sentinel\n' > "$work/metro.ttf"
printf 'bad\n' > "$work/bad.psf"
if "$work/font" "$src/system/fonts/Deng.ttf" "$work/bad.psf" "$work/metro.ttf" "$work/win95.ttf" 2>/dev/null; then exit 1; fi
[ "$(cat "$work/metro.ttf")" = sentinel ]
head -c 13 "$src/system/fonts/Deng.ttf" > "$work/truncated.ttf"
if "$work/font" "$work/truncated.ttf" "$src/system/fonts/system.psf" "$work/metro.ttf" "$work/win95.ttf" 2>/dev/null; then exit 1; fi
[ "$(cat "$work/metro.ttf")" = sentinel ]
printf 'font assets: reference bytes, no-op and malformed input passed\n'
