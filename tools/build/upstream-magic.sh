#!/bin/sh
set -eu
MAKEFLAGS=${MAKEFLAGS-}
case ${MAKEFLAGS%% *} in *n*) exit 0 ;; esac
[ "$#" = 4 ] || { echo 'usage: upstream-magic.sh SRC WORK OUTPUT HOSTCC' >&2; exit 2; }
src=$1 work=$2 output=$3 hostcc=$4
mkdir -p "$work" "$(dirname "$output")"
rm -rf "$work/source"
mkdir "$work/source"
git -C "$src/third_party/file" archive HEAD > "$work/source.tar"
tar -xf "$work/source.tar" -C "$work/source"
cd "$work/source"
# Host generator deliberately does not inherit target compiler flags.
MAKEOVERRIDES= CC=$hostcc CFLAGS=-O2 CPPFLAGS= LDFLAGS= LIBS=
export MAKEOVERRIDES CC CFLAGS CPPFLAGS LDFLAGS LIBS
autoreconf -fi
./configure --disable-shared --disable-zlib --disable-bzlib --disable-xzlib --disable-zstdlib --disable-lzlib --disable-lrziplib --disable-lz4lib --disable-libseccomp --disable-landlock
make all
test -s magic/magic.mgc
cp magic/magic.mgc "$output.tmp"
mv "$output.tmp" "$output"
