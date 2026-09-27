#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
out=${O:-"$root/out/x86_64/release"}
library="$out/system/lib/libportablegl.so.1"

make -C "$root" O="$out" portablegl
test -f "$library"
readelf -d "$library" | grep -q 'Library soname: \[libportablegl.so.1\]'

for suffix in create destroy resize present window_id process_event make_current native_context; do
    for prefix in reliefos leonos; do
        symbol="${prefix}_pgl_${suffix}"
        nm -D --defined-only "$library" |
            awk -v symbol="$symbol" '$NF == symbol { found = 1 } END { exit !found }'
    done
done

echo 'ok - PortableGL exports canonical and legacy API symbols with its existing SONAME'
