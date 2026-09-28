#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
migrator="$root/system/rootfs/usr/lib/reliefos/reliefos-migrate"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
fixture="$work/root"

mkdir -p "$fixture/etc/leonos" "$fixture/etc/reliefos" \
    "$fixture/var/lib/leonos"
printf 'theme=win95\n' > "$fixture/etc/leonos/display.conf"
printf 'theme=metro\n' > "$fixture/etc/reliefos/display.conf"
printf 'userdb-v1\n\000record\n' > "$fixture/var/lib/leonos/users.db"
chmod 0750 "$fixture/var/lib/leonos"
chmod 0640 "$fixture/var/lib/leonos/users.db"
cp "$fixture/var/lib/leonos/users.db" "$work/users.expected"
sha256sum "$fixture/var/lib/leonos/users.db" > "$work/users.old.sha256"

if sh "$migrator" "$fixture" > "$work/first.log" 2>&1; then
    :
else
    cat "$work/first.log" >&2
    exit 1
fi
grep -qx 'theme=win95' "$fixture/etc/leonos/display.conf"
grep -qx 'theme=metro' "$fixture/etc/reliefos/display.conf"
cmp "$work/users.expected" "$fixture/var/lib/reliefos/users.db"
sha256sum -c "$work/users.old.sha256"
test -f "$fixture/var/lib/leonos/users.db"
test "$(stat -c %a "$fixture/var/lib/reliefos")" = 750
test "$(stat -c %u:%g "$fixture/var/lib/reliefos")" = "$(stat -c %u:%g "$fixture/var/lib/leonos")"
test "$(stat -c %a "$fixture/var/lib/reliefos/users.db")" = 640
test "$(stat -c %u:%g "$fixture/var/lib/reliefos/users.db")" = "$(stat -c %u:%g "$fixture/var/lib/leonos/users.db")"
grep -q 'exists; keeping' "$work/first.log"

cp "$fixture/var/lib/reliefos/users.db" "$work/users.first"
sh "$migrator" "$fixture" > "$work/second.log" 2>&1
cmp "$work/users.first" "$fixture/var/lib/reliefos/users.db"
cmp "$fixture/var/lib/leonos/users.db" "$fixture/var/lib/reliefos/users.db"

blocked="$work/blocked-root"
mkdir -p "$blocked/etc/leonos" "$blocked/etc/reliefos" "$blocked/var/lib/leonos"
printf 'theme=win95\n' > "$blocked/etc/leonos/display.conf"
printf 'userdb-v1\n' > "$blocked/var/lib/leonos/users.db"
chmod 0755 "$blocked" "$blocked/etc" "$blocked/etc/leonos" \
    "$blocked/var" "$blocked/var/lib" "$blocked/var/lib/leonos"
chmod 0555 "$blocked/etc/reliefos" "$blocked/var/lib"

if [ "$(id -u)" -eq 0 ]; then
    if ! command -v runuser >/dev/null 2>&1; then
        echo 'FAIL - runuser is required to test an unwritable migration target as root' >&2
        exit 1
    fi
    if runuser -u nobody -- sh "$migrator" "$blocked" > "$work/blocked.log" 2>&1; then
        echo 'FAIL - migration unexpectedly wrote to an unwritable target' >&2
        exit 1
    fi
else
    if sh "$migrator" "$blocked" > "$work/blocked.log" 2>&1; then
        echo 'FAIL - migration unexpectedly wrote to an unwritable target' >&2
        exit 1
    fi
fi
grep -Eiq 'cannot|failed|permission|write' "$work/blocked.log"
chmod 0755 "$blocked/etc/reliefos" "$blocked/var/lib"

echo 'ok - legacy config and account data migrate without overwrite or data loss'
