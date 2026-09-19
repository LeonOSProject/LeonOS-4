#!/bin/sh
set -eu

src=${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow \
  -Wstrict-prototypes -Wmissing-prototypes -I"$src" \
  "$src/tools/host/apk/leonos-apk-own.c" \
  "$src/tools/host/manifest/json.c" "$src/tools/host/common/buffer.c" \
  "$src/tools/host/common/io.c" -o "$tmp/leonos-apk-own"

mkdir -p "$tmp/root/usr/bin" "$tmp/root/usr/lib/leonos/apps/demo"
printf elf >"$tmp/root/usr/bin/tool"
chmod 755 "$tmp/root/usr/bin/tool"
ln -s tool "$tmp/root/usr/bin/tool-link"
printf app >"$tmp/root/usr/lib/leonos/apps/demo/demo.elf"
cat >"$tmp/policy.json" <<'EOF'
{"version":1,"apk_registration":"not-installed","groups":{
 "tools":{"destination":"leonos","components":[],"paths":["usr/bin/tool"]},
 "apps":{"destination":"leonos","components":["demo"]},
 "leonos-base":{"destination":"leonos","components":[]}
}}
EOF

"$tmp/leonos-apk-own" --policy "$tmp/policy.json" --root "$tmp/root" --output "$tmp/list"
awk -F '\t' '$1=="apps" && $2=="file" && $3=="0644" && $4=="usr/lib/leonos/apps/demo/demo.elf" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="tools" && $2=="file" && $3=="0755" && $4=="usr/bin/tool" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="tools" && $2=="symlink" && $3=="0777" && $4=="usr/bin/tool-link" && $5=="tool" { ok=1 } END { exit !ok }' "$tmp/list"

# Ambiguous policy claims must fail before any package is built.
cat >"$tmp/bad.json" <<'EOF'
{"version":1,"apk_registration":"not-installed","groups":{
 "one":{"destination":"leonos","components":[],"paths":["usr/bin/tool"]},
 "two":{"destination":"leonos","components":[],"paths":["usr/bin/tool"]}
}}
EOF
if "$tmp/leonos-apk-own" --policy "$tmp/bad.json" --root "$tmp/root" --output "$tmp/bad-list" 2>"$tmp/error"; then
  echo 'ambiguous ownership was accepted' >&2
  exit 1
fi
grep -q 'ambiguous ownership' "$tmp/error"

# Binary detection must use bytes, not executable modes or filename suffixes.
printf '\177ELFfixture' > "$tmp/root/usr/bin/non-executable-data"
"$tmp/leonos-apk-own" --policy "$tmp/policy.json" --root "$tmp/root" --output "$tmp/list" \
  --elf-list "$tmp/elf" --installed-policy "$tmp/installed.json"
test "$(wc -l < "$tmp/elf")" = 1
grep -q 'non-executable-data' "$tmp/elf"
grep -q '"apk_registration":"installed-by-upstream-apk"' "$tmp/installed.json"
printf 'apk ownership: policy conflicts, symlink ownership and ELF inventory passed\n'
