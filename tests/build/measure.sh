#!/bin/sh
# Wall-clock and peak-memory measurement for build timings.
#
# GNU time is not installed on every supported host, so the metric is captured
# by sampling the target's own process group.  Peak value is the sum of RSS over
# all live members of the group; shared pages are counted once per process, so
# the number is comparable between runs but is not exact unique-set memory.
#
# Usage: measure.sh <label> <stdout-log> <result-file> -- command [args...]
set -u

if [ "$#" -lt 5 ]; then
    printf 'usage: %s <label> <log> <result> -- command [args...]\n' "$0" >&2
    exit 2
fi

label=$1
log=$2
result=$3
shift 3

if [ "$1" != "--" ]; then
    printf '%s: expected -- before command\n' "$0" >&2
    exit 2
fi
shift

if [ "$#" -eq 0 ]; then
    printf '%s: empty command\n' "$0" >&2
    exit 2
fi

if [ -e "$result" ]; then
    rm -f "$result" || exit 1
fi

start_ns=$(date +%s%N)

setsid "$@" >"$log" 2>&1 &
target=$!

peak_kb=0
while :; do
    live_kb=$(ps -eo rss=,pgid=,stat= | awk -v g="$target" \
        '$2 == g && $3 !~ /^Z/ { total += $1 } END { print total + 0 }')
    if [ "$live_kb" -eq 0 ]; then
        break
    fi
    if [ "$live_kb" -gt "$peak_kb" ]; then
        peak_kb=$live_kb
    fi
    sleep 0.2
done

wait "$target"
rc=$?
end_ns=$(date +%s%N)

wall_s=$(( (end_ns - start_ns) / 1000000 ))

{
    printf 'label\t%s\n' "$label"
    printf 'command\t%s\n' "$*"
    printf 'exit_code\t%s\n' "$rc"
    printf 'wall_ms\t%s\n' "$wall_s"
    printf 'peak_group_rss_kb\t%s\n' "$peak_kb"
    printf 'log\t%s\n' "$log"
} >"$result"

cat "$result"
exit "$rc"
