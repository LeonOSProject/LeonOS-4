#!/bin/sh
# Contract tests for the kernel-fetch delegation: `make fetch` in the parent
# also runs `make fetch` in the ntclks kernel checkout named by NTCLKS_DIR.
#
#   1. `make ntclks-fetch` delegates to the checkout's own fetch;
#   2. `make fetch` runs the parent fetch and the kernel fetch together;
#   3. a missing checkout warns and keeps the goal successful (fetch must keep
#      working on a fresh machine before the submodule exists);
#   4. `make -n` never reaches the sub-make (the adapter's no-output promise);
#   5. NTCLKS_DIR pointing back at this repository skips the delegation
#      instead of recursing into `make -C . fetch`.
#
# Everything is stubbed: a fake checkout Makefile records the fetch call and
# the parent fetch script is replaced with a recorder, so no network and no
# real cache is touched.
set -u
LC_ALL=C
export LC_ALL

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
cd "$repo_root" || exit 1

deps=${LEONOS_DEPS:?set by mk/tests.mk}
lock=${LEONOS_LOCK:-configs/dependencies.lock.json}

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-fetch-delegate.XXXXXX") || exit 1
failures=0
checks=0

cleanup() { [ -n "${KEEP_WORK:-}" ] || rm -rf "$work"; }
trap cleanup EXIT INT TERM

pass() { checks=$((checks + 1)); printf 'ok   - %s\n' "$1"; }
fail() {
    checks=$((checks + 1))
    failures=$((failures + 1))
    printf 'FAIL - %s\n' "$1"
    if [ "$#" -gt 1 ]; then shift; printf '       %s\n' "$@"; fi
}

# A stub kernel checkout whose fetch only records that it ran.
mkdir -p "$work/ntclks-stub"
cat >"$work/ntclks-stub/Makefile" <<EOF
.PHONY: fetch
fetch:
	@echo called >>'$work/kernel-fetch-called'
EOF

# A recorder for the parent's own fetch step.
cat >"$work/fetch-stub.sh" <<EOF
#!/bin/sh
echo called >>'$work/parent-fetch-called'
EOF

# The recursion case must not be able to hang the suite if the guard breaks.
if command -v timeout >/dev/null 2>&1; then
    guarded() { timeout 30 "$@"; }
else
    guarded() { "$@"; }
fi

printf '=== (1) make ntclks-fetch delegates to the checkout ===\n'
if make -s O="$work/out" NTCLKS_DIR="$work/ntclks-stub" ntclks-fetch \
        >"$work/delegate.log" 2>&1; then
    pass "make ntclks-fetch exits 0"
else
    fail "make ntclks-fetch exits 0" "$(head -c 400 "$work/delegate.log")"
fi
if [ -f "$work/kernel-fetch-called" ]; then
    pass "the checkout's fetch goal ran"
else
    fail "the checkout's fetch goal ran" 'no call marker was written'
fi

printf '\n=== (2) make fetch runs both fetches ===\n'
rm -f "$work/kernel-fetch-called" "$work/parent-fetch-called"
if make -s O="$work/out" LEONOS_DEPS_TOOL="$deps" LEONOS_LOCK="$lock" \
        LEONOS_FETCH_SCRIPT="$work/fetch-stub.sh" LEONOS_CACHE="$work/cache" \
        NTCLKS_DIR="$work/ntclks-stub" fetch >"$work/fetch.log" 2>&1; then
    pass "make fetch exits 0"
else
    fail "make fetch exits 0" "$(head -c 400 "$work/fetch.log")"
fi
if [ -f "$work/parent-fetch-called" ]; then
    pass "the parent fetch step ran"
else
    fail "the parent fetch step ran" 'no call marker was written'
fi
if [ -f "$work/kernel-fetch-called" ]; then
    pass "the kernel fetch step ran"
else
    fail "the kernel fetch step ran" 'no call marker was written'
fi

printf '\n=== (3) a missing checkout warns and stays successful ===\n'
rm -f "$work/kernel-fetch-called"
if make -s O="$work/out" NTCLKS_DIR="$work/nonexistent" ntclks-fetch \
        >"$work/missing.log" 2>&1; then
    pass "make ntclks-fetch exits 0 without a checkout"
else
    fail "make ntclks-fetch exits 0 without a checkout" \
        "$(head -c 400 "$work/missing.log")"
fi
if grep -q 'kernel checkout not found' "$work/missing.log"; then
    pass "the skip warns about the missing checkout"
else
    fail "the skip warns about the missing checkout" \
        "$(head -c 200 "$work/missing.log")"
fi
if [ ! -f "$work/kernel-fetch-called" ]; then
    pass "no fetch ran against a missing checkout"
else
    fail "no fetch ran against a missing checkout" 'a call marker was written'
fi

printf '\n=== (4) make -n never reaches the sub-make ===\n'
rm -f "$work/kernel-fetch-called"
if make -s -n O="$work/out" NTCLKS_DIR="$work/ntclks-stub" ntclks-fetch \
        >"$work/dryrun.log" 2>&1; then
    pass "make -n ntclks-fetch exits 0"
else
    fail "make -n ntclks-fetch exits 0" "$(head -c 400 "$work/dryrun.log")"
fi
if [ ! -f "$work/kernel-fetch-called" ]; then
    pass "make -n writes no call marker"
else
    fail "make -n writes no call marker" 'the sub-make ran under -n'
fi

printf '\n=== (5) NTCLKS_DIR pointing at this repository skips instead of recursing ===\n'
rm -f "$work/kernel-fetch-called"
if guarded make -s O="$work/out" NTCLKS_DIR="$repo_root" ntclks-fetch \
        >"$work/self.log" 2>&1; then
    pass "make ntclks-fetch NTCLKS_DIR=. exits 0 without recursing"
else
    fail "make ntclks-fetch NTCLKS_DIR=. exits 0 without recursing" \
        "$(head -c 400 "$work/self.log")"
fi
if grep -q 'skipping' "$work/self.log"; then
    pass "the self-pointing skip is announced"
else
    fail "the self-pointing skip is announced" "$(head -c 200 "$work/self.log")"
fi

printf '\n%d checks, %d failures\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
