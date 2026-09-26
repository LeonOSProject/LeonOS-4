#!/bin/sh
# locale.conf uses standard locale vocabulary.
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)

for f in tools/build/rootfs-stage.sh tools/build/standalone-root.sh; do
    grep -q 'LANG=' "$src/$f" || { printf 'FAIL - %s writes no LANG\n' "$f"; exit 1; }
done
grep -q 'env_apply_locale' "$src/userland/runtime/src/environment.c"
grep -q 'LEONOS_PATH_LOCALE_CONF' "$src/userland/runtime/src/environment.c"
grep -q 'config VMDK_DEFAULT_LANG' "$src/Kconfig"
if grep -q 'VMDK_DEFAULT_LANGUAGE' "$src/Kconfig"; then
    printf 'FAIL - old choice symbol still declared\n'
    exit 1
fi

grep -q 'MUSL_LOCPATH' "$src/tools/build/rootfs-stage.sh"
printf 'locale conf: standard key, locale path, kconfig rename passed\n'

# Execute the writer: locale selection must survive staging and keep legacy apps
# in the same language during the transition.
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT HUP INT TERM
mkdir -p "$w/input/etc/leonos" "$w/input/etc/skel"
printf '%%wheel ALL=(ALL:ALL) ALL\n' > "$w/input/etc/sudoers"
for locale in zh_CN.UTF-8 en_US.UTF-8; do
    sh "$src/tools/build/standalone-root.sh" "$src" "$w/input" "$w/$locale" live "$locale"
    grep -qx "LANG=$locale" "$w/$locale/etc/leonos/locale.conf"
    grep -qx 'MUSL_LOCPATH=/usr/share/musl/locales' "$w/$locale/etc/leonos/locale.conf"
done
if sh "$src/tools/build/standalone-root.sh" "$src" "$w/input" "$w/invalid" live '../bad' 2>/dev/null; then
    echo 'FAIL - invalid locale accepted'; exit 1
fi
test ! -e "$w/invalid"
printf 'locale conf: staged locale values and invalid-input rejection passed\n'
