#!/bin/sh
# Production execution-chain contract: acceptance row A16.
#
# The migration forbids Python, Meson and Ninja anywhere in the production build,
# including in wrapper scripts the build generates itself. "We did not call them"
# is only evidence if somebody looked, so this traces execve() over the real entry
# points and fails on a hit.
#
# Full production coverage includes SDK, signed packages, all images and RPR.
# This deliberately takes longer than unit and contract tests.
set -u

LC_ALL=C
export LC_ALL

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root" || exit 1

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-execchain.XXXXXX") || exit 1
failures=0
checks=0

cleanup() {
    [ -n "${KEEP_WORK:-}" ] || rm -rf "$work"
}
trap cleanup EXIT INT TERM

pass() { checks=$((checks + 1)); printf 'ok   - %s\n' "$1"; }
fail() {
    checks=$((checks + 1))
    failures=$((failures + 1))
    printf 'FAIL - %s\n' "$1"
    if [ "$#" -gt 1 ]; then
        shift
        printf '       %s\n' "$@"
    fi
}

if ! command -v strace >/dev/null 2>&1; then
    printf 'FAIL - strace is required to prove the exec chain\n'
    printf 'test-execchain: 1 checks, 1 failures\n'
    exit 1
fi

O="$work/out"
# A hard-linked copy of the real download cache: fetch sees the bytes already
# present, verifies their digests, and never touches the network from here.
cache="$work/cache"
mkdir -p "$cache"
if ! cp -al "$repo_root/cache/downloads/." "$cache/" 2>/dev/null; then
    cp -a "$repo_root/cache/downloads/." "$cache/" 2>/dev/null ||
        fail 'the download cache can be copied for a warm fetch' 'cache missing'
fi

# An executed program: the path execve() was handed, or argv[0] when the path was
# NULL. Matched as a file name, so a -I flag that happens to mention python does
# not trip it.
forbidden_program='execve\("([^"]*/)?(python[0-9.]*|meson|ninja)",'
# A word a shell would run: catches `sh -c "python3 build.py"` where the executed
# program is /bin/sh and the interpreter only appears in argv.
forbidden_word='[\" ](python3?(\\.py)?|meson|ninja)([\" ]|$)'

# trace <label> <make arguments...>
trace() {
    label=$1
    shift
    strace -f -qq -e trace=execve -o "$work/$label.exec" \
        make -s O="$O" LEONOS_CACHE="$cache" "$@" >"$work/$label.out" 2>&1
    status=$?
    if [ -f "$work/$label.exec" ]; then
        execs=$(grep -c 'execve(' "$work/$label.exec" || true)
    else
        execs=0
    fi
    if [ "$status" -ne 0 ]; then
        fail "$label build exits zero" "status $status"
        tail -n 10 "$work/$label.out" | sed 's/^/       | /'
        return 0
    fi
    # An empty trace would make "no python" trivially true, so the suite proves it
    # really watched a compiler run.
    if [ "$execs" -lt 20 ] || ! grep -q 'cc1\|clang\|/make' "$work/$label.exec"; then
        fail "$label trace captured real work" "$execs execve records"
    else
        pass "$label trace captured real work ($execs execve records)"
    fi
    programs=$(grep -E "$forbidden_program" "$work/$label.exec" |
            sed -e 's/(.*//' -e 's/.*execve//' | sort -u | head -5)
    words=$(grep -E "$forbidden_word" "$work/$label.exec" |
            grep -vE "$forbidden_program" | sed 's/^ *[^ (]*//' | head -3)
    if [ -n "$programs" ]; then
        fail "$label execs no forbidden interpreter" "$programs"
    elif [ -n "$words" ]; then
        fail "$label execs no forbidden interpreter" \
            "mentioned in argv: $(printf '%s' "$words" | head -c 200)"
    else
        pass "$label execs no python, meson or ninja"
    fi
}

printf '=== A16: execve trace over the migrated production surface ===\n'
trace defconfig defconfig
trace tools tools
trace kernel -j4 kernel
# The sysroot runs upstream musl's own configure and make, which is exactly where
# a Meson or Python leak would appear.
trace sysroot -j4 "$O/sysroot/musl/.leonos-musl.json"
trace fetch-fetch fetch
trace verify leonos-verify-cache

printf '\n=== A16: complete production chain ===\n'
trace production -j8 all rpr-pages

printf '\n%s: %d checks, %d failures\n' 'test-execchain' "$checks" "$failures"
[ "$failures" -eq 0 ] || exit 1
exit 0
