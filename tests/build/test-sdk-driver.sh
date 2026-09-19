#!/bin/sh
# A fake compiler records argv losslessly enough to detect shell interpretation.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT
mkdir -p "$w/sdk/bin" "$w/sdk/include" "$w/sdk/lib"
${HOSTCC:-cc} -std=c11 -Wall -Wextra -Wpedantic -Werror \
    "$root/tools/host/sdk/leonos-musl-cc.c" -o "$w/sdk/bin/leonos-musl-cc"
cat > "$w/compiler" <<'EOF'
#!/bin/sh
if [ "$1" = -print-resource-dir ]; then printf '%s\n' "$RESOURCE"; exit "${PROBE_STATUS:-0}"; fi
printf '%s\n' "$@" > "$RECORD"
exit "${COMPILER_STATUS:-0}"
EOF
chmod +x "$w/compiler"
export LEONOS_CC="$w/compiler" RECORD="$w/args" RESOURCE="$w/resource space"
"$w/sdk/bin/leonos-musl-cc" -c 'source ; dollar$.c' -o 'out space.o'
grep -Fx 'source ; dollar$.c' "$RECORD"
grep -Fx "$RESOURCE/include" "$RECORD"
if grep -q 'crt1.o' "$RECORD"; then exit 1; fi
"$w/sdk/bin/leonos-musl-cc" -static -x c 'source.c' -o out
grep -Fx "$w/sdk/lib/crt1.o" "$RECORD"
grep -Fx "$w/sdk/lib/mimalloc.o" "$RECORD"
"$w/sdk/bin/leonos-musl-cc" --version
test "$(wc -l < "$RECORD")" -eq 2
mv "$w/sdk" "$w/relocated sdk"
"$w/relocated sdk/bin/leonos-musl-cc" source.c -o out
grep -Fx "$w/relocated sdk/lib/Scrt1.o" "$RECORD"
status=0
COMPILER_STATUS=42 "$w/relocated sdk/bin/leonos-musl-cc" -c a.c || status=$?
test "$status" -eq 42
status=0
PROBE_STATUS=7 "$w/relocated sdk/bin/leonos-musl-cc" -c a.c || status=$?
test "$status" -ne 0
echo 'ok - SDK argv, compile/static/dynamic modes, relocation and failures'
