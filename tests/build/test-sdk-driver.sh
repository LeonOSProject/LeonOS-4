#!/bin/sh
# A fake compiler records argv losslessly enough to detect shell interpretation.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT
mkdir -p "$w/sdk/bin" "$w/sdk/include" "$w/sdk/lib"
${HOSTCC:-cc} -std=c11 -Wall -Wextra -Wpedantic -Werror \
    "$root/tools/host/sdk/reliefos-musl-cc.c" -o "$w/sdk/bin/reliefos-musl-cc"
cat > "$w/compiler" <<'EOF'
#!/bin/sh
printf '%s\n' "$0" >> "$COMPILER_RECORD"
if [ "$1" = -print-resource-dir ]; then printf '%s\n' "$RESOURCE"; exit "${PROBE_STATUS:-0}"; fi
printf '%s\n' "$@" > "$RECORD"
exit "${COMPILER_STATUS:-0}"
EOF
chmod +x "$w/compiler"
cp "$w/compiler" "$w/legacy-compiler"
chmod +x "$w/legacy-compiler"
export LEONOS_CC="$w/legacy-compiler" RELIEFOS_CC="$w/compiler" \
    RECORD="$w/args" COMPILER_RECORD="$w/compiler-paths" RESOURCE="$w/resource space"
"$w/sdk/bin/reliefos-musl-cc" -c 'source ; dollar$.c' -o 'out space.o'
grep -Fx "$w/compiler" "$COMPILER_RECORD"
grep -Fx 'source ; dollar$.c' "$RECORD"
grep -Fx "$RESOURCE/include" "$RECORD"
if grep -q 'crt1.o' "$RECORD"; then exit 1; fi
unset RELIEFOS_CC
"$w/sdk/bin/reliefos-musl-cc" -c 'legacy source.c' -o 'legacy out.o'
grep -Fx "$w/legacy-compiler" "$COMPILER_RECORD"
"$w/sdk/bin/reliefos-musl-cc" -static -x c 'source.c' -o out
grep -Fx "$w/sdk/lib/crt1.o" "$RECORD"
grep -Fx "$w/sdk/lib/mimalloc.o" "$RECORD"
grep -Fx -- '-lreliefos' "$RECORD"
"$w/sdk/bin/reliefos-musl-cc" --version
test "$(wc -l < "$RECORD")" -eq 2
"$w/sdk/bin/reliefos-musl-cc" source.c -o reliefos.out
grep -Fx -- '-l:libreliefos.so.2' "$RECORD"
grep -Fx -- '-Wl,-rpath,/usr/lib/reliefos:/lib:/usr/lib' "$RECORD"
RELIEFOS_SDK_ABI=leonos "$w/sdk/bin/reliefos-musl-cc" source.c -o leonos.out
grep -Fx -- '-l:libleonos.so.2' "$RECORD"
grep -Fx -- '-Wl,-rpath,/usr/lib/leonos:/lib:/usr/lib' "$RECORD"
if grep -Fx -- '-l:libreliefos.so.2' "$RECORD"; then exit 1; fi
status=0
RELIEFOS_SDK_ABI=unknown "$w/sdk/bin/reliefos-musl-cc" -c a.c || status=$?
test "$status" -eq 2
mv "$w/sdk" "$w/relocated sdk"
"$w/relocated sdk/bin/reliefos-musl-cc" source.c -o out
grep -Fx "$w/relocated sdk/lib/Scrt1.o" "$RECORD"
status=0
COMPILER_STATUS=42 "$w/relocated sdk/bin/reliefos-musl-cc" -c a.c || status=$?
test "$status" -eq 42
status=0
PROBE_STATUS=7 "$w/relocated sdk/bin/reliefos-musl-cc" -c a.c || status=$?
test "$status" -ne 0
echo 'ok - SDK argv, canonical/legacy ABI selection, relocation and failures'
