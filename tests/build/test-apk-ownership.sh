#!/bin/sh
set -eu

src=${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

python3 - "$src/configs/apk-ownership.json" <<'PY'
import json, sys
policy = json.load(open(sys.argv[1], encoding="utf-8"))
groups = policy["groups"]
required = {"reliefos-nls", "reliefos-apps", "reliefos-fastfetch",
            "reliefos-mimalloc", "reliefos-musl-dev", "reliefos-apk-tools", "reliefos-base"}
missing = sorted(required - groups.keys())
legacy = sorted(name for name in groups if name.startswith("leonos-"))
if missing or legacy or policy.get("current_distributor") != "reliefos-build":
    raise SystemExit(f"APK ownership identity is not migrated: missing={missing}, legacy={legacy}, distributor={policy.get('current_distributor')}")
PY

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow \
  -Wstrict-prototypes -Wmissing-prototypes -I"$src" \
  "$src/tools/host/apk/reliefos-apk-own.c" \
  "$src/tools/host/manifest/json.c" "$src/tools/host/common/buffer.c" \
  "$src/tools/host/common/io.c" -o "$tmp/reliefos-apk-own"

mkdir -p "$tmp/root/usr/bin" "$tmp/root/usr/lib/leonos" "$tmp/root/usr/lib/reliefos/apps/demo"
printf elf >"$tmp/root/usr/bin/tool"
chmod 755 "$tmp/root/usr/bin/tool"
ln -s tool "$tmp/root/usr/bin/tool-link"
printf app >"$tmp/root/usr/lib/reliefos/apps/demo/demo.elf"
printf legacy-library >"$tmp/root/usr/lib/leonos/libleonos.so.2"
printf library >"$tmp/root/usr/lib/reliefos/libreliefos.so.2"
cat >"$tmp/policy.json" <<'EOF'
{"version":1,"apk_registration":"not-installed","groups":{
 "reliefos-tools":{"destination":"reliefos","components":[],"paths":["usr/bin/tool"]},
 "reliefos-apps":{"destination":"reliefos","components":["demo"],"paths":["usr/lib/leonos","usr/lib/reliefos"]},
 "reliefos-base":{"destination":"reliefos","components":[]}
}}
EOF

"$tmp/reliefos-apk-own" --policy "$tmp/policy.json" --root "$tmp/root" --output "$tmp/list"
awk -F '\t' '$1=="reliefos-apps" && $2=="file" && $3=="0644" && $4=="usr/lib/reliefos/apps/demo/demo.elf" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="reliefos-apps" && $2=="file" && $4=="usr/lib/reliefos/libreliefos.so.2" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="reliefos-apps" && $2=="file" && $4=="usr/lib/leonos/libleonos.so.2" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="reliefos-tools" && $2=="file" && $3=="0755" && $4=="usr/bin/tool" && $5=="-" { ok=1 } END { exit !ok }' "$tmp/list"
awk -F '\t' '$1=="reliefos-tools" && $2=="symlink" && $3=="0777" && $4=="usr/bin/tool-link" && $5=="tool" { ok=1 } END { exit !ok }' "$tmp/list"

# Ambiguous policy claims must fail before any package is built.
cat >"$tmp/bad.json" <<'EOF'
{"version":1,"apk_registration":"not-installed","groups":{
 "one":{"destination":"reliefos","components":[],"paths":["usr/bin/tool"]},
 "two":{"destination":"reliefos","components":[],"paths":["usr/bin/tool"]}
}}
EOF
if "$tmp/reliefos-apk-own" --policy "$tmp/bad.json" --root "$tmp/root" --output "$tmp/bad-list" 2>"$tmp/error"; then
  echo 'ambiguous ownership was accepted' >&2
  exit 1
fi
grep -q 'ambiguous ownership' "$tmp/error"

# Binary detection must use bytes, not executable modes or filename suffixes.
printf '\177ELFfixture' > "$tmp/root/usr/bin/non-executable-data"
"$tmp/reliefos-apk-own" --policy "$tmp/policy.json" --root "$tmp/root" --output "$tmp/list" \
  --elf-list "$tmp/elf" --installed-policy "$tmp/installed.json"
test "$(wc -l < "$tmp/elf")" = 1
grep -q 'non-executable-data' "$tmp/elf"
grep -q '"apk_registration":"installed-by-upstream-apk"' "$tmp/installed.json"
printf 'apk ownership: policy conflicts, symlink ownership and ELF inventory passed\n'
