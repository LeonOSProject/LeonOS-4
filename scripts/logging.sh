#!/bin/sh
# Shared user-facing stage logging for LeonOS shell scripts.
# Keep this format in sync with mk/logging.mk and tools/build/format-log.awk.

leonos_log() {
    [ "$#" -eq 2 ] || return 2
    printf '  %-8s %s\n' "$1" "$2"
}
