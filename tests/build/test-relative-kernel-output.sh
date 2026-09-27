#!/bin/sh
# Relative output paths must keep the parent and ntclks sub-build in one tree.
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root"
ntclks=${NTCLKS_DIR:-$repo_root/kernel/ntclks}

mkdir -p out
work=$(mktemp -d "$repo_root/out/test-relative-kernel-output.XXXXXX")
relative_o=${work#"$repo_root"/}
trap 'rm -rf -- "$work" "$ntclks/$relative_o"' EXIT HUP INT TERM

for kernel_o in default override; do
    if [ "$kernel_o" = override ]; then
        ntclks_o=$relative_o/kernel-custom
        if ! make -s O="$relative_o" NTCLKS_O="$ntclks_o" headers_install; then
            printf 'FAIL - relative NTCLKS_O export failed\n' >&2
            exit 1
        fi
    else
        ntclks_o=$relative_o/ntclks
        if ! make -s O="$relative_o" headers_install; then
            printf 'FAIL - relative O export failed\n' >&2
            exit 1
        fi
    fi

    test -f "$repo_root/$ntclks_o/kernel-export/include/leonos/audio_abi.h"
    test -f "$work/kernel-export/include/leonos/audio_abi.h"
done

printf 'ok   - relative output paths publish ntclks UAPI headers\n'
