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

# Resolve symlinks before deleting anything: an escaping link would take the
# removal outside the output tree.
resolved=$(realpath -s "$target" 2>/dev/null || echo "")
[ -n "$resolved" ] || refuse 'cannot resolve the output directory'
case "$resolved" in
    "/"|"" ) refuse 'resolved output directory is unsafe' ;;
esac
if [ -L "$target" ]; then
    real=$(readlink -f "$target" 2>/dev/null || echo "")
    [ -n "$real" ] || refuse 'output symlink does not resolve'
    [ "$real" = "$resolved" ] || refuse "output directory is a symlink to $real"
fi

# Explicit list rather than a glob: an unknown entry in the tree is not ours to
# delete, and this is what makes `clean` auditable.
# third-party holds upstream build directories this configuration owns (see mk/third-party.mk).
products="obj generated host include auth sysroot stage packages images logs meta third-party"
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
