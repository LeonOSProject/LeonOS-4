#!/bin/sh
# Fetch and verify the downloads recorded in configs/dependencies.lock.json.
#
# This is the only regular network stage. A build never calls it: the build asks
# for the cache to already hold the bytes (--verify-only) and stops with the
# dependency id and this command when it does not, so an unreachable mirror can
# never turn a normal build into a download.
#
# Concurrency needs no lock file. Each process writes its own partial file and
# publishes it with link(), which is atomic: two racing fetches of the same
# dependency both verify, one link wins, and the loser's copy is discarded.
set -eu

die() {
    printf 'fetch: %s\n' "$*" >&2
    exit 1
}

deps=""
lock=""
cache=""
verify_only=0
selected=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --deps) deps=$2; shift 2 ;;
        --lock) lock=$2; shift 2 ;;
        --cache) cache=$2; shift 2 ;;
        --only) selected=$2; shift 2 ;;
        --verify-only) verify_only=1; shift ;;
        --help|-h)
            printf '%s\n' \
                'usage: fetch.sh --deps PATH --lock PATH --cache DIR' \
                '                [--only id[,id]] [--verify-only]' >&2
            exit 2 ;;
        *) printf 'fetch: unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

for required in deps lock cache; do
    eval "value=\${$required}"
    [ -n "$value" ] || { printf 'fetch: --%s is required\n' "$required" >&2; exit 2; }
done
[ -x "$deps" ] || die "$deps is not executable; run make tools first"

mkdir -p "$cache"
partials=""
cleanup() {
    for partial in $partials; do
        rm -f "$partial"
    done
}
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM

digest_of() {
    sha256sum "$1" | cut -d' ' -f1
}

wanted=$("$deps" --lock "$lock" --fetch-list)
[ -n "$wanted" ] || die "the lock file lists nothing to download"

TAB=$(printf '\t')
missing=""
mismatched=""
while IFS=$TAB read -r id digest url name; do
    [ -n "$id" ] || continue
    if [ -n "$selected" ]; then
        case ",$selected," in
            *",$id,"*) ;;
            *) continue ;;
        esac
    fi
    target="$cache/$name"

    if [ -f "$target" ]; then
        actual=$(digest_of "$target")
        if [ "$actual" = "$digest" ]; then
            printf '  %-8s %s (cached)\n' FETCH "$id"
            continue
        fi
        if [ "$verify_only" -eq 1 ]; then
            mismatched="$mismatched $id"
            continue
        fi
        # A cached file that disagrees with the lock is never accepted, and the
        # freshly downloaded copy replaces it below.
        printf '  %-8s %s\n' STALE "$id does not match the lock, refetching"
        rm -f "$target"
    fi

    if [ "$verify_only" -eq 1 ]; then
        missing="$missing $id"
        continue
    fi

    command -v curl >/dev/null 2>&1 || die "curl is required to fetch $id"
    partial="$cache/.$name.partial.$$"
    partials="$partials $partial"
    printf '  %-8s %s\n' FETCH "$id"
    # The standard proxy variables are inherited by curl, TLS validation stays
    # on, and https is required because the lock file only carries https URLs.
    if ! curl --fail --location --proto '=https' --tlsv1.2 \
            --retry 3 --retry-delay 1 --silent --show-error \
            --output "$partial" "$url"; then
        rm -f "$partial"
        die "fetching $id from $url failed"
    fi
    actual=$(digest_of "$partial")
    if [ "$actual" != "$digest" ]; then
        rm -f "$partial"
        # A download never rewrites the lock file: a mismatch is either a broken
        # mirror or a wrong pin, and both need a human decision.
        die "$id: downloaded $actual, the lock file pins $digest"
    fi
    if ln "$partial" "$target" 2>/dev/null; then
        rm -f "$partial"
    else
        # Someone else published first. Their copy went through the same check,
        # so re-verifying it here is enough to accept or refuse it.
        rm -f "$partial"
        actual=$(digest_of "$target")
        [ "$actual" = "$digest" ] || die "$id: a concurrent fetch left $target unverified"
    fi
done <<EOF
$wanted
EOF

if [ -n "$mismatched" ]; then
    printf 'fetch: these cached files do not match the lock file:%s\n' "$mismatched" >&2
    printf '       delete them from %s and run make fetch\n' "$cache" >&2
    exit 1
fi
if [ -n "$missing" ]; then
    printf 'fetch: these locked dependencies are not in %s:%s\n' "$cache" "$missing" >&2
    printf '       run make fetch before building\n' >&2
    exit 1
fi
