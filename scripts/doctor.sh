#!/bin/sh
# Grouped host / target / third-party checks. Exits non-zero on anything missing
# so CI and a fresh checkout get a single actionable report (plan section 6.2).
set -u

failures=0
group() { printf '%s\n' "$1"; }
missing() { printf '  MISSING  %s\n' "$1"; failures=$((failures + 1)); }
present() { printf '  ok       %s -> %s\n' "$1" "$2"; }

check_tool() {
    found=$(command -v "$1" 2>/dev/null || true)
    if [ -z "$found" ]; then
        missing "$1"
    else
        present "$1" "$found"
    fi
}

printf 'LeonOS build doctor\n'
printf 'source root\t%s\n' "${SRC:-.}"
printf 'output dir\t%s\n' "${O:-unset}"
printf 'arch\t\t%s\nprofile\t%s\n' "${ARCH:-unset}" "${PROFILE:-unset}"
printf 'toolchain\t%s\n\n' "${TOOLCHAIN:-unset}"

group 'host tools (required by make itself and the C helpers)'
for tool in sh grep sed awk find sort cmp mv ln mkdir rm printf date tr head \
            cut expr uname nproc xz tar patch curl; do
    check_tool "$tool"
done
printf '  HOSTCC     %s\n' "${HOSTCC:-cc}"
if ! command -v "${HOSTCC:-cc}" >/dev/null 2>&1; then
    missing "HOSTCC=${HOSTCC:-cc}"
fi

group ''
group 'target toolchain (validated by actually compiling)'
# Named explicitly: HOSTCC and TARGET_CC must be visible as two different
# variables in one output, or "the host compiler works" proves nothing about the
# cross compiler.
printf '  TARGET_CC  %s\n' "${TARGET_CC:-unset}"
printf '  TARGET_LD  %s\n' "${TARGET_LD:-unset}"
for tool in "$TARGET_CC" "$TARGET_LD" "$TARGET_AR" "$TARGET_OBJCOPY" \
            "$TARGET_STRIP" "$TARGET_RUSTC"; do
    check_tool "$tool"
done

# A clang on PATH that cannot emit the freestanding triple, or that has no
# compiler-rt builtins archive, is worse than no compiler at all: the link would
# fail much later with a confusing message.
if command -v "$TARGET_CC" >/dev/null 2>&1; then
    probe_dir=$(mktemp -d 2>/dev/null || echo "")
    if [ -n "$probe_dir" ]; then
        printf 'int main(void){return 0;}\n' > "$probe_dir/probe.c"
        if "$TARGET_CC" -target "${TARGET_TRIPLE_KERNEL}" -ffreestanding -c \
                "$probe_dir/probe.c" -o "$probe_dir/probe.o" >/dev/null 2>&1; then
            printf '  ok       %s can target %s\n' "$TARGET_CC" "$TARGET_TRIPLE_KERNEL"
        else
            missing "$TARGET_CC cannot target $TARGET_TRIPLE_KERNEL"
        fi
        resource_dir=$("$TARGET_CC" -print-resource-dir 2>/dev/null || echo "")
        if [ -n "$resource_dir" ] && [ -d "$resource_dir/include" ]; then
            printf '  ok       compiler-rt headers %s\n' "$resource_dir/include"
        else
            missing "$TARGET_CC compiler-rt headers (clang -print-resource-dir)"
        fi
        if "$TARGET_LD" --version >/dev/null 2>&1; then
            printf '  ok       linker %s responds\n' "$TARGET_LD"
        else
            missing "$TARGET_LD is not runnable"
        fi
        rm -rf "$probe_dir"
    fi
fi

group ''
group 'image and run tools (needed by the image and run phases)'
for tool in mke2fs mkfs.fat mformat mcopy xorriso qemu-img qemu-system-x86_64; do
    check_tool "$tool"
done

# LeonOS 4 boots through UEFI, so a QEMU without firmware cannot be tested at
# all. Reporting this as satisfied is how a run target ends up looking green
# while the guest never starts.
firmware_found=''
for candidate in "${SRC:-.}/buildsystem/firmware/OVMF.fd" \
                 "${SRC:-.}/build/firmware/OVMF.fd" \
                 /usr/share/edk2/x64/OVMF.4m.fd /usr/share/ovmf/OVMF.fd \
                 /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/qemu/OVMF.fd; do
    if [ -f "$candidate" ] && [ "$(wc -c < "$candidate")" -ge 1048576 ]; then
        firmware_found=$candidate
        break
    fi
done
if [ -n "$firmware_found" ]; then
    printf '  ok       UEFI firmware %s\n' "$firmware_found"
else
    missing 'UEFI firmware for QEMU (install edk2-ovmf, or place OVMF.fd in buildsystem/firmware/)'
fi

group ''
group 'third-party build inputs'
if [ -f third_party/kconfig-frontends/configure.ac ]; then
    printf '  ok       kconfig-frontends submodule present\n'
else
    missing 'third_party/kconfig-frontends (git submodule update --init --recursive)'
fi
if [ -f third_party/zlib/contrib/puff/puff.c ]; then
    printf '  ok       zlib reference inflate (contrib/puff) present\n'
else
    missing 'third_party/zlib/contrib/puff/puff.c'
fi
# Which upstream sources the build needs is the lock file's job, so doctor asks
# it rather than keeping a second, easily stale list here.
lock=${LOCK:-}
deps_tool=${DEPS:-}
cache=${CACHE:-}
if [ -n "$lock" ] && [ ! -f "$lock" ]; then
    missing "$lock (the dependency lock file is gone)"
elif [ -n "$deps_tool" ] && [ -x "$deps_tool" ] && [ -n "$lock" ]; then
    if "$deps_tool" --lock "$lock" --check --root "$PWD" >/dev/null 2>&1; then
        printf '  ok       %s (%s dependencies)\n' \
            "$lock" "$("$deps_tool" --lock "$lock" --list | grep -c '')"
    else
        missing "$lock does not validate: $deps_tool --lock $lock --check --root ."
    fi
    # A `for` loop, not a pipeline: a piped `while` runs in a subshell, and a
    # MISSING submodule there would print without failing doctor. Ids are
    # validated by leonos-deps to contain no whitespace.
    for dependency in $("$deps_tool" --lock "$lock" --list 2>/dev/null); do
        directory=$("$deps_tool" --lock "$lock" --id "$dependency" --print directory 2>/dev/null) || continue
        kind=$("$deps_tool" --lock "$lock" --id "$dependency" --print kind 2>/dev/null) || continue
        [ "$kind" = submodule ] || continue
        if [ -n "$directory" ] && [ -n "$(ls -A "$directory" 2>/dev/null)" ]; then
            printf '  ok       %s\n' "$directory"
        else
            printf '  MISSING  %s (git submodule update --init)\n' "$directory"
        fi
    done
    if [ -n "$cache" ]; then
        wanted=$("$deps_tool" --lock "$lock" --fetch-list 2>/dev/null | grep -c '')
        have=0
        [ -d "$cache" ] && have=$(cd "$cache" && find . -type f -name '.*.partial.*' -prune -o -type f -print | wc -l)
        printf '  %-8s %s of %s locked downloads in %s (make fetch)\n' \
            "$([ "$have" -ge "$wanted" ] && echo ok || echo note)" "$have" "$wanted" "$cache"
    fi
elif [ -n "$lock" ]; then
    printf '  note     lock file not validated: %s is not built yet (make tools)\n' \
        "$deps_tool"
fi

printf '\n'
if [ "$failures" -ne 0 ]; then
    printf 'doctor: %d missing requirement(s)\n' "$failures" >&2
    exit 1
fi
printf 'doctor: all checked requirements present\n'
