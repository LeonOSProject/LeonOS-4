#!/bin/sh
# Header-boundary contract tests for the kernel/userland split.
#
# Asserts the rebuild scope the separation depends on:
#   1. a kernel-private header change must NOT recompile userland runtime;
#   2. a UAPI content change must reinstall the export and recompile its
#      userland consumers;
#   3. headers_install must not churn mtimes when nothing changed (otherwise
#      every build would silently rebuild all of userland).
set -u
LC_ALL=C
export LC_ALL

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root" || exit 1

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-header-boundary.XXXXXX") || exit 1
O="$work/out"
failures=0
checks=0

cleanup() {
    [ -n "${KEEP_WORK:-}" ] || rm -rf "$work"
    # Restore the working tree: tests append marker comments to live sources.
    for changed in kernel/ntclks/include/ntclks/sched.h include/uapi/leonos/fs_abi.h; do
        if [ -f "$changed.bak" ]; then mv "$changed.bak" "$changed"; fi
    done
}
trap cleanup EXIT INT TERM

advance_clock() { sleep 1; }

pass() { checks=$((checks + 1)); printf 'ok   - %s\n' "$1"; }
fail() {
    checks=$((checks + 1))
    failures=$((failures + 1))
    printf 'FAIL - %s\n' "$1"
    if [ "$#" -gt 1 ]; then shift; printf '       %s\n' "$@"; fi
}

build() {
    # $1 = log file, rest = targets. Runtime only: it is the userland consumer
    # of the export and pulls musl/pam once per run.
    logfile=$1
    shift
    make -s O="$O" "$@" >"$logfile" 2>&1
    status=$?
    if [ "$status" -ne 0 ]; then
        printf 'FAIL - make exited with %s; assertions below mean nothing\n' "$status"
        tail -n 25 "$logfile" | sed 's/^/       | /'
        exit 1
    fi
}

cc_actions() { grep -cE '^  CC ' "$1" || true; }
install_actions() { grep -cE '^  INSTALL ' "$1" || true; }

export_include="$O/kernel-export/include"

printf '== header boundary ==\n'

# Baseline: config + export + runtime.
build "$work/0.log" defconfig headers_install runtime
[ -f "$export_include/leonos/fs_abi.h" ] && [ -f "$export_include/linux/types.h" ] \
    && pass "export tree installed under $export_include" \
    || fail "export tree missing" "$(find "$O/kernel-export" -type f | head)"

# 1. Kernel-private header change must not recompile the userland runtime.
cp kernel/ntclks/include/ntclks/sched.h kernel/ntclks/include/ntclks/sched.h.bak
printf '\n/* header-boundary probe */\n' >> kernel/ntclks/include/ntclks/sched.h
advance_clock
build "$work/1.log" runtime
if [ "$(cc_actions "$work/1.log")" -eq 0 ]; then
    pass "kernel-private header change recompiles 0 userland objects"
else
    fail "kernel-private header change recompiled userland objects:" \
        "$(grep -E '^  CC ' "$work/1.log" | head)"
fi
mv kernel/ntclks/include/ntclks/sched.h.bak kernel/ntclks/include/ntclks/sched.h

# 2. UAPI content change must reinstall the export and recompile consumers.
cp include/uapi/leonos/fs_abi.h include/uapi/leonos/fs_abi.h.bak
printf '\n/* header-boundary probe */\n' >> include/uapi/leonos/fs_abi.h
advance_clock
build "$work/2.log" runtime
if [ "$(install_actions "$work/2.log")" -ge 1 ] && [ "$(cc_actions "$work/2.log")" -ge 1 ]; then
    pass "UAPI content change reinstalled export and recompiled consumers"
else
    fail "UAPI content change did not propagate" \
        "INSTALL=$(install_actions "$work/2.log") CC=$(cc_actions "$work/2.log")"
fi
mv include/uapi/leonos/fs_abi.h.bak include/uapi/leonos/fs_abi.h

# 3. A forced reinstall with unchanged content must not touch installed files
#    (content-stable publishing; otherwise every build rebuilds userland).
advance_clock
build "$work/3.log" headers_install
before=$work/mtimes-before.txt
after=$work/mtimes-after.txt
find "$export_include" -type f -printf '%p %T@\n' | sort > "$before"
touch configs/header-export.list
advance_clock
build "$work/4.log" headers_install
find "$export_include" -type f -printf '%p %T@\n' | sort > "$after"
if cmp -s "$before" "$after"; then
    pass "unchanged export keeps installed mtimes stable"
else
    fail "installed mtimes churned without content change" \
        "$(diff "$before" "$after" | head)"
fi

printf '%s checks, %s failures\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
