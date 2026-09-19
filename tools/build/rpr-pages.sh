#!/bin/sh
# Assemble a local Pages tree only. Publication is a separate user/CI action.
set -eu
[ "$#" = 8 ] || exit 2
repository=$1 apps=$2 kernel=$3 middle=$4 build=$5 apk=$6 key=$7 output=$8
[ ! -L "$key" ] && [ -f "$key" ] && [ "$(stat -c %a "$key")" = 600 ] || { echo 'private signing key must be a regular 0600 file' >&2; exit 1; }
version=$(sed -n 's/^#define LEONOS_KERNEL_VERSION "\([0-9.]*-[0-9]*\)"$/\1/p' "$build")
build_number=$(sed -n 's/^#define LEONOS_BUILD_NUMBER \([0-9]*\)$/\1/p' "$build")
[ -n "$version" ] && [ "${version##*-}" = "$build_number" ] || { echo 'invalid release version' >&2; exit 1; }
image=${version%-*}
mkdir -p "$(dirname "$output")"
work=$(mktemp -d "$output.new.XXXXXX")
work=$(CDPATH= cd -- "$work" && pwd -P)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/site/apk" "$work/site/kernel"
openssl pkey -in "$key" -pubout -out "$work/site/apk/leonos-rpr.rsa.pub" 2>/dev/null
chmod 644 "$work/site/apk/leonos-rpr.rsa.pub"
for source in "$repository"/leonos-*.apk "$apps"/leonos-*.apk; do
    [ -f "$source" ] || { echo "missing RPR package $source" >&2; exit 1; }
    name=${source##*/}
    case $name in *[!a-zA-Z0-9._+-]*) echo 'unsafe RPR filename' >&2; exit 1 ;; esac
    [ ! -e "$work/site/apk/$name" ] || { echo "duplicate RPR package $name" >&2; exit 1; }
    cp "$source" "$work/site/apk/$name"
done
"$apk" mkndx --keys-dir "$work/site/apk" --sign-key "$key" --output "$work/site/apk/packages.adb" "$work/site/apk"/*.apk
(cd "$work/site/apk" && sha256sum ./*.apk leonos-rpr.rsa.pub packages.adb | sed 's|  ./|  |' | LC_ALL=C sort -k2 > SHA256SUMS)
{
    printf '{"architecture":"x86_64","index":"packages.adb","public_key":"leonos-rpr.rsa.pub","schema":1,"packages":['
    comma=
    for file in "$work/site/apk"/*.apk; do printf '%s"%s"' "$comma" "${file##*/}"; comma=,; done
    printf ']}\n'
} > "$work/site/apk/repository.json"
cp "$kernel" "$work/site/kernel/kernel.sys"
cp "$middle" "$work/site/kernel/middlelayer.sys"
kernel_hash=$(sha256sum "$kernel" | cut -d' ' -f1)
middle_hash=$(sha256sum "$middle" | cut -d' ' -f1)
cat > "$work/site/kernel/release.txt" <<RELEASE
format_version=1
image_version=$image
version=$version
build_number=$build_number
kernel_file=kernel.sys
kernel_sha256=$kernel_hash
middlelayer_file=middlelayer.sys
middlelayer_sha256=$middle_hash
RELEASE
printf '%s  kernel.sys\n%s  middlelayer.sys\n' "$kernel_hash" "$middle_hash" > "$work/site/kernel/SHA256SUMS"
printf '{"architecture":"x86_64","build_number":%s,"image_version":"%s","files":{"kernel.sys":{"sha256":"%s"},"middlelayer.sys":{"sha256":"%s"}},"schema":1,"version":"%s"}\n' "$build_number" "$image" "$kernel_hash" "$middle_hash" "$version" > "$work/site/kernel/release.json"
printf '{"apk":"/apk/packages.adb","kernel":"/kernel/release.txt","schema":1,"version":"%s"}\n' "$version" > "$work/site/manifest.json"
printf '<!doctype html><meta charset=utf-8><title>LeonOS RPR</title><h1>LeonOS 4 Remote Package Repository</h1><p>Latest kernel: %s</p><ul><li><a href="apk/repository.json">APK repository</a></li><li><a href="kernel/release.json">Kernel release</a></li></ul>\n' "$version" > "$work/site/index.html"
: > "$work/site/.nojekyll"
printf 'leonos-rpr-ok\n' > "$work/site/health.txt"
printf '%s\n' "$version" > "$work/site/.complete"
if [ -d "$output.previous" ] && [ ! -e "$output" ]; then mv "$output.previous" "$output"; fi
rm -rf "$output.previous"
if [ -e "$output" ]; then mv "$output" "$output.previous"; fi
mv "$work/site" "$output"
rm -rf "$output.previous"
