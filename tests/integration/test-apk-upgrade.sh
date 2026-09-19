#!/bin/sh
# Exercise the actual guest updater in a private Linux mount namespace. This proves package
# transactions, not LeonOS kernel/VM execution. All writes go to a private copy.
set -eu
[ "$#" = 2 ] || { echo 'usage: test-apk-upgrade OUTPUT_TREE OLD_MANAGED_ROOT' >&2; exit 2; }
out=$(CDPATH= cd -- "$1" && pwd -P)
old=$(CDPATH= cd -- "$2" && pwd -P)
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
key=${APK_SIGNING_KEY:-$HOME/.local/share/leonos/apk-signing/key.pem}
work=$(mktemp -d "$out/apk-upgrade-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
apk=$out/upstream/apk/apk.static
cp -a "$out/rootfs/managed" "$work/runner"
cp -a "$old" "$work/runner/target"
cp -a "$out/packages/apk/repository" "$work/runner/newrepo"
if [ -n "${PRIOR_BAD_REPOSITORY:-}" ]; then
    cp -a "$PRIOR_BAD_REPOSITORY" "$work/runner/badrepo"
fi
mkdir -p "$work/external/usr/share/external-fixture" "$work/runner/fixture"
printf 'external package preserved\n' > "$work/external/usr/share/external-fixture/data"
unshare -Ur "$apk" mkpkg --files "$work/external" --output "$work/runner/fixture/external.apk" \
    --info name:external-fixture --info version:1.0-r0 --info arch:x86_64 --sign-key "$key"
prior_errors=$(awk '/^f:/ && substr($0,3) ~ /[fs]/ {n++} END {print n+0}' "$work/runner/target/lib/apk/db/installed")
status=0
unshare -Ur "$apk" --root "$work/runner/target" --repositories-file /dev/null add "$work/runner/fixture/external.apk" > "$work/add.log" 2>&1 || status=$?
cat "$work/add.log"
[ "$status" = "$prior_errors" ]
! grep -q '^ERROR:' "$work/add.log"
unshare -Ur "$apk" --root "$work/runner/target" info --exists external-fixture
for tool in ar strings; do
    if [ -f "$old/usr/bin/$tool" ] && [ ! -L "$old/usr/bin/$tool" ]; then
        cmp "$old/usr/bin/$tool" "$work/runner/target/usr/bin/$tool"
    fi
done
printf 'local-admin-setting=yes\n' > "$work/runner/target/etc/leonos/upgrade-fixture.conf"
cp "$work/runner/target/etc/apk/world" "$work/world-before"
awk '/^P:/{p=$0} /^V:/{if(p=="P:external-fixture")print}' "$work/runner/target/lib/apk/db/installed" > "$work/external-before"
# Use the new updater, with the same /target and signed-repository contract as
# the installer; preserve the previously installed public trust key.
cp "$src/userland/storage/leonos-apk-update" "$work/runner/usr/lib/leonos/leonos-apk-update"
# pivot_root, rather than chroot, lets APK create its script sandbox on Linux.
# The old root remains mounted only inside this short-lived private namespace.
run_update() {
unshare -Urmp --fork sh -eu -c '
    mount --make-rprivate /
    mount --bind "$1" "$1"
    cd "$1"
    mkdir -p .oldroot
    pivot_root . .oldroot
    exec /usr/lib/leonos/leonos-apk-update /target "$2"
' sh "$work/runner" "$1"
}
if [ -n "${PRIOR_BAD_REPOSITORY:-}" ]; then
    if run_update /badrepo > "$work/bad.log" 2>&1; then
        echo 'expected the old BusyBox package to conflict with binutils' >&2
        exit 1
    fi
    grep 'trying to overwrite usr/bin/ar owned by binutils' "$work/bad.log"
    grep 'trying to overwrite usr/bin/strings owned by binutils' "$work/bad.log"
fi
run_update /newrepo
cmp "$work/world-before" "$work/runner/target/etc/apk/world"
for tool in ar strings; do
    if [ -f "$old/usr/bin/$tool" ] && [ ! -L "$old/usr/bin/$tool" ]; then
        cmp "$old/usr/bin/$tool" "$work/runner/target/usr/bin/$tool"
    fi
done
printf 'local-admin-setting=yes\n' | cmp - "$work/runner/target/etc/leonos/upgrade-fixture.conf"
printf 'external package preserved\n' | cmp - "$work/runner/target/usr/share/external-fixture/data"
awk '/^P:/{p=$0} /^V:/{if(p=="P:external-fixture")print}' "$work/runner/target/lib/apk/db/installed" | cmp - "$work/external-before"
new_version=$(sed -n 's/^  "version": "\([^"]*\)",/\1/p' "$out/packages/apk/manifest.json")
awk -v wanted="$new_version" '/^P:/{p=substr($0,3)} /^V:/{if(p~/^leonos-/ && substr($0,3)!=wanted)exit 1}' "$work/runner/target/lib/apk/db/installed"
printf 'APK upgrade: legacy versions upgraded; external package, world and local configuration preserved\n'
