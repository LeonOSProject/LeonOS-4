#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
mkdir -p "$w/tree/empty"; chmod 1777 "$w/tree/empty"
printf x > "$w/tree/file with spaces"
ln -s 'file with spaces' "$w/tree/link"
sh "$src/tools/build/stage-inventory.sh" write "$w/tree" "$w/files"
sh "$src/tools/build/stage-inventory.sh" check "$w/tree" "$w/files" "$w/sig"
cp -p "$w/sig" "$w/before"
sh "$src/tools/build/stage-inventory.sh" check "$w/tree" "$w/files" "$w/sig"
[ "$(stat -c %Y "$w/sig")" = "$(stat -c %Y "$w/before")" ]
touch -d @1 "$w/sig"
chmod 755 "$w/tree/empty"
sh "$src/tools/build/stage-inventory.sh" check "$w/tree" "$w/files" "$w/sig"
[ "$(stat -c %Y "$w/sig")" -gt 1 ]
chmod 1777 "$w/tree/empty"; touch -d @1 "$w/sig"
rm "$w/tree/link"
sh "$src/tools/build/stage-inventory.sh" check "$w/tree" "$w/files" "$w/sig"
[ "$(stat -c %Y "$w/sig")" -gt 1 ]
printf 'stage inventories: no-op, directory modes and deleted symlink passed\n'
