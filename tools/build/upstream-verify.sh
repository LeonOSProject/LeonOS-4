#!/bin/sh
# Cheap presence check: a missing installed file invalidates the package group.
# Keep signature mtime stable on an intact install; do not force package rebuilds.
set -eu
[ "$#" = 3 ] || exit 2
root=$1 manifest=$2 signature=$3
mkdir -p "$(dirname "$signature")"
if [ ! -f "$signature" ] || [ ! -f "$manifest" ]; then touch "$signature"; exit 0; fi
while IFS= read -r name; do
 [ -e "$root/$name" ] || [ -L "$root/$name" ] || { touch "$signature"; exit 0; }
done < "$manifest"
