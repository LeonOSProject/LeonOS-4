#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -I"$src" "$src/tools/host/assets/leonos-oschinpt-index.c" "$src/tools/host/common/io.c" "$src/tools/host/common/buffer.c" -o "$w/index"
printf 'a\tba\n# ignore\nb\tba\nc\taa\n' > "$w/dict"
"$w/index" "$w/dict" "$w/out"
# OSCI, version 1, two records, 24 dictionary bytes; aa before ba.
[ "$(od -An -tx1 -N12 "$w/out" | tr -d ' \n')" = 4f5343490100000002000000 ]
[ "$(od -An -tx1 -j16 -N8 "$w/out" | tr -d ' \n')" = 6161000000000000 ]
[ "$(stat -c %s "$w/out")" = 48 ]
printf '# nothing\n' > "$w/empty"
cp "$w/out" "$w/before"
if "$w/index" "$w/empty" "$w/out" 2>/dev/null; then exit 1; fi
cmp "$w/out" "$w/before"
printf 'oschinpt index: ABI ordering, grouping, empty input passed\n'
