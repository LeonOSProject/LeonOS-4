#!/bin/sh
# Brand identity gate for the ReliefOS / ReliefNT rename (plan task 1).
#
# Layers (each is a sub-function; later plan tasks turn them green one by one):
#   appearance   - user-visible product names (os-release, GRUB menus)
#   paths        - canonical guest paths and OpenRC service names
#   headers_libs - canonical public headers and runtime library names
#   artifacts    - build product names (images, SDK archive)
#   old_names    - every remaining old-name hit must be listed in
#                  tests/build/brand-allowlist.tsv with a valid category.
#                  Any unlisted hit is a FAIL. Categories are defined in
#                  docs/branding-compatibility.md section 3; the transitional
#                  'migration' category is reported but only rejected when
#                  BRAND_STRICT=1 (final review, plan task 11).
#
# Old-name scan scope: Git-tracked files (git grep) and tracked filenames,
# in the main repository and in the kernel submodule. Untracked user content,
# build outputs and third-party submodules are out of scope by design; the
# final review (plan task 11) covers untracked source with a separate rg pass.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
allowlist="$root/tests/build/brand-allowlist.tsv"
strict=${BRAND_STRICT:-0}
fails=0

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

ok() { printf 'ok - %s\n' "$1"; }
fail() { printf 'FAIL - %s\n' "$1"; fails=$((fails + 1)); }
todo() { printf 'todo - %s\n' "$1"; }

# Layer 1: user-visible product names.
check_appearance() {
    if grep -qx 'NAME="ReliefOS"' "$root/system/rootfs/etc/os-release"; then
        ok 'os-release NAME is ReliefOS'
    else
        fail 'os-release NAME must be NAME="ReliefOS"'
    fi
    if grep -qx 'ID=reliefos' "$root/system/rootfs/etc/os-release"; then
        ok 'os-release ID is reliefos'
    else
        fail 'os-release ID must be ID=reliefos'
    fi
    if grep -qx 'PRETTY_NAME="ReliefOS"' "$root/system/rootfs/etc/os-release"; then
        ok 'os-release PRETTY_NAME is ReliefOS without version suffix'
    else
        fail 'os-release PRETTY_NAME must be exactly PRETTY_NAME="ReliefOS"'
    fi
    if grep -qx 'VERSION_ID=4' "$root/system/rootfs/etc/os-release"; then
        ok 'os-release VERSION_ID keeps numeric semantics'
    else
        fail 'os-release VERSION_ID must stay VERSION_ID=4'
    fi
    if grep -q 'menuentry "ReliefOS"' "$root/boot/grub/grub.cfg"; then
        ok 'GRUB menu entry uses ReliefOS'
    else
        fail 'boot/grub/grub.cfg must contain menuentry "ReliefOS"'
    fi
}

# Layer 2: canonical guest paths and service names.
check_paths() {
    if [ -f "$root/include/reliefos/layout.h" ]; then
        ok 'include/reliefos/layout.h exists'
    else
        fail 'include/reliefos/layout.h is missing (canonical layout header)'
    fi
    if grep -q '"/etc/reliefos"' "$root/include/reliefos/layout.h" 2>/dev/null; then
        ok 'layout.h declares /etc/reliefos'
    else
        fail 'include/reliefos/layout.h must declare /etc/reliefos'
    fi
    if ls "$root/system/rootfs/etc/init.d/reliefos-"* >/dev/null 2>&1; then
        ok 'OpenRC services use the reliefos-* prefix'
    else
        fail 'system/rootfs/etc/init.d must contain reliefos-* services'
    fi
}

# Layer 3: canonical public headers and runtime library names.
check_headers_libs() {
    if [ -d "$root/include/reliefos" ] && ls "$root/include/reliefos/"*.h >/dev/null 2>&1; then
        ok 'include/reliefos/ holds the canonical public headers'
    else
        fail 'include/reliefos/ must hold the canonical public headers'
    fi
    if [ -d "$root/userland/runtime/include/reliefos" ]; then
        ok 'runtime include/reliefos/ exists'
    else
        fail 'userland/runtime/include/reliefos/ is missing'
    fi
    if grep -rq 'libreliefos\.so\.2' "$root/mk" "$root/tools/build" 2>/dev/null; then
        ok 'build graph produces libreliefos.so.2'
    else
        fail 'mk/ and tools/build/ must produce libreliefos.so.2'
    fi
    if grep -q '^include/uapi/reliefos/' "$root/configs/header-export.list"; then
        ok 'header export lists include/uapi/reliefos/'
    else
        fail 'configs/header-export.list must list include/uapi/reliefos/ headers'
    fi
}

# Layer 4: build product names.
check_artifacts() {
    if grep -q 'reliefos\.vmdk' "$root/mk/images.mk" 2>/dev/null; then
        ok 'images.mk produces reliefos.vmdk'
    else
        fail 'mk/images.mk must produce reliefos.vmdk'
    fi
    if grep -q 'reliefos-live\.iso' "$root/mk/images.mk" 2>/dev/null; then
        ok 'images.mk produces reliefos-live.iso'
    else
        fail 'mk/images.mk must produce reliefos-live.iso'
    fi
    if grep -q 'reliefos-installer\.iso' "$root/mk/images.mk" 2>/dev/null; then
        ok 'images.mk produces reliefos-installer.iso'
    else
        fail 'mk/images.mk must produce reliefos-installer.iso'
    fi
    if grep -rq 'reliefos-musl-sdk' "$root/mk" "$root/tools/build" 2>/dev/null; then
        ok 'SDK archive is named reliefos-musl-sdk'
    else
        fail 'SDK archive must be named reliefos-musl-sdk'
    fi
}

# Allowlist lookup: scope + path must appear exactly once as columns 1 and 3.
hit_allowed() {
    awk -F'\t' -v s="$1" -v p="$2" \
        '!/^#/ && $1==s && $3==p {found=1} END {exit found?0:1}' "$allowlist"
}

# Audit one repository: tracked non-binary text hits and tracked filenames.
audit_repo() {
    hit_scope=$1
    hit_dir=$2
    hits="$work/hits.$hit_scope"
    [ -e "$hit_dir/.git" ] || return 0
    : > "$hits"
    git -C "$hit_dir" grep -I -i -l -E 'leonos|ntclks' >> "$hits" 2>/dev/null || true
    git -C "$hit_dir" ls-files | grep -Ei 'leonos|ntclks' >> "$hits" || true
    sort -u "$hits" | while IFS= read -r hit_path; do
        [ -n "$hit_path" ] || continue
        if ! hit_allowed "$hit_scope" "$hit_path"; then
            printf 'FAIL - non-allowlisted old name in %s:%s (rename it or add a categorised entry to brand-allowlist.tsv)\n' \
                "$hit_scope" "$hit_path"
        fi
    done > "$work/audit.$hit_scope"
    # The scan loop above must not hide failures behind the pipe: replay the
    # report into the real counters now that we are back in the main shell.
    if [ -s "$work/audit.$hit_scope" ]; then
        cat "$work/audit.$hit_scope"
        fails=$((fails + $(grep -c 'FAIL - ' "$work/audit.$hit_scope" || true)))
    else
        ok "old-name audit ($hit_scope): every hit is allowlisted"
    fi
    # Files with hits must be listed exactly once; duplicates are a data error.
    dup=$(cut -f1,3 "$allowlist" 2>/dev/null | grep -v '^#' | sort | uniq -d || true)
    if [ -n "$dup" ]; then
        fail "allowlist has duplicate entries: $dup"
    fi
    return 0
}

check_appearance
check_paths
check_headers_libs
check_artifacts

if [ -f "$allowlist" ]; then
    # Validate category vocabulary up front.
    while IFS='	' read -r a_scope a_cat a_path a_reason; do
        case $a_scope in ''|'#'*) continue ;; esac
        case $a_cat in
            compatibility|history|external-url|attribution|migration) ;;
            *) fail "allowlist invalid category '$a_cat' for $a_path" ;;
        esac
    done < "$allowlist"
    audit_repo main "$root"
    if [ -d "$root/kernel/reliefnt" ]; then
        audit_repo kernel "$root/kernel/reliefnt"
    elif [ -d "$root/kernel/ntclks" ]; then
        audit_repo kernel "$root/kernel/ntclks"
    fi
else
    fail "missing $allowlist"
fi

# Report allowlist entries still in the transitional 'migration' category.
migration_left=$(awk -F'\t' '!/^#/ && $2=="migration" {n++} END {print n+0}' "$allowlist")
if [ "$migration_left" -gt 0 ]; then
    if [ "$strict" = 1 ]; then
        fail "BRAND_STRICT: $migration_left allowlist entries still in 'migration' category"
    else
        todo "$migration_left allowlist entries still in 'migration' category (final review must clear)"
    fi
fi

if [ "$fails" -ne 0 ]; then
    printf 'not ok - brand identity: %d assertion(s) failed\n' "$fails"
    exit 1
fi
printf 'brand identity: all gates passed\n'
