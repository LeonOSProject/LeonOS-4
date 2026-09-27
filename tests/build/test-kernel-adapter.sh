#!/bin/sh
# Contract tests for the ntclks kernel adapter (mk/kernel.mk, mk/headers.mk).
#
# The kernel products are built by the standalone checkout and published here;
# this suite covers the adapter's failure and recovery surface: a missing
# checkout fails with an actionable error, a dirty checkout is rebuilt and
# re-published, a deleted product is restored, parallel invocations on separate
# output directories succeed, and a build failure in the checkout surfaces the
# sub-make error and publishes no half-products. The destructive cases run
# against a scratch copy of the checkout (a fixture), never the real one.
set -u
LC_ALL=C
export LC_ALL

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root" || exit 1

# The kernel checkout under test (env-overridable, see tests/build/test-incremental.sh).
ntclks=${NTCLKS_DIR:-$repo_root/kernel/ntclks}

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-adapter.XXXXXX") || exit 1
O="$work/out"
failures=0
checks=0

cleanup() {
    [ -n "${KEEP_WORK:-}" ] || rm -rf "$work"
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
    # $1 = log file, $2 = output directory, rest = extra make arguments.
    logfile=$1
    shift
    outdir=$1
    shift
    make -s -j"$(nproc)" O="$outdir" "$@" kernel >"$logfile" 2>&1
}

printf '=== (a) a missing kernel checkout fails with an actionable error ===\n'
for goal in kernel headers_install; do
    if make -s O="$work/missing" NTCLKS_DIR="$work/no-such-checkout" "$goal" \
            >"$work/missing-$goal.log" 2>&1; then
        fail "missing checkout refuses '$goal'" 'make exited 0'
    else
        pass "missing checkout refuses '$goal'"
    fi
    if grep -q 'NTCLKS_DIR' "$work/missing-$goal.log"; then
        pass "the '$goal' error names NTCLKS_DIR"
    else
        fail "the '$goal' error names NTCLKS_DIR" \
            "$(head -c 200 "$work/missing-$goal.log")"
    fi
done

printf '\n=== (b) a dirty checkout source rebuilds and re-publishes ===\n'
if ! build "$work/base.log" "$O"; then
    fail 'the baseline kernel build succeeds' "see $work/base.log"
    tail -n 15 "$work/base.log" | sed 's/^/       | /'
    printf '%s: aborting\n' "$0" >&2
    exit 1
fi
pass 'the baseline kernel build succeeds'
find "$O/generated" -type f -printf '%p %T@\n' | LC_ALL=C sort >"$work/published-before"
loader_hash_before=$(sha256sum "$O/generated/boot/loader.elf" | cut -d' ' -f1)
driver_hash_before=$(sha256sum "$O/generated/drivers/mouse.drv" | cut -d' ' -f1)

advance_clock
touch "$ntclks/kernel/ntclks/futex.c"
if build "$work/dirty.log" "$O"; then
    pass 'a touched checkout source is rebuilt'
else
    fail 'a touched checkout source is rebuilt' 'make failed'
fi
if grep -qF -- "$ntclks/kernel/ntclks/futex.c" "$work/dirty.log" &&
        grep -qE '^  (LD|IMAGE) ' "$work/dirty.log"; then
    pass 'the sub-make relinks and refreshes the affected products'
else
    fail 'the sub-make relinks and refreshes the affected products' \
        "$(grep -E '^  (CC|LD|IMAGE) ' "$work/dirty.log" | head -3 | tr '\n' ' ')"
fi
if grep -qE 'loader\.elf|\.drv' "$work/dirty.log"; then
    fail 'a kernel source touch leaves the loader and drivers alone' \
        "$(grep -E 'loader\.elf|\.drv' "$work/dirty.log" | head -2 | tr '\n' ' ')"
else
    pass 'a kernel source touch leaves the loader and drivers alone'
fi
if [ "$loader_hash_before" = "$(sha256sum "$O/generated/boot/loader.elf" | cut -d' ' -f1)" ] &&
        [ "$driver_hash_before" = "$(sha256sum "$O/generated/drivers/mouse.drv" | cut -d' ' -f1)" ]; then
    pass 'untouched products keep their bytes'
else
    fail 'untouched products keep their bytes' 'loader.elf or mouse.drv moved'
fi
# The publish path runs on every delegation; the published set must track what
# the kernel checkout installed byte for byte.
synced=yes
for pair in system/kernel.sys:kernel.sys system/kernel.debug:kernel.debug \
            system/kerneldebug.sys:kerneldebug.sys boot/loader.elf:loader.elf \
            drivers/mouse.drv:mouse.drv drivers/serial.drv:serial.drv \
            drivers/e1000.drv:e1000.drv drivers/ac97.drv:ac97.drv \
            drivers/es1371.drv:es1371.drv; do
    rel=${pair%%:*}
    name=${pair#*:}
    cmp -s "$O/generated/$rel" "$O/kernel-install/$name" || synced=no
done
if [ "$synced" = yes ]; then
    pass 'published products are byte-identical to the kernel install output'
else
    fail 'published products are byte-identical to the kernel install output' \
        'a published file drifted from the install'
fi

printf '\n=== (c) a deleted published product is restored ===\n'
kernel_checksum=$(sha256sum "$O/generated/system/kernel.sys" | cut -d' ' -f1)
rm -f "$O/generated/system/kernel.sys" "$O/generated/drivers/mouse.drv"
if build "$work/restore.log" "$O" &&
        [ -s "$O/generated/system/kernel.sys" ] && [ -s "$O/generated/drivers/mouse.drv" ]; then
    pass 'the deleted kernel.sys and mouse.drv are restored'
else
    fail 'the deleted kernel.sys and mouse.drv are restored' 'still missing'
fi
if [ "$(sha256sum "$O/generated/system/kernel.sys" | cut -d' ' -f1)" = "$kernel_checksum" ]; then
    pass 'the restored kernel.sys is byte-identical'
else
    fail 'the restored kernel.sys is byte-identical' 'bytes differ'
fi

printf '\n=== (d) two parallel kernel builds on different output directories ===\n'
build "$work/par1.log" "$work/par1" &
par1_pid=$!
build "$work/par2.log" "$work/par2" &
par2_pid=$!
wait "$par1_pid"; par1_status=$?
wait "$par2_pid"; par2_status=$?
if [ "$par1_status" -eq 0 ] && [ "$par2_status" -eq 0 ]; then
    pass 'both parallel builds succeed'
else
    fail 'both parallel builds succeed' "statuses $par1_status $par2_status"
    tail -n 8 "$work/par1.log" "$work/par2.log" | sed 's/^/       | /'
fi
for tree in "$work/par1" "$work/par2"; do
    if [ -s "$tree/generated/system/kernel.sys" ] &&
            cmp -s "$tree/generated/system/kernel.sys" "$O/generated/system/kernel.sys"; then
        pass "$(basename "$tree") produces the same kernel image"
    else
        fail "$(basename "$tree") produces the same kernel image" 'missing or differs'
    fi
done

printf '\n=== (e) a build failure in the checkout publishes no half-products ===\n'
# Fixture: a scratch copy of the checkout. The real checkout is never edited.
fixture=$work/fixture
cp -a "$ntclks" "$fixture" || exit 1
broken=$fixture/kernel/ntclks/futex.c
fail_out=$work/fail-out

printf '\nthis is a deliberate syntax error in the fixture\n' >>"$broken"
if build "$work/fail.log" "$fail_out" NTCLKS_DIR="$fixture"; then
    fail 'a syntax error in the checkout fails the parent build' 'make exited 0'
else
    pass 'a syntax error in the checkout fails the parent build'
fi
if grep -qF 'futex.c' "$work/fail.log"; then
    pass 'the sub-make compile error surfaces in the build output'
else
    fail 'the sub-make compile error surfaces in the build output' \
        "$(tail -c 300 "$work/fail.log")"
fi
if [ ! -e "$fail_out/generated/system/kernel.sys" ] &&
        [ ! -e "$fail_out/generated/boot/loader.elf" ] &&
        [ ! -e "$fail_out/generated/drivers/mouse.drv" ]; then
    pass 'the failed build publishes no half-products'
else
    fail 'the failed build publishes no half-products' 'a product was published'
fi

# Recovery: fixing the source makes the same tree build and publish.
cp "$ntclks/kernel/ntclks/futex.c" "$broken"
if build "$work/recover.log" "$fail_out" NTCLKS_DIR="$fixture" &&
        [ -s "$fail_out/generated/system/kernel.sys" ]; then
    pass 'the recovered fixture builds and publishes'
else
    fail 'the recovered fixture builds and publishes' 'make failed or no kernel.sys'
fi
recovered_checksum=$(sha256sum "$fail_out/generated/system/kernel.sys" | cut -d' ' -f1)

# A real content change must re-publish with new bytes (the dirty-modification
# path), and restoring the content must restore the image byte for byte.
printf '\nint leonos_probe_marker = 1;\n' >>"$broken"
if build "$work/content.log" "$fail_out" NTCLKS_DIR="$fixture"; then
    content_checksum=$(sha256sum "$fail_out/generated/system/kernel.sys" | cut -d' ' -f1)
    if [ "$content_checksum" != "$recovered_checksum" ]; then
        pass 'a content change in the checkout re-publishes a new kernel.sys'
    else
        fail 'a content change in the checkout re-publishes a new kernel.sys' \
            'kernel.sys bytes did not move'
    fi
else
    fail 'a content change in the checkout re-publishes a new kernel.sys' 'make failed'
fi
cp "$ntclks/kernel/ntclks/futex.c" "$broken"
if build "$work/restore-content.log" "$fail_out" NTCLKS_DIR="$fixture" &&
        [ "$(sha256sum "$fail_out/generated/system/kernel.sys" | cut -d' ' -f1)" = \
          "$recovered_checksum" ]; then
    pass 'restoring the source restores the image byte-identical'
else
    fail 'restoring the source restores the image byte-identical' 'bytes differ'
fi

# Failure after products exist: the published set must stay the old complete
# set, not gain a half-published mix.
loader_checksum=$(sha256sum "$fail_out/generated/boot/loader.elf" | cut -d' ' -f1)
printf '\nthis is a deliberate syntax error in the fixture\n' >>"$broken"
rm -f "$fail_out/generated/system/kernel.sys"
if build "$work/fail2.log" "$fail_out" NTCLKS_DIR="$fixture"; then
    fail 'a second failure keeps the parent build nonzero' 'make exited 0'
else
    pass 'a second failure keeps the parent build nonzero'
fi
if [ ! -e "$fail_out/generated/system/kernel.sys" ]; then
    pass 'a failed build does not restore a deleted product'
else
    fail 'a failed build does not restore a deleted product' 'kernel.sys reappeared'
fi
if [ "$(sha256sum "$fail_out/generated/boot/loader.elf" | cut -d' ' -f1)" = \
      "$loader_checksum" ]; then
    pass 'surviving products keep their bytes across a failed build'
else
    fail 'surviving products keep their bytes across a failed build' 'loader.elf moved'
fi

printf '\n%s: %d checks, %d failures\n' 'test-kernel-adapter' "$checks" "$failures"
[ "$failures" -eq 0 ] || exit 1
exit 0
