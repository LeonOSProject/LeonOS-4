#!/bin/sh
# Target arithmetic must link in both modes, including compiler-provided helpers.
set -eu
: "${SDK_ROOT:?new musl SDK required}"
: "${MUSL_LOADER:?host-compatible musl loader required}"
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT HUP INT TERM
cat > "$w/div.c" <<'C'
volatile unsigned __int128 dividend = 123, divisor = 7;
int main(void) { return dividend / divisor != 17; }
C
"$SDK_ROOT/bin/leonos-musl-cc" "$w/div.c" -o "$w/dynamic"
"$MUSL_LOADER" --library-path "$SDK_ROOT/lib" "$w/dynamic"
"$SDK_ROOT/bin/leonos-musl-cc" -static "$w/div.c" -o "$w/static"
"$w/static"
echo 'ok - SDK dynamic/static compiler-rt arithmetic (Linux host)'
