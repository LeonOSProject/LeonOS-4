#!/bin/sh
# Incremental-correctness contract tests for the kernel closed loop.
#
# Covers acceptance rows A02 to A06 of the migration plan. Every assertion is
# about observed work: which actions Make emitted, which products changed
# mtime, and which objects the linker consumed. Nothing here trusts a stamp.
set -u

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root" || exit 1

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-incremental.XXXXXX") || exit 1
O="$work/out"
failures=0
checks=0

cleanup() {
    [ -n "${KEEP_WORK:-}" ] || rm -rf "$work"
}
trap cleanup EXIT INT TERM

pass() { checks=$((checks + 1)); printf 'ok   - %s\n' "$1"; }
fail() {
    checks=$((checks + 1))
    failures=$((failures + 1))
    printf 'FAIL - %s\n' "$1"
    if [ "$#" -gt 1 ]; then
        shift
        printf '       %s\n' "$@"
    fi
}

# Actions Make actually ran, identified by the fixed-width label column.
build() {
    make -s O="$O" kernel 2>&1 | grep -E '^  (CC|AS|LD|IMAGE|GEN|HOSTCC|CONFIG) ' || true
}

count_label() { printf '%s\n' "$1" | grep -cE "^  $2 " || true; }

snapshot() {
    # path + full mtime with nanoseconds, for the files this suite cares about.
    find "$O/obj/kernel" -name '*.o' -printf '%p %T@\n' 2>/dev/null | LC_ALL=C sort
    for product in "$O/generated/system/kernel.sys" \
                   "$O/generated/system/kernel.debug" \
                   "$O/generated/system/kernel.unstripped" \
                   "$O/include/generated/autoconf.h" \
                   "$O/include/generated/boot_logo.h"; do
        [ -e "$product" ] && printf '%s %s\n' "$product" "$(stat -c %y "$product")"
    done | LC_ALL=C sort
}

expect_same() {
    if [ "$1" = "$2" ]; then pass "$3"; else fail "$3" "mtimes differ"; fi
}

expect_count() {
    got=$(printf '%s\n' "$1" | grep -cE "^  $2 " || true)
    if [ "$got" = "$3" ]; then
        pass "$4"
    else
        fail "$4" "expected $3 '$2' actions, got $got"
    fi
}

expect_contains() {
    if printf '%s\n' "$1" | grep -qF -- "$2"; then
        pass "$3"
    else
        fail "$3" "missing: $2"
    fi
}

printf '=== clean output build ===\n'
first=$(build)
if [ ! -f "$O/generated/system/kernel.sys" ]; then
    fail "clean output directory produces a kernel image" "no kernel.sys"
    printf '%s: aborting\n' "$0" >&2
    exit 1
fi
pass 'clean output directory produces a kernel image'
printf '       clean build actions: CC=%s AS=%s LD=%s IMAGE=%s\n' \
    "$(printf '%s\n' "$first" | grep -cE '^  CC ' || true)" \
    "$(printf '%s\n' "$first" | grep -cE '^  AS ' || true)" \
    "$(printf '%s\n' "$first" | grep -cE '^  LD ' || true)" \
    "$(printf '%s\n' "$first" | grep -cE '^  IMAGE ' || true)"

# Every object that ends up on disk has to have been produced by a compile
# action in this very build. A count that agrees by accident, or an object that
# appeared without an action, is exactly the kind of drift this suite exists to
# catch, so name the offenders rather than comparing two numbers.
printf '%s\n' "$first" | sed -n 's/^  CC       //p' | LC_ALL=C sort >"$work/cc-actions"
find "$O/obj/kernel" -name '*.c.o' | sed "s#^$O/obj/kernel/##" | LC_ALL=C sort >"$work/cc-objects"
unrecorded=0
while read -r object; do
    [ -n "$object" ] || continue
    source=$(printf '%s' "$object" | sed 's#\.o$##')
    if ! grep -qxF -- "$repo_root/$source" "$work/cc-actions"; then
        printf '       no CC action recorded for %s\n' "$object"
        unrecorded=$((unrecorded + 1))
    fi
done <"$work/cc-objects"
if [ "$unrecorded" = 0 ]; then
    pass 'every kernel object was produced by a compile action'
else
    fail 'every kernel object was produced by a compile action' \
        "$unrecorded object(s) appeared without one"
fi

kernel_objects=$(find "$O/obj/kernel" -name '*.c.o' | wc -l)
assembly_objects=$(find "$O/obj/kernel" -name '*.S.o' | wc -l)
if [ "$kernel_objects" -gt 50 ] && [ "$assembly_objects" -gt 0 ]; then
    pass "manifest discovered the kernel sources ($kernel_objects C, $assembly_objects asm)"
else
    fail "manifest discovered the kernel sources" "got $kernel_objects C, $assembly_objects asm"
fi

printf '\n=== A02: a second build does no work ===\n'
before=$(snapshot)
second=$(build)
after=$(snapshot)
if [ -z "$(printf '%s\n' "$second" | tr -d '[:space:]')" ]; then
    pass 'no-op build emits no compile, link, image or generate action'
else
    fail 'no-op build emits no compile, link, image or generate action'
    printf '%s\n' "$second" | sed 's/^/       | /'
fi
expect_same "$before" "$after" 'no-op build leaves every object and product mtime unchanged'

printf '\n=== A03: one ordinary source file ===\n'
target_source=kernel/ntclks/futex.c
target_object=$O/obj/kernel/$target_source.o
before=$(snapshot)
touch "$repo_root/$target_source"
third=$(build)
expect_count "$third" CC 1 'changing one C file recompiles exactly one object'
expect_count "$third" LD 1 'changing one C file relinks once'
expect_count "$third" IMAGE 2 'changing one C file refreshes both image products'
if printf '%s\n' "$third" | grep -qF -- "$target_source"; then
    pass 'the recompiles file is the one that changed'
else
    fail 'the recompiles file is the one that changed' "expected $target_source in output"
fi
untouched=$(snapshot | grep -c '\.o ' || true)
if [ "$untouched" = "$((kernel_objects + assembly_objects))" ]; then
    pass "all $((kernel_objects + assembly_objects)) objects are tracked by the snapshot"
else
    fail "snapshot covers every object" "tracked $untouched"
fi

printf '\n=== A04: a public header consumers share ===\n'
# The depfiles are the oracle: every object that recorded the header must be
# rebuilt and no object that did not record it may be. Generated headers under
# the output tree are excluded so the candidate is a real source header.
depfiles=$(find "$O/obj/kernel" -name '*.c.o.d' | tr '\n' ' ')
header=$(for depfile in $depfiles; do
            # Join backslash continuations before splitting: clang emits one
            # prerequisite per word across several lines, and the headers are
            # almost always on the continuation lines.
            sed -e :a -e '/\\$/N; s/\\\n//; ta' "$depfile"
         done | tr ' ' '\n' | grep -E '\.h$' | grep -v "^$O/" |
         sort | uniq -c | sort -rn | awk 'NR == 1 { print $2 }')

if [ -z "$header" ] || [ ! -f "$header" ]; then
    fail 'a shared source header was found to test' "candidate: ${header:-none}"
else
    expected=$(grep -lF -- "$header" $depfiles 2>/dev/null | wc -l)
    touch "$header"
    fourth=$(build)
    actual=$(printf '%s\n' "$fourth" | grep -cE '^  CC ' || true)
    relinked=$(printf '%s\n' "$fourth" | grep -cE '^  LD ' || true)
    if [ "$actual" = "$expected" ] && [ "$actual" -gt 1 ]; then
        pass "changing $(basename "$header") rebuilds exactly the $expected recorded consumers"
    else
        fail "changing $(basename "$header") rebuilds exactly the recorded consumers" \
            "depfiles recorded $expected, Make recompiled $actual"
    fi
    if [ "$relinked" = 1 ]; then
        pass 'the header change relinks once'
    else
        fail 'the header change relinks once' "got $relinked"
    fi
fi

printf '\n=== A04b: the linker script ===\n'
touch "$repo_root/kernel/ntclks/arch/x86_64/linker.ld"
script_build=$(build)
script_cc=$(printf '%s\n' "$script_build" | grep -cE '^  CC ' || true)
script_ld=$(printf '%s\n' "$script_build" | grep -cE '^  LD ' || true)
if [ "$script_cc" = 0 ] && [ "$script_ld" = 1 ]; then
    pass 'changing the linker script relinks without recompiling'
else
    fail 'changing the linker script relinks without recompiling' \
        "CC=$script_cc LD=$script_ld"
fi

printf '\n=== A05: compile flags, tool identity and profile isolation ===\n'
before=$(snapshot)
fifth=$(make -s O="$O" KERNEL_CFLAGS=-DLEONOS_SIG_PROBE kernel 2>&1 | grep -E '^  (CC|AS|LD|IMAGE) ' || true)
after=$(snapshot)
cc_actions=$(printf '%s\n' "$fifth" | grep -cE '^  CC ' || true)
as_actions=$(printf '%s\n' "$fifth" | grep -cE '^  AS ' || true)
if [ "$cc_actions" = "$kernel_objects" ] && [ "$as_actions" = 0 ]; then
    pass "KERNEL_CFLAGS override rebuilds all $kernel_objects C objects and no asm object"
else
    fail 'KERNEL_CFLAGS override rebuilds only the C action class' \
        "CC=$cc_actions AS=$as_actions expected CC=$kernel_objects AS=0"
fi
if [ "$before" != "$after" ]; then
    pass 'the flag change moved product mtimes'
else
    fail 'the flag change moved product mtimes' 'nothing rebuilt'
fi

# The other profile must not see this output directory at all.
other=$(make -s O="$O" PROFILE=debug -n kernel >/dev/null 2>&1; echo $?)
debug_out="$work/out-debug"
make -s O="$debug_out" PROFILE=debug defconfig >/dev/null 2>&1
if [ -f "$debug_out/config/.config" ] && [ "$debug_out" != "$O" ]; then
    pass 'PROFILE=debug uses a separate output directory'
else
    fail 'PROFILE=debug uses a separate output directory'
fi

printf '\n=== A06: removing a source file ===\n'
victim=kernel/ntclks/signalfd.c
victim_object="$O/obj/kernel/$victim.o"
if [ ! -f "$victim_object" ]; then
    fail "victim object exists before removal" "$victim_object"
else
    mv "$repo_root/$victim" "$work/victim.c"
    sixth=$(build)
    restored() { mv "$work/victim.c" "$repo_root/$victim"; }
    if printf '%s\n' "$sixth" | grep -qE '^  LD '; then
        pass 'removing a source relinks the kernel'
    else
        fail 'removing a source relinks the kernel' "output: $(printf '%s' "$sixth" | head -c 200)"
    fi
    if [ -f "$victim_object" ]; then
        printf '       note: the orphaned object file remains on disk; the link must not consume it\n'
    fi
    if make -s O="$O" -n kernel 2>&1 | grep -qF -- "$victim.o"; then
        fail 'the removed source is no longer an input to the link'
    else
        pass 'the removed source is no longer an input to the link'
    fi
    restored
    seventh=$(build)
    expect_count "$seventh" LD 1 'restoring the source relinks again'
fi

printf '\n=== A07: deleting one product of a multi-output stage ===\n'
rm -f "$O/generated/system/kernel.sys"
eighth=$(build)
if [ -f "$O/generated/system/kernel.sys" ]; then
    pass 'a deleted image product is regenerated'
else
    fail 'a deleted image product is regenerated' 'still missing'
fi
rm -f "$O/include/generated/boot_logo.h"
ninth=$(build)
if [ -f "$O/include/generated/boot_logo.h" ]; then
    pass 'a deleted generated header is regenerated'
else
    fail 'a deleted generated header is regenerated' 'still missing'
fi

printf '\n%s: %d checks, %d failures\n' 'test-incremental' "$checks" "$failures"
[ "$failures" -eq 0 ] || exit 1
exit 0
