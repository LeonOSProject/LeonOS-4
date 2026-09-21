#!/bin/sh
set -eu

# Build only locale maps requested by configs/nls/LINGUAS.  The source archive
# and checksum are selected by the dependency lock and are fetched separately.
[ "$#" -eq 4 ] || { echo "usage: musl-locales.sh SRC ARCHIVE WORK OUT" >&2; exit 2; }
src=$1
archive=$2
work=$3
out=$4
lock="$src/configs/dependencies.lock.json"
expected=$("$src/out/host/bin/leonos-deps" --lock "$lock" --id musl-locales --print sha256)
actual=$(sha256sum "$archive" | cut -d' ' -f1)
[ "$actual" = "$expected" ] || { echo "musl-locales: checksum mismatch" >&2; exit 1; }
rm -rf "$work"
mkdir -p "$work" "$out"
tar -xzf "$archive" -C "$work" --strip-components=1
for locale in $(cat "$src/configs/nls/LINGUAS"); do
    test "$locale" = zh_CN || continue
    input="$work/locales/$locale"
    # The locked upstream does not yet ship zh_CN. The project keeps the
    # compatible subset derived from glibc's zh_CN definition in configs/locale.
    [ -f "$input" ] || input="$src/configs/locale/$locale"
    test -f "$input" || { echo "musl-locales: no $locale data" >&2; exit 1; }
    # The postmarketOS fork stores musl locale source files directly. They are
    # converted to binary maps by dumplocale from the matching musl build.
    po="$src/configs/locale/$locale.po"
    test -f "$po" || { echo "musl-locales: no gettext map for $locale" >&2; exit 1; }
    msgfmt --check --endianness=little -o "$out/$locale.UTF-8" "$po"
done
