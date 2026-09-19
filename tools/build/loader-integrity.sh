#!/bin/sh
# SHA256 is supplied by the host coreutils; this adapter only formats a header.
set -eu
[ "$#" = 4 ] || exit 2
kernel=$1 middle=$2 output=$3 emit=$4
k=$(sha256sum "$kernel"); k=${k%% *}
m=$(sha256sum "$middle"); m=${m%% *}
[ "${#k}" = 64 ] && [ "${#m}" = 64 ]
tmp=$(mktemp "$output.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM
{
    printf '#ifndef LEONOS_LOADER_INTEGRITY_H\n#define LEONOS_LOADER_INTEGRITY_H\n\n#define LEONOS_LOADER_INTEGRITY_SHA256_LEN 32u\n'
    for entry in "KERNEL:$k" "MIDDLELAYER:$m"; do
        name=${entry%%:*} digest=${entry#*:}
        printf 'static const unsigned char LEONOS_LOADER_%s_SHA256[32] = { ' "$name"
        printf '%s\n' "$digest" | sed 's/../0x&, /g;s/, $//'
        printf ' };\n'
    done
    printf '\n#endif\n'
} > "$tmp"
"$emit" --input "$tmp" --output "$output"
