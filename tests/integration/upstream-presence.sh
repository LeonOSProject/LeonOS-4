#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/root"
printf 'payload\n' > "$tmp/root/file"
ln -s /guest/absolute "$tmp/root/link"
printf './file\n./link\n' > "$tmp/manifest"
sh "$src/tools/build/upstream-verify.sh" "$tmp/root" "$tmp/manifest" "$tmp/present.sig"
touch -t 200001010000 "$tmp/present.sig"
before=$(stat -c %Y "$tmp/present.sig")
sh "$src/tools/build/upstream-verify.sh" "$tmp/root" "$tmp/manifest" "$tmp/present.sig"
test "$(stat -c %Y "$tmp/present.sig")" = "$before"
rm "$tmp/root/file"
sh "$src/tools/build/upstream-verify.sh" "$tmp/root" "$tmp/manifest" "$tmp/present.sig"
test "$(stat -c %Y "$tmp/present.sig")" -gt "$before"
printf '%s\n' 'upstream presence invalidation: PASS'
