#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -I"$src" "$src/tools/host/version/leonos-version.c" "$src/tools/host/common/io.c" "$src/tools/host/common/buffer.c" -o "$w/version"
"$w/version" --version-file "$src/configs/build-version" --source-id abc123 --epoch 1700000000 --output "$w/default.h"
grep -Eq '^#define LEONOS_KERNEL_VERSION "[0-9]+\.[0-9]+\.[0-9]+"$' "$w/default.h"
grep -q '^#define LEONOS_SOURCE_ID "abc123"$' "$w/default.h"
if grep -q LEONOS_BUILD_NUMBER "$w/default.h"; then exit 1; fi
if "$w/version" --version-file "$src/configs/build-version" --build-id 42 --output "$w/id.h" 2>/dev/null; then exit 1; fi
printf 'version ABI: release version without build number, separate source identity passed\n'
