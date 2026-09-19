#!/bin/sh
# Stream a command to the terminal and a log without losing its exit status.
set -eu
tag=
if [ "${1:-}" = --tag ]; then
    [ "$#" -ge 4 ] || { echo 'usage: run-logged [--tag TAG] LOG COMMAND [ARG...]' >&2; exit 2; }
    tag=$2
    shift 2
fi
[ "$#" -ge 2 ] || { echo 'usage: run-logged [--tag TAG] LOG COMMAND [ARG...]' >&2; exit 2; }
log=$1; shift
mkdir -p "$(dirname "$log")"
work=$(mktemp -d "$log.stream.XXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'exit 130' INT
trap 'exit 143' HUP TERM
mkfifo "$work/output"
if [ -n "$tag" ]; then
    mkfifo "$work/formatted"
    awk -v tag="$tag" -f "$(dirname "$0")/format-log.awk" < "$work/formatted" &
    formatter=$!
    tee "$log" < "$work/output" > "$work/formatted" &
else
    tee "$log" < "$work/output" &
fi
reader=$!
status=0
"$@" > "$work/output" 2>&1 || status=$?
wait "$reader" || { [ "$status" != 0 ] || status=1; }
if [ -n "$tag" ]; then
    wait "$formatter" || { [ "$status" != 0 ] || status=1; }
fi
if [ -n "$tag" ] && [ "$status" != 0 ]; then
    printf '  %-8s %s exited %s; raw log: %s\n' ERROR "$tag" "$status" "$log" >&2
fi
exit "$status"
