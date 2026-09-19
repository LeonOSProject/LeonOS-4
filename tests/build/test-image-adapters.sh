#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror "$src/tools/host/images/leonos-ext2-time.c" -lext2fs -lcom_err -o "$w/time"
mkdir -p "$w/root/etc" "$w/root/tmp"
mkdir -p "$w/root/etc/leonos" "$w/root/home/test"
printf 'leonos-standalone-test-v1\n' > "$w/root/etc/leonos/test-image"
printf 'owned\n' > "$w/root/home/test/file"
chmod 1777 "$w/root/tmp"
printf 'data\n' > "$w/root/file with spaces"
ln -s 'file with spaces' "$w/root/link"
for name in a b; do
    LEONOS_EXT2_TIME=$w/time sh "$src/tools/build/images.sh" ext2 "$w/root" "$w/$name.ext2" 1700000000 5c13543b-732c-4f81-8652-621124484420 > "$w/$name.log" 2>&1
done
cmp "$w/a.ext2" "$w/b.ext2"
debugfs -R 'stat /tmp' "$w/a.ext2" 2>/dev/null | grep -q '1777'
debugfs -R 'stat /link' "$w/a.ext2" 2>/dev/null | grep -q 'symlink'
debugfs -R 'stat /home/test/file' "$w/a.ext2" 2>/dev/null | grep -Eq 'User: +1000 +Group: +1000'
[ "$(stat -c %a "$w/root/tmp")" = 1777 ]
printf 'image adapters: reproducible ext2, modes and symlinks passed\n'
