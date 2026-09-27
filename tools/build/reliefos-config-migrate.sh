#!/bin/sh
# Explicitly migrate component symbols in an existing Kconfig configuration.
set -eu

usage() {
    printf 'usage: %s INPUT OUTPUT | --in-place CONFIG\n' "$0" >&2
    exit 2
}

convert() {
    input=$1
    output=$2
    [ -f "$input" ] || { printf 'config-migrate: no input file: %s\n' "$input" >&2; return 1; }
    [ "$input" != "$output" ] || {
        printf 'config-migrate: input and output must differ (use --in-place)\n' >&2
        return 1
    }
    out_dir=$(dirname -- "$output")
    mkdir -p "$out_dir"
    tmp=$(mktemp "$out_dir/.reliefos-config.XXXXXX") || return 1
    if ! awk '
        function symbol(line,    pos) {
            if (line ~ /^# CONFIG_(LEON|RELIEFOS)_COMPONENT_[A-Z0-9_]+ is not set$/) {
                pos = index(line, " is not set")
                return substr(line, 3, pos - 3)
            }
            if (line ~ /^CONFIG_(LEON|RELIEFOS)_COMPONENT_[A-Z0-9_]+=/) {
                pos = index(line, "=")
                return substr(line, 1, pos - 1)
            }
            return ""
        }
        function value(line,    pos) {
            if (line ~ /^# /) return "n"
            pos = index(line, "=")
            return substr(line, pos + 1)
        }
        function rename_old(line) {
            if (line ~ /^# CONFIG_LEON_COMPONENT_/) {
                sub(/^# CONFIG_LEON_COMPONENT_/, "# CONFIG_RELIEFOS_COMPONENT_", line)
            } else {
                sub(/^CONFIG_LEON_COMPONENT_/, "CONFIG_RELIEFOS_COMPONENT_", line)
            }
            return line
        }
        {
            lines[NR] = $0
            key = symbol($0)
            if (key == "") next
            current = value($0)
            if (key ~ /^CONFIG_LEON_COMPONENT_/) {
                old_seen[key] = 1
                if (old_value[key] != "" && old_value[key] != current) {
                    print "config-migrate: conflicting duplicate " key > "/dev/stderr"
                    bad = 1
                }
                old_value[key] = current
                new_key = key
                sub(/^CONFIG_LEON_COMPONENT_/, "CONFIG_RELIEFOS_COMPONENT_", new_key)
                old_to_new[key] = new_key
            } else {
                new_seen[key] = 1
                if (new_value[key] != "" && new_value[key] != current) {
                    print "config-migrate: conflicting duplicate " key > "/dev/stderr"
                    bad = 1
                }
                new_value[key] = current
            }
        }
        END {
            for (key in old_seen) {
                new_key = old_to_new[key]
                if (new_seen[new_key] && old_value[key] != new_value[new_key]) {
                    print "config-migrate: old and new values conflict for " new_key > "/dev/stderr"
                    bad = 1
                }
            }
            if (bad) exit 1
            for (i = 1; i <= NR; i++) {
                line = lines[i]
                key = symbol(line)
                if (key ~ /^CONFIG_LEON_COMPONENT_/) {
                    new_key = old_to_new[key]
                    if (new_seen[new_key]) continue
                    line = rename_old(line)
                }
                print line
            }
        }
    ' "$input" > "$tmp"; then
        rm -f -- "$tmp"
        return 1
    fi
    chmod --reference="$input" "$tmp"
    mv -f -- "$tmp" "$output"
}

if [ "$#" -eq 2 ] && [ "$1" != --in-place ]; then
    convert "$1" "$2"
    exit $?
fi
if [ "$#" -eq 2 ] && [ "$1" = --in-place ]; then
    config=$2
    backup=$config.leonos.bak
    [ -f "$config" ] || { printf 'config-migrate: no existing config: %s\n' "$config" >&2; exit 1; }
    [ ! -e "$backup" ] || { printf 'config-migrate: backup already exists: %s\n' "$backup" >&2; exit 1; }
    backup_tmp=$backup.tmp.$$
    cp -p -- "$config" "$backup_tmp"
    mv -- "$backup_tmp" "$backup"
    replacement=$(mktemp "$(dirname -- "$config")/.reliefos-config.XXXXXX")
    if convert "$config" "$replacement"; then
        mv -f -- "$replacement" "$config"
    else
        rm -f -- "$replacement"
        exit 1
    fi
    exit 0
fi
usage
