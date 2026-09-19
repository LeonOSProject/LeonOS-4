#!/bin/sh
# Same-output-directory mutual exclusion (plan section 6.3, acceptance row A09).
#
# Two top-level makes that share one O must not interleave: they race on the same
# objects, the same generated headers and the same signature files. Different O
# directories are independent and may build concurrently, and a make nested
# inside a build that already owns *that* directory inherits the ownership
# instead of re-acquiring it, so recursive $(MAKE) cannot deadlock.
#
# A short-lived flock serializes owner-record inspection and replacement. The
# make PID plus /proc start time holds logical ownership for the whole build;
# no daemon or second scheduler is needed. The guard inode must never be removed
# by clean, including while its descriptor is open in another contender.
# usage: build-lock.sh acquire|release|status <lock-directory> <absolute-O>

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

# The lock directory is persistent, outside regenerable metadata. Reject links
# rather than allowing the guard inode or owner record to alias another tree.
[ "$(realpath -m -- "$dir")" = "$(realpath -ms -- "$dir")" ] || {
    printf 'build-lock: refusing symlink lock directory\n' >&2; exit 2;
}
mkdir -p "$dir" || exit 2
[ ! -L "$dir/guard" ] && [ ! -L "$dir/owner" ] || exit 2
exec 9>"$dir/guard" || exit 2
flock -x 9 || exit 2

owner=$(cat "$dir/owner" 2>/dev/null || :)
owner_id=${owner%%:*}
owner_pid=${owner_id%%.*}
owner_start=${owner_id#*.}
live=0
case "$owner_pid:$owner_start" in
    *[!0-9:]*|:*|*:) ;;
    *)
        if [ "$(proc_field "$owner_pid" 22)" = "$owner_start" ] &&
           [ "$(proc_field "$owner_pid" 3)" != Z ]; then live=1; fi ;;
esac

if [ "$action" = status ]; then
    [ "$live" = 0 ] || printf '%s\n' "$owner"
    exit 0
fi
if [ "$action" = release ]; then
    [ "$owner" != "$self:$target_o" ] || rm -f "$dir/owner"
    exit 0
fi

if [ "$live" = 1 ]; then
    # Only an actual ancestor can delegate ownership. An environment token is
    # not authority, even when copied from another currently running build.
    probe=$holder_pid
    if [ "$owner" = "$self:$target_o" ]; then
        printf '%s\n' "$owner"; exit 0
    fi
    if [ "${LEONOS_BUILD_OWNER:-}" = "$owner" ] &&
       [ "${owner#*:}" = "$target_o" ]; then
        while [ "$probe" -gt 1 ] 2>/dev/null; do
            probe=$(proc_field "$probe" 4) || break
            if [ "$probe" = "$owner_pid" ]; then
                printf '%s\n' "$owner"; exit 0
            fi
        done
    fi
    printf 'build-lock: refusing to build; this output directory is in use\n' >&2
    printf '  owner: pid %s, process start time %s ticks since boot\n' "$owner_pid" "$owner_start" >&2
    printf '  output directory: %s\n' "$target_o" >&2
    exit 1
fi
# Atomic replacement leaves either the old stale owner or the complete new one
# after interruption. flock releases automatically even after SIGKILL.
printf '%s:%s\n' "$self" "$target_o" > "$dir/owner.tmp.$$" || exit 2
mv -f -- "$dir/owner.tmp.$$" "$dir/owner" || exit 2
printf '%s:%s\n' "$self" "$target_o"
