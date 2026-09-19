#!/bin/sh
# Deterministic lock interleavings, using FIFOs instead of build timing.
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
pids=''
trap 'kill $pids 2>/dev/null || :; rm -rf "$w"' EXIT
export repo w
cat > "$w/fixture.mk" <<'MAKE'
.PHONY: delayed hold attempt
 delayed:
	@touch $(w)/older-ready; read x < $(w)/older-go; $(MAKE) -s -f $(w)/fixture.mk attempt
 hold:
	@sh $(repo)/scripts/build-lock.sh acquire $(w)/out/.build-lock $(w)/out > $(w)/token; touch $(w)/held; read x < $(w)/finish
 attempt:
	@sh $(repo)/scripts/build-lock.sh acquire $(w)/out/.build-lock $(w)/out
MAKE
# The older make itself must acquire, not a later child make.
sed -i 's@$(MAKE) -s -f $(w)/fixture.mk attempt@sh $(repo)/scripts/build-lock.sh acquire $(w)/out/.build-lock $(w)/out@' "$w/fixture.mk"
mkfifo "$w/older-go" "$w/finish"
wait_file() { n=0; while [ ! -e "$1" ]; do n=$((n+1)); [ "$n" -lt 500 ] || exit 2; sleep .01; done; }
make -s -f "$w/fixture.mk" delayed > "$w/older.log" 2>&1 & older=$!; pids="$pids $older"
wait_file "$w/older-ready"
make -s -f "$w/fixture.mk" hold > "$w/newer.log" 2>&1 & newer=$!; pids="$pids $newer"
wait_file "$w/held"
printf 'go\n' > "$w/older-go"
fail=0
if wait "$older"; then echo 'FAIL: delayed older make bypassed active owner'; fail=1; else echo 'ok: delayed older make refused'; fi
if LEONOS_BUILD_OWNER="$(cat "$w/token")" make -s -f "$w/fixture.mk" attempt > "$w/forged.log" 2>&1; then echo 'FAIL: unrelated make inherited owner'; fail=1; else echo 'ok: unrelated owner token refused'; fi
# Cleaning regenerable metadata must not discard the active owner's guard.
printf 'leonos4-build-out version=1 root=%s\n' "$repo" > "$w/out/.leonos-out"
mkdir -p "$w/out/meta"
O="$w/out" SRC="$repo" sh "$repo/scripts/clean.sh" >/dev/null
if make -s -f "$w/fixture.mk" attempt > "$w/clean-contender.log" 2>&1; then echo 'FAIL: clean dropped active lock'; fail=1; else echo 'ok: clean preserves active lock'; fi
printf 'done\n' > "$w/finish"
wait "$newer"
make -s -f "$w/fixture.mk" attempt >/dev/null
echo 'ok: stale owner recovered'
mkdir -p "$w/real/output/obj"
printf 'leonos4-build-out version=1 root=%s\n' "$repo" > "$w/real/output/.leonos-out"
touch "$w/real/output/obj/keep"
ln -s "$w/real" "$w/alias"
if O="$w/alias/output" SRC="$repo" sh "$repo/scripts/clean.sh" >/dev/null 2>&1; then echo 'FAIL: clean accepted symlink parent'; fail=1; else echo 'ok: clean rejected symlink parent'; fi
[ -f "$w/real/output/obj/keep" ] || fail=1
exit "$fail"
