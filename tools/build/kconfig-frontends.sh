#!/bin/sh
# Build and drive the repository-pinned C Kconfig front end.
#
# The pinned kconfig-frontends source is a GNU Make project, so its own build
# inherits this build's jobserver through MAKEFLAGS instead of being handed a
# hard-coded -j (plan section 6.1). The submodule is never modified: the gperf
# compatibility fix is applied to a disposable work copy.
set -u

die() { printf 'kconfig-frontends: %s\n' "$1" >&2; exit 1; }

action=${1:-}
[ -n "$action" ] || die 'expected "build" or "run"'
shift

source_dir=''
work_dir=''
prefix=''
conf=''
mconf=''
kconfig=''
config=''
seed=''
mode=''

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source) source_dir=$2; shift 2 ;;
        --work)   work_dir=$2; shift 2 ;;
        --prefix) prefix=$2; shift 2 ;;
        --conf)   conf=$2; shift 2 ;;
        --mconf)  mconf=$2; shift 2 ;;
        --kconfig) kconfig=$2; shift 2 ;;
        --config) config=$2; shift 2 ;;
        --seed)   seed=$2; shift 2 ;;
        --mode)   mode=$2; shift 2 ;;
        *) die "unknown option $1" ;;
    esac
done

if [ "$action" = build ]; then
    [ -n "$source_dir" ] && [ -n "$work_dir" ] && [ -n "$prefix" ] || die 'build needs --source --work --prefix'
    for required in bootstrap configure.ac frontends/mconf/mconf.c; do
        [ -f "$source_dir/$required" ] || die "$source_dir is not initialised; run 'git submodule update --init --recursive'"
    done
    if [ -x "$prefix/bin/kconfig-conf" ] && [ -x "$prefix/bin/kconfig-mconf" ]; then
        exit 0
    fi
    rm -rf "$work_dir"
    mkdir -p "$work_dir" "$prefix" || die 'cannot create build directories'
    ( cd "$source_dir" && tar -cf - --exclude=.git . ) | ( cd "$work_dir" && tar -xf - ) \
        || die 'cannot copy the front end source'

    # Pinned gperf declares the generated lookup with an unsigned int length
    # while modern gperf emits size_t; the declaration and definition must agree.
    gperf_input="$work_dir/libs/parser/hconf.gperf"
    old='static const struct kconf_id *kconf_id_lookup(register const char *str, register unsigned int len);'
    new='static const struct kconf_id *kconf_id_lookup(register const char *str, register size_t len);'
    if grep -F -q -- "$old" "$gperf_input"; then
        perl -e '' >/dev/null 2>&1 || true
        awk -v old="$old" -v new="$new" '{ if ($0 == old) print new; else print }' \
            "$gperf_input" > "$gperf_input.tmp" && mv "$gperf_input.tmp" "$gperf_input" \
            || die 'cannot apply the gperf compatibility fix'
    elif ! grep -F -q -- "$new" "$gperf_input"; then
        die "unexpected kconfig-frontends gperf input: $gperf_input"
    fi

    LC_ALL=C
    export LC_ALL
    ( cd "$work_dir" && ./bootstrap && \
        ./configure --prefix="$prefix" --enable-frontends=conf,mconf --disable-utils \
            --disable-L10n --disable-shared --enable-static --disable-werror && \
        make && make install ) >"$prefix/build.log" 2>&1 \
        || die "front end build failed; see $prefix/build.log"
    [ -x "$prefix/bin/kconfig-conf" ] || die "no kconfig-conf in $prefix"
    exit 0
fi

[ -n "$conf" ] && [ -n "$kconfig" ] && [ -n "$config" ] || die 'run needs --conf --kconfig --config'

KCONFIG_CONFIG=$config
export KCONFIG_CONFIG

case "$mode" in
    defconfig)
        [ -n "$seed" ] && [ -f "$seed" ] || die 'defconfig needs a readable --seed'
        "$conf" --defconfig="$seed" "$kconfig" || die 'kconfig-conf --defconfig failed'
        ;;
    olddefconfig)
        [ -f "$config" ] || { [ -n "$seed" ] && cp "$seed" "$config"; }
        "$conf" --olddefconfig "$kconfig" || die 'kconfig-conf --olddefconfig failed'
        ;;
    menu)
        [ -f "$config" ] || { [ -n "$seed" ] && cp "$seed" "$config"; }
        [ -x "$mconf" ] || die 'kconfig-mconf was not built'
        "$mconf" "$kconfig" || die 'menuconfig cancelled or failed'
        ;;
    '')
        # First build with no configuration initialises from the committed
        # profile, but an existing .config is never silently rewritten.
        if [ -f "$config" ]; then
            exit 0
        fi
        [ -n "$seed" ] && [ -f "$seed" ] || die 'no .config and no seed to initialise from'
        "$conf" --defconfig="$seed" "$kconfig" || die 'kconfig-conf --defconfig failed'
        ;;
    *)
        die "unknown configuration mode '$mode'"
        ;;
esac
