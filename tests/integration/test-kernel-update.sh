#!/bin/sh
# Exercise the actual leonos-kernel-update transaction in a private Linux
# mount namespace. This proves the update script's manifest pairing, commit
# swap and rollback semantics, not LeonOS kernel/VM execution and not the
# TLS/RPR network path (rprfetch is mocked; the fetcher's HTTPS refusal and
# the TLS stack's CA verification are covered by source review only).
# All writes go to a disposable tree under OUTPUT_DIR (default: mktemp).
set -eu
[ "$#" -le 1 ] || { echo 'usage: test-kernel-update.sh [OUTPUT_DIR]' >&2; exit 2; }
out=${1:-$(mktemp -d "${TMPDIR:-/tmp}/leonos-kernel-update-test.XXXXXX")}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd -P)
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
work=$(mktemp -d "$out/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

fails=0
pass() { printf 'ok   - %s\n' "$1"; }
fail() { printf 'fail - %s\n' "$1"; fails=$((fails + 1)); }

# ------------------------------------------------------------------ fixtures
old_kernel='OLD-KERNEL-SYS-PAYLOAD'
new_kernel='NEW-KERNEL-SYS-PAYLOAD'
old_loader='OLD-LOADER-ELF-PAYLOAD'
new_loader='NEW-LOADER-ELF-PAYLOAD'
stale_middle='STALE-MIDDLELAYER-SYS-PAYLOAD'
hash() { printf '%s' "$1" | sha256sum | awk '{print $1}'; }
old_kernel_hash=$(hash "$old_kernel")
new_kernel_hash=$(hash "$new_kernel")
old_loader_hash=$(hash "$old_loader")
new_loader_hash=$(hash "$new_loader")

manifest() { # manifest FORMAT IMAGE RELEASE KERNEL_HASH LOADER_HASH
    printf 'format_version=%s\nimage_version=%s\nversion=%s\nkernel_file=kernel.sys\nkernel_sha256=%s\nloader_file=loader.elf\nloader_sha256=%s\n' \
        "$2" "$3" "$4" "$5" "$6" >"$1"
}

setup_tree() { # setup_tree TREE SYNC_FAIL(0/1)
    tree=$1
    rm -rf "$tree"
    mkdir -p "$tree/boot/leonos" "$tree/etc/leonos" "$tree/run" "$tree/tmp" \
        "$tree/serve" "$tree/usr/bin" "$tree/usr/sbin" "$tree/bin" "$tree/lib" \
        "$tree/lib64" "$tree/sbin"
    printf '%s' "$old_kernel" >"$tree/boot/leonos/kernel.sys"
    printf '%s' "$old_loader" >"$tree/boot/loader.elf"
    printf '%s' "$stale_middle" >"$tree/boot/leonos/middlelayer.sys"
    printf 'RPR_BASE_URL=https://rpr.test.invalid\n' >"$tree/etc/leonos/rpr.conf"
    chmod 1777 "$tree/tmp"
    # Mocks live in /usr/sbin, which is first in the script's fixed PATH.
    cat >"$tree/usr/sbin/rprfetch" <<'EOF'
#!/bin/sh
# test mock: serve fixture files by URL basename from /serve
[ "$#" = 2 ] || exit 2
name=${1##*/}
[ -f "/serve/$name" ] || { printf 'rprfetch-mock: no fixture for %s\n' "$1" >&2; exit 1; }
cp "/serve/$name" "$2"
EOF
    cat >"$tree/usr/sbin/uname" <<'EOF'
#!/bin/sh
# test mock: pin the running kernel release the updater compares against
printf '%s\n' '1.0.0-mock'
EOF
    cat >"$tree/usr/sbin/sync" <<'EOF'
#!/bin/sh
# test mock: deterministic commit-window fault injection when asked
[ -e /run/SYNC_FAIL ] && exit 1
exit 0
EOF
    chmod 755 "$tree/usr/sbin/rprfetch" "$tree/usr/sbin/uname" "$tree/usr/sbin/sync"
    cp "$src/userland/storage/leonos-kernel-update" "$tree/usr/sbin/leonos-kernel-update"
    chmod 755 "$tree/usr/sbin/leonos-kernel-update"
    [ "$2" = 0 ] || : >"$tree/run/SYNC_FAIL"
    # Fixtures are served from inside the chroot at /serve.
    serve=$tree/serve
}

run_update() { # run_update TREE [args...] -> status in $status, log in $work/last.log
    tree=$1
    shift
    status=0
    unshare -Urmp --fork sh -c '
        set -eu
        tree=$1
        shift
        mount --make-rprivate /
        for d in bin lib lib64 sbin usr/bin usr/lib usr/lib64; do
            [ -d "/$d" ] || continue
            mkdir -p "$tree/$d"
            mount --bind "/$d" "$tree/$d"
            mount -o remount,ro,bind "$tree/$d" "$tree/$d"
        done
        # The script redirects diagnostics to /dev/null; without the node the
        # redirection itself fails and the guarded command reports a bogus
        # "already running".
        mkdir -p "$tree/dev"
        touch "$tree/dev/null"
        mount --bind /dev/null "$tree/dev/null"
        exec chroot "$tree" /usr/sbin/leonos-kernel-update "$@"
    ' sh "$tree" "$@" >"$work/last.log" 2>&1 || status=$?
}

assert_untouched() { # assert_untouched TREE LABEL
    if [ "$(cat "$1/boot/leonos/kernel.sys")" = "$old_kernel" ] &&
        [ "$(cat "$1/boot/loader.elf")" = "$old_loader" ] &&
        [ "$(cat "$1/boot/leonos/middlelayer.sys")" = "$stale_middle" ]; then
        pass "$2: boot payload untouched"
    else
        fail "$2: boot payload was modified"
    fi
    if find "$1/boot" -name '*.rpr*' | grep -q .; then
        fail "$2: update left temporary/backup files behind"
    else
        pass "$2: no temporary/backup files left behind"
    fi
}

# --------------------------------------------------------------- scenarios

# 1. success: newer release, matching hashes -> swap in the new pair
tree=$work/tree-success
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.1 1.0.1 "$new_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" = 0 ] && [ "$(cat "$tree/boot/leonos/kernel.sys")" = "$new_kernel" ] &&
    [ "$(cat "$tree/boot/loader.elf")" = "$new_loader" ]; then
    pass "update success: newer release swaps kernel.sys and loader.elf together"
else
    fail "update success: expected exit 0 and both payloads replaced (status=$status)"
fi
if grep -q 'Installed kernel and loader 1.0.1 (build 1.0.1)' "$work/last.log"; then
    pass "update success: reports the installed release"
else
    fail "update success: missing install report"
fi
if [ -e "$tree/boot/leonos/middlelayer.sys" ]; then
    fail "update success: stale middlelayer.sys not retired"
else
    pass "update success: stale middlelayer.sys retired with the commit"
fi
if find "$tree/boot" -name '*.rpr*' | grep -q .; then
    fail "update success: commit left temporary/backup files behind"
else
    pass "update success: completed commit deletes the backups"
fi

# 2. local release already current -> no work, no changes
tree=$work/tree-current
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.0 1.0.0 "$new_kernel_hash" "$new_loader_hash"
run_update "$tree"
if [ "$status" = 0 ] && grep -q 'Kernel image is current' "$work/last.log"; then
    pass "already current: exits 0 without touching the system"
else
    fail "already current: expected exit 0 and 'Kernel image is current' (status=$status)"
fi
assert_untouched "$tree" "already current"

# 3. --check reports availability without modifying anything
tree=$work/tree-check
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.1 1.0.1 "$new_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree" --check
if [ "$status" = 0 ] && grep -q 'Kernel image update available' "$work/last.log"; then
    pass "--check: reports the available release without modifying the system"
else
    fail "--check: expected exit 0 and an availability report (status=$status)"
fi
assert_untouched "$tree" "--check"

# 4. kernel checksum mismatch -> reject before the commit window
tree=$work/tree-badkernel
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.1 1.0.1 "$old_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" = 1 ] && grep -q 'kernel.sys checksum mismatch' "$work/last.log"; then
    pass "checksum mismatch: kernel.sys with a wrong hash is rejected"
else
    fail "checksum mismatch: expected exit 1 and a kernel mismatch (status=$status)"
fi
assert_untouched "$tree" "kernel checksum mismatch"

# 5. loader checksum mismatch -> reject before the commit window
tree=$work/tree-badloader
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.1 1.0.1 "$new_kernel_hash" "$old_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" = 1 ] && grep -q 'loader.elf checksum mismatch' "$work/last.log"; then
    pass "checksum mismatch: loader.elf with a wrong hash is rejected"
else
    fail "checksum mismatch: expected exit 1 and a loader mismatch (status=$status)"
fi
assert_untouched "$tree" "loader checksum mismatch"

# 6. format 1 manifest -> refused instead of installing a half update
tree=$work/tree-format1
setup_tree "$tree" 0
manifest "$serve/release.txt" 1 1.0.1 1.0.1 "$new_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" = 1 ] && grep -q 'Unsupported kernel release manifest' "$work/last.log"; then
    pass "format 1 manifest: refused instead of installing a half update"
else
    fail "format 1 manifest: expected exit 1 and a refusal (status=$status)"
fi
assert_untouched "$tree" "format 1 manifest"

# 7. image/release version disagreement -> rejected
tree=$work/tree-disagree
setup_tree "$tree" 0
manifest "$serve/release.txt" 2 1.0.2 1.0.1 "$new_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" = 1 ] && grep -q 'Remote image and release versions disagree' "$work/last.log"; then
    pass "version pairing: disagreeing image/release versions are rejected"
else
    fail "version pairing: expected exit 1 and a disagreement (status=$status)"
fi
assert_untouched "$tree" "version disagreement"

# 8. failure inside the commit window -> the previous kernel, loader and
#    retired payload are rolled back (fault injection: sync refuses)
tree=$work/tree-rollback
setup_tree "$tree" 1
manifest "$serve/release.txt" 2 1.0.1 1.0.1 "$new_kernel_hash" "$new_loader_hash"
printf '%s' "$new_kernel" >"$serve/kernel.sys"
printf '%s' "$new_loader" >"$serve/loader.elf"
run_update "$tree"
if [ "$status" != 0 ]; then
    pass "commit failure: update exits non-zero when the commit cannot complete"
else
    fail "commit failure: expected a non-zero exit (status=$status)"
fi
assert_untouched "$tree" "commit failure rollback"

printf '\n'
if [ "$fails" = 0 ]; then
    printf 'test-kernel-update: all checks passed\n'
else
    printf 'test-kernel-update: %s checks failed\n' "$fails"
    exit 1
fi
