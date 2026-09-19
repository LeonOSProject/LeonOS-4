#!/bin/sh
# Assemble the complete, relocatable developer SDK without Python.
set -eu
[ "$#" = 6 ] || {
    echo 'usage: developer-sdk.sh SRC MUSL_SDK EXTRA_TREE STAGE ZIP EPOCH' >&2
    exit 2
}
src=$1 musl=$2 extra=$3 stage=$4 archive=$5 epoch=$6
export TZ=UTC LC_ALL=C

for required in "$src/devtools/Makefile" "$musl/include/stdio.h" \
    "$musl/lib/crt1.o" "$musl/bin/leonos-musl-cc"; do
    [ -f "$required" ] || { echo "missing developer SDK input: $required" >&2; exit 1; }
done

mkdir -p "$(dirname "$stage")" "$(dirname "$archive")"
archive_dir=$(CDPATH= cd -- "$(dirname "$archive")" && pwd -P)
archive=$archive_dir/$(basename "$archive")
work=$(mktemp -d "$(dirname "$stage")/.developer-sdk.XXXXXX")
trap 'rm -rf "$work" "$archive.tmp"' EXIT HUP INT TERM
tmp=$work/devtools
mkdir -p "$tmp"

# Keep hand-written documentation, examples, linker scripts and templates.
# Generated headers/components and local example build products are not source
# templates. Export only the documented template surface.
for name in Makefile README.md README-musl.md docs examples; do
    [ ! -e "$src/devtools/$name" ] || cp -a "$src/devtools/$name" "$tmp/"
done
find "$tmp" -type d -name build -prune -exec rm -rf {} +
find "$tmp" -type f \( -name '*.o' -o -name '*.elf' -o -name '*.d' \) -delete
# Generated libraries and the compiler driver must never come from a previous
# checkout's devtools tree.  The musl SDK is authoritative for the C sysroot.
rm -rf "$tmp/lib" "$tmp/bin"
cp -a "$musl/." "$tmp/"

# EXTRA_TREE is prepared by Make from selected component products.  Overlaying
# a small tree keeps this assembler independent of build-directory layout.
if [ -d "$extra" ]; then cp -a "$extra/." "$tmp/"; fi
rm -f "$tmp/.complete"

# Reject partial SDKs before publishing either the directory or the ZIP.
for required in include/stdio.h lib/crt1.o lib/crti.o lib/crtn.o \
    lib/libc.a lib/libc.so lib/libleonos.a lib/libleonos.so.2 \
    bin/leonos-musl-cc Makefile; do
    [ -f "$tmp/$required" ] || { echo "incomplete developer SDK: $required" >&2; exit 1; }
done

find "$tmp" -exec touch -h -d "@$epoch" {} +
(cd "$work" && LC_ALL=C find devtools -print | LC_ALL=C sort | zip -X -q "$archive.tmp" -@)
mv "$archive.tmp" "$archive"

rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$tmp" "$stage"
rm -rf "$stage.previous"
