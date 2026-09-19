#!/bin/sh
set -eu
MAKEFLAGS=${MAKEFLAGS-}
case ${MAKEFLAGS%% *} in *n*) exit 0 ;; esac
[ "$#" = 10 ] || { echo 'usage: upstream-terminal.sh PACKAGE SRC DEPS LOCK WORK STAGE MUSL NCURSES CC TARGET' >&2; exit 2; }
pkg=$1 src=$2 deps=$3 lock=$4 work=$5 stage=$6 musl=$7 ncurses=$8 cc=$9
shift 9
target=$1
get() { "$deps" --lock "$lock" --id "$pkg" --print "$1"; }
source=$src/$(get directory)
revision=$(get commit)
[ "$(git -C "$source" rev-parse HEAD)" = "$revision" ] || { echo "$pkg: source revision mismatch" >&2; exit 1; }
mkdir -p "$work" "$(dirname "$stage")"
rm -rf "$work/source" "$work/build"
mkdir -p "$work/source" "$work/build"
# git archive consumes the pinned revision, excluding local/generated changes.
git -C "$source" archive "$revision" > "$work/source.tar"
tar -xf "$work/source.tar" -C "$work/source"
source=$work/source
resource=$("$cc" -print-resource-dir)
CC="$cc --target=$target --sysroot=$musl --gcc-toolchain=/nonexistent -fuse-ld=lld --rtlib=compiler-rt --unwindlib=none -nostdinc -isystem $musl/include -isystem $resource/include -L$musl/lib"
AR=llvm-ar RANLIB=llvm-ranlib CFLAGS='-O2 -fno-stack-protector -mno-avx -mno-avx2' CPPFLAGS= LDFLAGS=-static LIBS=
export CC AR RANLIB CFLAGS CPPFLAGS LDFLAGS LIBS
MAKEFLAGS=${MAKEFLAGS%% -- *}; export MAKEFLAGS; unset MAKEOVERRIDES
tmp=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
case $pkg in
ncurses)
 tic=$(command -v tic); infocmp=$(command -v infocmp)
 cd "$work/build"
 "$source/configure" --host="$target" --prefix=/usr --with-normal --without-shared --without-debug --enable-widec --with-termlib --enable-overwrite --without-ada --without-cxx-binding --without-tests --with-default-terminfo-dir=/usr/share/terminfo --with-terminfo-dirs=/usr/share/terminfo:/etc/terminfo:/lib/terminfo --with-fallbacks=xterm,xterm-256color,linux,vt100,ansi,screen,screen-256color --with-tic-path="$tic" --with-infocmp-path="$infocmp" --enable-mixed-case
 ;;
vim)
 CPPFLAGS="-I$ncurses/usr/include" LDFLAGS="-static -L$ncurses/usr/lib" LIBS="-lncursesw -ltinfow $musl/lib/mimalloc.o"
 export CPPFLAGS LDFLAGS LIBS
 cd "$source"
 ./configure --host="$target" --prefix=/usr --with-features=normal --enable-gui=no --without-wayland --without-x --with-tlib=ncursesw --disable-nls --disable-gpm --disable-pythoninterp --disable-python3interp --disable-perlinterp --disable-rubyinterp --disable-luainterp --disable-tclinterp
 ;;
*) exit 2 ;;
esac
make
make install "DESTDIR=$tmp"
if [ "$pkg" = ncurses ]; then
 for name in xterm xterm-256color linux vt100 ansi screen screen-256color; do grep -F "/* $name */" "$work/build/ncurses/fallback.c" >/dev/null; done
 # Retain the same deterministic first spelling as the former driver.
 (cd "$tmp/usr/share/terminfo" && find . -type f -print | LC_ALL=C sort | LC_ALL=C awk '{ k=tolower($0); if (seen[k]++) print }') > "$work/duplicate-terminfo"
 while IFS= read -r name; do rm "$tmp/usr/share/terminfo/$name"; done < "$work/duplicate-terminfo"
 license=COPYING
else license=LICENSE; fi
mkdir -p "$tmp/usr/share/licenses/$pkg"
cp "$source/$license" "$tmp/usr/share/licenses/$pkg/"
printf '%s\n' "$pkg $revision $target" > "$tmp/.complete"
if [ -d "$stage.previous" ] && [ ! -e "$stage" ]; then mv "$stage.previous" "$stage"; fi
rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$tmp" "$stage"
rm -rf "$stage.previous"
(cd "$stage" && find . \( -type f -o -type l \) ! -name .complete -print | LC_ALL=C sort) > "$work/installed-files"
