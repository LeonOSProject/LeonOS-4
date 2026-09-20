#!/bin/sh
# SHA256 is supplied by the host coreutils; this adapter only formats a header.
set -eu
[ "$#" = 3 ] || exit 2
kernel=$1 output=$2 emit=$3
k=$(sha256sum "$kernel"); k=${k%% *}
[ "${#k}" = 64 ]
tmp=$(mktemp "$output.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM
{
    printf '#ifndef LEONOS_LOADER_INTEGRITY_H\n#define LEONOS_LOADER_INTEGRITY_H\n\n#define LEONOS_LOADER_INTEGRITY_SHA256_LEN 32u\n'
    printf 'static const unsigned char LEONOS_LOADER_KERNEL_SHA256[32] = { '
    printf '%s\n' "$k" | sed 's/../0x&, /g;s/, $//'
    printf ' };\n'
    printf '\n#endif\n'
} > "$tmp"
"$emit" --input "$tmp" --output "$output"
