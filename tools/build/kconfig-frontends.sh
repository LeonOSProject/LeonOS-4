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

    # The front end writes to a source-local staging name (see the run action
    # below), but users should be told about the configuration they requested.
    # Keep the upstream behavior when the LeonOS-only display variable is absent.
    confdata="$work_dir/libs/parser/confdata.c"
    display_old='	conf_message(_("configuration written to %s"), newname);'
    display_env='	env = getenv("LEONOS_KCONFIG_DISPLAY_CONFIG");'
    display_new='	conf_message(_("configuration written to %s"), env && *env ? env : newname);'
    if grep -F -q -- "$display_old" "$confdata"; then
        awk -v old="$display_old" -v first="$display_env" -v new="$display_new" \
            '{ if ($0 == old) { print first; print new; } else print }' \
            "$confdata" > "$confdata.tmp" && mv "$confdata.tmp" "$confdata" \
            || die 'cannot apply the configuration display-name fix'
    elif ! grep -F -q -- "$display_env" "$confdata" || ! grep -F -q -- "$display_new" "$confdata"; then
        die "unexpected kconfig-frontends conf_write implementation: $confdata"
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

# kconfig-frontends 3.12 cannot write an absolute KCONFIG_CONFIG that lives
# outside the Kconfig root: conf_write() reports "Error during writing of the
# configuration" and unlinks the half-written file. Stage the file inside the
# Kconfig root, which is also where relative source paths resolve, and publish
# the result to the requested output directory afterwards. This keeps any O=
# location working without leaving anything behind.
kconfig_dir=$(CDPATH= readlink -f -- "$(dirname -- "$kconfig")" 2>/dev/null || dirname -- "$kconfig")
cd "$kconfig_dir" || die "cannot enter $kconfig_dir"
stage=".leonos-kconfig-stage.$$"
cleanup_stage() { rm -f "$stage" "$stage~" "$stage.old"; }
trap cleanup_stage EXIT INT TERM
if [ -f "$config" ]; then
    cp "$config" "$stage" || die "cannot stage the existing configuration"
elif [ -n "$seed" ] && [ -f "$seed" ] && [ ! -s "$stage" ]; then
    cp "$seed" "$stage" || die 'cannot stage the default configuration'
fi
# The front end writes through a .tmpconfig.PID next to the current directory and
# renames it into place; a rename across filesystems fails, which is why an
# absolute out-of-tree KCONFIG_CONFIG reports a write error and unlinks its
# output. Staging locally and publishing with mv (copy fallback) fixes that.
# Anything it leaves behind is removed on the way out so the source tree stays clean.
mkdir -p include/config || die 'cannot create the kconfig scratch directory'
tmpconfig_before=$(ls .tmpconfig.* 2>/dev/null | sort)
KCONFIG_CONFIG=$stage
LEONOS_KCONFIG_DISPLAY_CONFIG=$config
export KCONFIG_CONFIG LEONOS_KCONFIG_DISPLAY_CONFIG

case "$mode" in
    defconfig)
        [ -n "$seed" ] && [ -f "$seed" ] || die 'defconfig needs a readable --seed'
        "$conf" --defconfig="$seed" "$kconfig" || die 'kconfig-conf --defconfig failed'
        ;;
    olddefconfig)
        "$conf" --olddefconfig "$kconfig" || die 'kconfig-conf --olddefconfig failed'
        ;;
    menu|menuconfig)
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

mkdir -p "$(dirname -- "$config")" || die 'cannot create the configuration directory'
if cmp -s "$stage" "$config"; then
    rm "$stage"
else
    mv "$stage" "$config" || die "cannot publish the configuration to $config"
fi
rm -f "$stage~" "$stage.old"
for leftover in $(ls .tmpconfig.* 2>/dev/null | sort); do
    printf '%s\n' "$tmpconfig_before" | grep -qxF -- "$leftover" || rm -f -- "$leftover"
done
if [ -d include/config ] && [ -z "$(ls -A include/config 2>/dev/null)" ]; then
    rmdir include/config 2>/dev/null || true
fi
