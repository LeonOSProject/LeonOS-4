#!/bin/sh
# Verify the generated table against the upstream data, not an old build tree.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc=${HOSTCC:-cc}
"$cc" -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$root" \
    "$root/tools/host/assets/leonos-gbk.c" \
    "$root/tools/host/common/io.c" "$root/tools/host/common/buffer.c" -o "$work/gen"
"$work/gen" "$root/third_party/litehtml/src/encodings.cpp" "$work/table.h"
cat > "$work/check.c" <<'EOF'
#include <assert.h>
#include "table.h"
int main(void) {
    assert(LEONOS_GBK_POINTER_COUNT == 23940);
    unsigned count = 0;
    for (unsigned i = 0; i < LEONOS_GBK_POINTER_COUNT; ++i)
        if (leonos_gbk_to_unicode[i]) ++count;
    assert(count == LEONOS_GBK_MAPPED_COUNT);
    for (unsigned i = 0; i < count; ++i) {
        unsigned p = leonos_gbk_unicode_pointers[i];
        assert(p < LEONOS_GBK_POINTER_COUNT && leonos_gbk_to_unicode[p]);
        if (i) {
            unsigned prev = leonos_gbk_unicode_pointers[i-1];
            assert(leonos_gbk_to_unicode[prev] < leonos_gbk_to_unicode[p] ||
                (leonos_gbk_to_unicode[prev] == leonos_gbk_to_unicode[p] && prev < p));
        }
    }
    /* GBK D6 D0 is U+4E2D; CE C4 is U+6587. */
    assert(leonos_gbk_to_unicode[(0xd6 - 0x81)*190+(0xd0 - 0x41)] == 0x4e2d);
    assert(leonos_gbk_to_unicode[(0xce - 0x81)*190+(0xc4 - 0x41)] == 0x6587);
}
EOF
"$cc" -std=c11 -Wall -Werror "$work/check.c" -o "$work/check"
"$work/check"
before=$(stat -c '%y' "$work/table.h")
"$work/gen" "$root/third_party/litehtml/src/encodings.cpp" "$work/table.h"
[ "$before" = "$(stat -c '%y' "$work/table.h")" ]
cp "$work/table.h" "$work/expected"
for invalid in 'garbage' 'int gb18030_decoder::m_index[] = { 1, null };' \
    'int gb18030_decoder::m_index[] = { 99999999999999999999999 };'; do
    printf '%s\n' "$invalid" > "$work/bad.cpp"
    if "$work/gen" "$work/bad.cpp" "$work/table.h" 2>/dev/null; then
        echo 'FAIL - invalid GBK input accepted'; exit 1
    fi
    cmp "$work/table.h" "$work/expected"
done
printf 'ok - GBK mapping, reverse order, stable output, invalid-input preservation\n'
