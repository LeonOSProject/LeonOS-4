#!/bin/sh
# Requires a real configured O; never edits project sources.
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
out=${1:?usage: upstream-incremental.sh O}
make_out=$out
out=$(CDPATH= cd -- "$out" && pwd)
work=$(mktemp -d "$out/meta/upstream-incremental.XXXXXX")
trap 'rm -rf "$work"' EXIT
snapshot() {
 for pkg in libmd libbsd util-linux sudo shadow e2fsprogs dosfstools exfatprogs ncurses vim; do
  find "$out/upstream/$pkg/root" -type f -printf '%p %T@ %s\n'
 done
 find "$out/userland" "$out/resources" -type f -printf '%p %T@ %s\n'
}
make -C "$src" O="$make_out" leonos-upstream -j8 > "$work/baseline.log" 2>&1 || { cat "$work/baseline.log" >&2; exit 1; }
snapshot > "$work/before.unsorted"
LC_ALL=C sort "$work/before.unsorted" > "$work/before"
make -C "$src" O="$make_out" leonos-upstream -j8 > "$work/noop.log" 2>&1 || { cat "$work/noop.log" >&2; exit 1; }
snapshot > "$work/after.unsorted"
LC_ALL=C sort "$work/after.unsorted" > "$work/after"
cmp "$work/before" "$work/after"
# Exercise the complete install manifest, beyond the primary ELF outputs.
secondary=$out/upstream/dosfstools/root/usr/share/doc/dosfstools/README
test -f "$secondary"
rm "$secondary"
make -C "$src" O="$make_out" upstream-dosfstools -j8 > "$work/restore.log" 2>&1 || { cat "$work/restore.log" >&2; exit 1; }
test -f "$secondary"
printf '%s\n' 'upstream no-op mtimes and secondary-output restoration: PASS'
