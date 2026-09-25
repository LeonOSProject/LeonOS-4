#!/bin/sh
# Remove regenerable build products from the selected output directory.
#
# Safety rules from the migration plan section 6.3: never `rm -rf $(O)` blindly,
# require an ownership marker, refuse the source root and the filesystem root,
# and keep the shared download cache in every mode.
set -u

target=${O:-}
src=${SRC:-}
keep=${KEEP_CONFIG:-1}

refuse() {
    printf 'clean: refusing to clean: %s\n' "$1" >&2
    exit 1
}

case "$target" in
    /*) ;;
    *)  case "$target" in
            "") refuse 'O is empty' ;;
            *)  target="$PWD/$target" ;;
        esac ;;
esac

[ "$target" = "/" ] && refuse 'O is the filesystem root'
[ -n "$src" ] && [ "$target" = "$src" ] && refuse "O is the source root ($src)"

# Resolve all components before reading the marker or removing products.
resolved=$(realpath -ms -- "$target" 2>/dev/null) || refuse 'cannot resolve O'
physical=$(realpath -m -- "$target" 2>/dev/null) || refuse 'cannot resolve O'
[ "$physical" = "$resolved" ] || refuse 'O contains a symlink component'
[ "$resolved" != / ] || refuse 'resolved O is the filesystem root'
if [ -n "$src" ]; then
    source_root=$(realpath -m -- "$src") || refuse 'cannot resolve source root'
    [ "$resolved" != "$source_root" ] || refuse 'resolved O is the source root'
fi
target=$resolved
[ ! -L "$target/.leonos-out" ] || refuse 'ownership marker is a symlink'
marker="$target/.leonos-out"
if [ ! -f "$marker" ]; then
    refuse "no ownership marker at $marker (not a build output directory created by this Makefile)"
fi
if ! grep -q '^leonos4-build-out version=1 ' "$marker"; then
    refuse "ownership marker in $target is not a LeonOS build output marker"
fi
marker_root=$(sed -n 's/^leonos4-build-out version=1 root=//p' "$marker" | head -n1)
if [ -n "$src" ] && [ -n "$marker_root" ] && [ "$marker_root" != "$src" ]; then
    refuse "output directory belongs to a different source root ($marker_root)"
fi

# Explicit list rather than a glob: an unknown entry in the tree is not ours to
# delete, and this is what makes `clean` auditable.
# third-party holds upstream build directories this configuration owns (see mk/third-party.mk).
products="userland userland-installer userland-installer-policy upstream rootfs resources rpr-apps rpr-pages obj generated host include auth pam system musl installer sdk sysroot stage packages images logs meta third-party kernel-export kernel-install"
if [ "$keep" = 0 ]; then
    products="$products config"
fi

printf 'clean: %s (keep config=%s)\n' "$resolved" "$keep"
for entry in $products; do
    if [ -e "$resolved/$entry" ] || [ -L "$resolved/$entry" ]; then
        printf '  remove %s\n' "$entry"
        rm -rf -- "$resolved/$entry" || exit 1
    fi
done
if [ "$keep" = 0 ] && [ -e "$resolved/.leonos-out" ]; then
    printf '  remove .leonos-out\n'
    rm -f -- "$resolved/.leonos-out"
fi
printf 'clean: the shared download cache under cache/downloads was not touched\n'
