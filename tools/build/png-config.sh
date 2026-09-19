#!/bin/sh
# Adjust the pinned prebuilt libpng configuration, refusing an unknown layout.
set -eu
[ "$#" = 3 ] || { echo 'usage: png-config.sh INPUT OUTPUT EMIT' >&2; exit 2; }
[ "$(grep -Fc '/* end of options */' "$1")" = 1 ] || exit 1
tmp=$(mktemp "${2}.XXXXXX")
trap 'rm -f "$tmp"' EXIT
awk '/\/\* end of options \*\// {
    print "#undef PNG_FLOATING_ARITHMETIC_SUPPORTED"
    print "#undef PNG_FLOATING_POINT_SUPPORTED"
    print "#undef PNG_READ_FLOAT_SUPPORTED"
    print "#undef PNG_INCH_CONVERSIONS_SUPPORTED"
} { print }' "$1" > "$tmp"
# leonos-emit consumes stdin and leaves an identical output untouched.
"$3" --output "$2" < "$tmp"
