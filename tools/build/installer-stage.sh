#!/bin/sh
# Build both installer policy roots through the same signed APK transaction as
# the ordinary system, then embed the installed root and minimal ESP payload.
set -eu
[ "$#" = 6 ] || { echo 'usage: installer-stage SRC O RAW_ROOT ESP OUTPUT EPOCH' >&2; exit 2; }
src=$1 out=$2 raw=$3 esp=$4 output=$5 epoch=$6
: "${APK_TOOL:?}" "${APK_UPSTREAM:?}" "${APK_OWN_TOOL:?}" "${APK_KEY:?}" "${APK_VERSION:?}"
mkdir -p "$(dirname "$output")"
work=$(mktemp -d "$output.new.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cp -a "$raw" "$work/installed-raw"
for app in desktop settings; do
    cp "$out/userland-installer-policy/$app.elf" "$work/installed-raw/usr/lib/leonos/apps/$app/$app.elf"
done
cp "$out/installer/lib/libleonos.so.2" "$work/installed-raw/usr/lib/leonos/libleonos.so.2"
rm -f "$work/installed-raw/etc/license.conf" "$work/installed-raw/etc/install.id"
package_root() {
    printf '  APK      %s\n' "$3"
    SOURCE_DATE_EPOCH=$epoch APK_MUSL_SYSROOT=$out/sysroot/musl sh "$src/tools/build/apk-stage.sh" "$src" "$1" "$2" "$3" \
        "$APK_TOOL" "$APK_UPSTREAM" "$src/configs/apk-ownership.json" "$APK_OWN_TOOL" "$APK_KEY" "$APK_VERSION"
}
package_root "$work/installed-raw" "$work/installed" "$out/packages/apk-installed"
cp -a "$raw" "$work/runtime-raw"
mkdir -p "$work/runtime-raw/usr/lib/leonos/apps/installer"
cp "$out/userland/installer.elf" "$work/runtime-raw/usr/lib/leonos/apps/installer/installer.elf"
chmod 755 "$work/runtime-raw/usr/lib/leonos/apps/installer/installer.elf"
ln -s ../lib/leonos/apps/installer/installer.elf "$work/runtime-raw/usr/bin/installer"
cp "$out/installer/lib/libleonos.so.2" "$work/runtime-raw/usr/lib/leonos/libleonos.so.2"
mkdir -p "$work/runtime-raw/usr/lib/leonos/apps/gptinit" "$work/runtime-raw/root" "$work/runtime-raw/etc/leonos"
cp "$out/userland-installer/gptinit.elf" "$work/runtime-raw/usr/lib/leonos/apps/gptinit/gptinit.elf"
cat > "$work/runtime-raw/usr/lib/leonos/apps/gptinit/manifest.ini" <<'MANIFEST'
[app]
id=gptinit
name=GPT initializer
version=installer
category=Installer tools
exec=gptinit.elf
entry=0
terminal=1
hidden=1
commands=gptinit
MANIFEST
ln -s ../lib/leonos/apps/gptinit/gptinit.elf "$work/runtime-raw/usr/bin/gptinit"
printf 'installer\n' > "$work/runtime-raw/etc/leonos/installer-runtime"
cp "$src/docs/ADVANCED_INSTALL.txt" "$work/runtime-raw/root/ADVANCED_INSTALL.txt"
package_root "$work/runtime-raw" "$work/runtime" "$out/packages/apk-installer-runtime"
[ -x "$work/runtime/usr/lib/leonos/apps/installer/installer.elf" ] || {
    echo 'installer runtime package is missing installer.elf' >&2
    exit 1
}
mkdir -p "$work/runtime/install"
printf '  STAGE    installer payload and ESP\n'
mv "$work/installed" "$work/runtime/install/root"
cp -a "$esp" "$work/runtime/install/esp"
"${INSTALLER_DEDUP_TOOL:-$out/host/bin/leonos-dedup}" "$work/runtime"
find "$work/runtime" -exec touch -h -d "@$epoch" {} +
if [ -d "$output.previous" ] && [ ! -e "$output" ]; then mv "$output.previous" "$output"; fi
rm -rf "$output.previous"
if [ -e "$output" ]; then mv "$output" "$output.previous"; fi
mv "$work/runtime" "$output"
rm -rf "$output.previous"
