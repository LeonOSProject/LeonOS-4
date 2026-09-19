#!/bin/sh
# Hash source membership, links, modes and bytes without whitespace splitting.
set -eu
[ "$#" -ge 2 ] || exit 2
output=$1; shift
mkdir -p "$(dirname "$output")"
work=$(mktemp "$output.XXXXXX")
trap 'rm -f "$work" "$work.digest"' EXIT HUP INT TERM
find "$@" -printf '%y %m %p %l\0' | LC_ALL=C sort -z > "$work"
find "$@" -type f -exec sha256sum --zero -- {} + | LC_ALL=C sort -z >> "$work"
sha256sum "$work" | cut -d' ' -f1 > "$work.digest"
if ! cmp -s "$work.digest" "$output"; then mv "$work.digest" "$output"; fi
