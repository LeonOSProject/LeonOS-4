#!/bin/sh
# Same-output-directory mutual exclusion (plan section 6.3, acceptance row A09).
#
# Two top-level makes that share one O must not interleave: they race on the same
# objects, the same generated headers and the same signature files. Different O
# directories are independent and may build concurrently, and a make nested
# inside a build that already owns *that* directory inherits the ownership
# instead of re-acquiring it, so recursive $(MAKE) cannot deadlock.
#
# Why not flock(1): Make cannot hold a descriptor open for the lifetime of the
# build -- every recipe line gets its own shell, and Make has no exit hook this
# build can rely on (.EXIT and .STATUS are not honoured by GNU Make 4.4). So the
# lock is a set of owner records whose *liveness* is the lock: the winner is the
# owner whose process started first, and a record is only believed while the
# process behind it is still running with the same start time. A build that is
# killed outright therefore cannot wedge an output tree forever.
#
# This needs /proc. That is the same requirement the rest of the build already
# has (the Linux toolchain, /proc-based tests); without it the tool exits 2
# rather than silently failing to exclude anything.
#
# usage: build-lock.sh acquire|release|status <owner-directory> <absolute-O>
# `acquire` prints the ownership token `<pid>.<start ticks>:<absolute-O>` on
# stdout when it wins and writes its complaint to stderr when it does not.
# Exit 0 = acquired or inherited, 1 = refused, 2 = the tool itself failed.

set -u

if [ "$#" -ne 3 ]; then
    printf 'usage: %s acquire|release|status <owner-directory> <absolute-O>\n' "$0" >&2
    exit 2
fi
action=$1
dir=$2
target_o=$3

case "$action" in
acquire|release|status) ;;
*) printf 'usage: %s acquire|release|status <owner-directory> <absolute-O>\n' "$0" >&2
    exit 2 ;;
esac

# Field 22 of /proc/<pid>/stat is the process start time in clock ticks; pairing
# it with the pid is what makes a recycled pid detectable.
proc_field() {
    [ -r "/proc/$1/stat" ] || return 1
    # The comm field is parenthesised and may contain spaces, so strip up to the
    # last ')' and count the remaining fields from there.
    sed -e 's/^.*) //' "/proc/$1/stat" |
        awk -v wanted="$2" '{ print $(wanted - 2) }'
}

# The owner is the nearest `make` in the ancestor chain, not the shell running
# this script: the record has to expire exactly when the build does.
probe=$$
holder_pid=''
while [ "$probe" != 1 ] && [ -r "/proc/$probe/stat" ]; do
    parent=$(proc_field "$probe" 4) || break
    [ -n "$parent" ] && [ "$parent" -gt 0 ] 2>/dev/null || break
    [ "$parent" = "$probe" ] && break
    probe=$parent
    if [ "$(cat "/proc/$probe/comm" 2>/dev/null)" = make ]; then
        holder_pid=$probe
        break
    fi
done
if [ -z "$holder_pid" ]; then
    printf 'build-lock: no make process found in the ancestor chain\n' >&2
    exit 2
fi
holder_start=$(proc_field "$holder_pid" 22)
case "$holder_start" in
    ''|*[!0-9]*)
        printf 'build-lock: cannot read the start time of pid %s\n' "$holder_pid" >&2
        exit 2
        ;;
esac
self=$holder_pid.$holder_start

# Inheritance: an outer make that owns this exact output directory has already
# excluded everybody else, so a nested make for the same directory is part of the
# same build. A nested make for a *different* directory must still acquire its
# own lock -- that is how a test suite running under `make test` gets real
# mutual exclusion for the output trees it creates.
if [ "$action" = acquire ] && [ -n "${LEONOS_BUILD_OWNER:-}" ]; then
    owner_id=${LEONOS_BUILD_OWNER%%:*}
    owner_dir=${LEONOS_BUILD_OWNER#*:}
    case "$owner_id" in
        [1-9]*.[0-9]*)
            if [ "$owner_dir" = "$target_o" ]; then
                printf '%s\n' "$LEONOS_BUILD_OWNER"
                exit 0
            fi
            ;;
    esac
fi

if ! mkdir -p "$dir" 2>/dev/null; then
    printf 'build-lock: cannot create the owner directory %s\n' "$dir" >&2
    exit 2
fi

owner="$dir/$self"

if [ "$action" = release ]; then
    rm -f "$owner"
    exit 0
fi

# Prune records whose process is gone, or whose pid has been recycled (its start
# time then no longer matches). This is also how a finished build releases the
# directory, since Make gives us no exit hook to run.
for record in "$dir"/*; do
    [ -e "$record" ] || continue
    name=${record##*/}
    case "$name" in
        [1-9]*.[0-9]*) ;;
        *) continue ;;
    esac
    [ "$name" = "$self" ] && continue
    [ "$(proc_field "${name%%.*}" 22)" = "${name#*.}" ] || rm -f "$record"
done

if [ "$action" = status ]; then
    printf '%s\n' "$self"
    for record in "$dir"/*; do
        [ -e "$record" ] || continue
        printf '  %s\n' "${record##*/}"
    done
    exit 0
fi

if ! printf '%s\n' "$holder_pid" >"$owner" 2>/dev/null; then
    printf 'build-lock: cannot write the owner record %s\n' "$owner" >&2
    exit 2
fi

# "Am I first?" is not enough on its own: two makes can each finish their scan
# before the other publishes. Ordering on (start time, pid) is total and both
# sides compute it from the same set, so exactly one of them wins however the
# publishes interleave.
for record in "$dir"/*; do
    [ -e "$record" ] || continue
    name=${record##*/}
    case "$name" in
        [1-9]*.[0-9]*) ;;
        *) continue ;;
    esac
    [ "$name" = "$self" ] && continue
    other_pid=${name%%.*}
    other_start=${name#*.}
    [ "$(proc_field "$other_pid" 22)" = "$other_start" ] || continue
    if [ "$other_start" -lt "$holder_start" ] ||
            { [ "$other_start" = "$holder_start" ] &&
              [ "$other_pid" -lt "$holder_pid" ]; }; then
        printf 'build-lock: refusing to build; this output directory is in use\n' >&2
        printf '  owner: pid %s, process start time %s ticks since boot\n' \
            "$other_pid" "$other_start" >&2
        printf '  owner records of this output directory: %s\n' "$dir" >&2
        printf 'Wait for that build to finish, choose a different O=, or delete the\n' >&2
        printf 'record once you are certain that process is gone.\n' >&2
        rm -f "$owner"
        exit 1
    fi
done

printf '%s:%s\n' "$self" "$target_o"
exit 0
