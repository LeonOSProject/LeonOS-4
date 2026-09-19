#!/bin/sh
# Record/check stage membership, kinds, modes and symlink targets. Inventories
# live outside their stage, so checking never mutates the tree being verified.
set -eu
[ "$#" -ge 3 ] || exit 2
mode=$1 root=$2 inventory=$3
mkdir -p "$(dirname "$inventory")"
work=$(mktemp "$inventory.XXXXXX")
trap 'rm -f "$work"' EXIT HUP INT TERM
if [ -d "$root" ] && [ ! -L "$root" ]; then
    find "$root" -mindepth 1 -printf '%y\t%m\t%P\t%l\0' | LC_ALL=C sort -z > "$work"
fi
case $mode in
write) [ -d "$root" ] && [ ! -L "$root" ]; mv "$work" "$inventory" ;;
check)
    [ "$#" = 4 ] || exit 2
    signature=$4
    mkdir -p "$(dirname "$signature")"
    if [ ! -d "$root" ] || [ -L "$root" ] || ! cmp -s "$work" "$inventory" || [ ! -f "$signature" ]; then
        touch "$signature"
    fi
    ;;
*) exit 2 ;;
esac
