#!/bin/sh
# Offline Alpine bootstrap: lock digest plus upstream RSA binary signature.
set -eu
[ "$#" = 5 ] || { echo 'usage: upstream-apk.sh SRC DEPS LOCK CACHE STAGE' >&2; exit 2; }
src=$1 deps=$2 lock=$3 cache=$4 stage=$5
get() { "$deps" --lock "$lock" --id "$id" --print "$1"; }
id=apk-tools-static
url=$(get url) digest=$(get sha256)
archive=$cache/${url##*/}
[ -f "$archive" ] || { echo 'missing apk-tools-static archive; run make fetch' >&2; exit 1; }
[ "$(sha256sum "$archive" | cut -d' ' -f1)" = "$digest" ] || exit 1
mkdir -p "$(dirname "$stage")"
tmp=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
# Explicit stdout extraction does not permit archive links to escape staging.
tar --ignore-zeros -xOf "$archive" sbin/apk.static > "$tmp/apk.static"
tar --ignore-zeros -xOf "$archive" sbin/apk.static.SIGN.RSA.sha256.alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub > "$tmp/apk.static.sig"
openssl dgst -sha256 -verify "$src/system/rootfs/etc/apk/keys/alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub" -signature "$tmp/apk.static.sig" "$tmp/apk.static"
chmod 755 "$tmp/apk.static"
mkdir -p "$tmp/packages"
"$deps" --lock "$lock" --list > "$tmp/ids"
while IFS= read -r id; do
 case $id in alpine-*)
  url=$(get url) digest=$(get sha256)
  archive=$cache/${url##*/}
  [ -f "$archive" ] || { echo "missing $id archive; run make fetch" >&2; exit 1; }
  [ "$(sha256sum "$archive" | cut -d' ' -f1)" = "$digest" ] || exit 1
  "$tmp/apk.static" --keys-dir "$src/system/rootfs/etc/apk/keys" verify "$archive"
  cp "$archive" "$tmp/packages/"
 esac
done < "$tmp/ids"
rm "$tmp/ids"
printf '%s\n' verified > "$tmp/.complete"
if [ -d "$stage.previous" ] && [ ! -e "$stage" ]; then mv "$stage.previous" "$stage"; fi
rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$tmp" "$stage"
rm -rf "$stage.previous"
(cd "$stage" && find . \( -type f -o -type l \) ! -name installed-files -print | LC_ALL=C sort) > "$stage/installed-files"
