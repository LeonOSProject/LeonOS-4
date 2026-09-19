#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'chmod -R u+w "$work"; rm -rf "$work"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -Wall -Wextra -Werror -Wpedantic -I"$src" \
 "$src/tools/host/manifest/leonos-stage.c" "$src/tools/host/common/io.c" "$src/tools/host/common/buffer.c" "$src/tools/host/manifest/json.c" -o "$work/stage"
mkdir "$work/input" "$work/root"
printf 'payload\n' > "$work/input/file with spaces"
ln -s 'file with spaces' "$work/input/alias"
printf 't\t%s\t/usr/share/fixture\t0755\tfixture\tunique\n' "$work/input" > "$work/plan"
"$work/stage" "$work/plan" "$work/root" "$work/manifest.json"
cmp "$work/input/file with spaces" "$work/root/usr/share/fixture/file with spaces"
[ "$(readlink "$work/root/usr/share/fixture/alias")" = 'file with spaces' ]
grep -q '"schema_version":1' "$work/manifest.json"
# A duplicate claim must fail before copying any payload.
cat "$work/plan" "$work/plan" > "$work/duplicate"
mkdir "$work/duplicate-root"
if "$work/stage" "$work/duplicate" "$work/duplicate-root" "$work/bad.json" 2>/dev/null; then exit 1; fi
[ ! -e "$work/duplicate-root/usr" ]
# Explicit product overlay wins and mode is retained.
printf 'override\n' > "$work/override"
cp "$work/plan" "$work/overlay"
printf 'f\t%s\t/usr/share/fixture/file with spaces\t0600\tpolicy\toverride\n' "$work/override" >> "$work/overlay"
mkdir "$work/overlay-root"
"$work/stage" "$work/overlay" "$work/overlay-root" "$work/overlay.json"
cmp "$work/override" "$work/overlay-root/usr/share/fixture/file with spaces"
[ "$(stat -c %a "$work/overlay-root/usr/share/fixture/file with spaces")" = 600 ]
# Guest symlink parents must never cause host writes.
mkdir "$work/escape-root" "$work/outside"
printf 'l\t%s\t/escape\t0777\tfixture\tunique\nf\t%s\t/escape/bad\t0644\tfixture\tunique\n' "$work/outside" "$work/override" > "$work/escape-plan"
if "$work/stage" "$work/escape-plan" "$work/escape-root" "$work/bad.json" 2>/dev/null; then exit 1; fi
[ ! -e "$work/outside/bad" ]
printf 'f\t%s\t/../outside\t0644\tfixture\tunique\n' "$work/override" > "$work/traversal"
if "$work/stage" "$work/traversal" "$work/escape-root" "$work/bad.json" 2>/dev/null; then exit 1; fi
# Fresh staging cannot retain a removed resource.
rm "$work/input/alias" "$work/input/file with spaces"
mkdir "$work/deleted-root"
"$work/stage" "$work/plan" "$work/deleted-root" "$work/deleted.json"
[ ! -e "$work/deleted-root/usr/share/fixture/file with spaces" ]
printf 'rootfs stage: fixtures passed\n'
