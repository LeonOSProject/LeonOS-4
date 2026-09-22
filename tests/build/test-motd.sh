#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -Wall -Wextra -Werror -I"$src" \
    "$src/tests/host/test_motd.c" "$src/userland/apps/motd/format.c" -o "$work/test"
"$work/test"
