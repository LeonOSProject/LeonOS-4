#!/bin/sh
# Check the actual offline APK transaction output, not source declarations.
# Catches missing binary preinstalls and accidental repackaging as local apps.
set -eu
out=${1:?usage: test-official-tools.sh OUTPUT_TREE}
out=$(CDPATH= cd -- "$out" && pwd -P)
root=$out/rootfs/managed
apk=$out/upstream/apk/apk.static
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
for package in file libmagic less libncursesw ncurses-terminfo-base vim vim-common xxd; do
    "$apk" --root "$root" info --exists "$package"
done
for command in file less vim; do
    "$apk" --root "$root" info --who-owns "/usr/bin/$command" > "$work/owner"
    grep -F "is owned by $command-" "$work/owner"
    # Compare with the signed archive directly: no rebuilt/replaced executable.
    set -- "$out/upstream/apk/packages/$command-"[0-9]*.apk
    [ "$#" = 1 ] && [ -f "$1" ]
    tar --ignore-zeros -xOf "$1" "usr/bin/$command" > "$work/binary" 2>/dev/null
    cmp "$work/binary" "$root/usr/bin/$command"
done
for command in lua tcc; do
    [ ! -e "$root/usr/bin/$command" ] && [ ! -L "$root/usr/bin/$command" ]
done
for command in file less vim lua tcc; do
    [ ! -e "$root/usr/lib/leonos/apps/$command" ]
done
if grep -Eq '^P:(leonos-(file|less|vim|lua|tcc|editors)|tcc|lua[0-9.]*)$' "$root/lib/apk/db/installed"; then
    echo 'retired local package or optional compiler/interpreter is still installed' >&2
    exit 1
fi
for repository in "$out/packages/apk/repository" "$out/rpr-apps/repository" "$out/rpr-pages"; do
    [ -d "$repository" ] || continue
    if find "$repository" -type f | grep -E '/leonos-(file|less|vim|lua|tcc|editors)-[0-9].*\.apk$'; then
        echo 'retired local package remains in repository' >&2
        exit 1
    fi
done
printf 'official tools: signed binary bytes, APK ownership and retired payload checks passed\n'
