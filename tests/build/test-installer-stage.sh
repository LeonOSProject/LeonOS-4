#!/bin/sh
# Test installer-only payload at the signed-package boundary, not just its source.
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT HUP INT TERM
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra -Werror "$src/tools/host/manifest/leonos-dedup.c" -o "$w/dedup"
export INSTALLER_DEDUP_TOOL="$w/dedup"
mkdir -p "$w/src/tools/build" "$w/src/docs" "$w/out/userland" "$w/out/userland-installer" "$w/out/userland-installer-policy" "$w/out/installer/lib" "$w/raw/usr/lib/leonos/apps/desktop" "$w/raw/usr/lib/leonos/apps/settings" "$w/raw/usr/lib/leonos" "$w/raw/usr/bin" "$w/esp"
printf 'guide\n' > "$w/src/docs/ADVANCED_INSTALL.txt"
for app in desktop settings; do printf 'policy\n' > "$w/out/userland-installer-policy/$app.elf"; done
printf 'runtime\n' > "$w/out/installer/lib/libleonos.so.2"
printf 'gptinit\n' > "$w/out/userland-installer/gptinit.elf"
printf 'installer\n' > "$w/out/userland/installer.elf"
chmod 755 "$w/out/userland/installer.elf"
cat > "$w/src/tools/build/apk-stage.sh" <<'ADAPTER'
#!/bin/sh
set -eu
cp -a "$2" "$3"
ADAPTER
APK_TOOL=fixture APK_UPSTREAM=fixture APK_OWN_TOOL=fixture APK_KEY=fixture APK_VERSION=fixture \
 sh "$src/tools/build/installer-stage.sh" "$w/src" "$w/out" "$w/raw" "$w/esp" "$w/stage" 1700000000
program=usr/lib/leonos/apps/installer/installer.elf
[ -x "$w/stage/$program" ] || { echo 'FAIL - installer executable missing from runtime package'; exit 1; }
cmp "$w/out/userland/installer.elf" "$w/stage/$program"
[ "$(readlink "$w/stage/usr/bin/installer")" = ../lib/leonos/apps/installer/installer.elf ]
[ ! -e "$w/stage/install/root/$program" ]
echo 'installer stage: executable packaged in runtime only, command link preserved'
