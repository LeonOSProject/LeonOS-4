#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -I"$src" "$src/tools/host/version/leonos-version.c" "$src/tools/host/common/io.c" "$src/tools/host/common/buffer.c" -o "$w/version"
"$w/version" --version-file "$src/configs/build-version" --source-id abc123 --epoch 1700000000 --output "$w/default.h"
grep -Eq '^#define LEONOS_KERNEL_VERSION "[0-9]+\.[0-9]+\.[0-9]+-1700000000"$' "$w/default.h"
grep -q '^#define LEONOS_SOURCE_ID "abc123"$' "$w/default.h"
"$w/version" --version-file "$src/configs/build-version" --source-id abc123 --epoch 1700000000 --build-id 42 --output "$w/id.h"
grep -q '^#define LEONOS_BUILD_NUMBER 42$' "$w/id.h"
grep -Eq '^#define LEONOS_KERNEL_VERSION "[0-9]+\.[0-9]+\.[0-9]+-42"$' "$w/id.h"
printf 'version ABI: numeric update version and stable source identity passed\n'
