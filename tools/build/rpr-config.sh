#!/bin/sh
# Parse the complete Kconfig value, without evaluating shell text.
set -eu
[ "$#" = 1 ] || exit 2
awk '
/^CONFIG_RPR_BASE_URL=/ {
    value=substr($0,index($0,"=")+1)
    if (value !~ /^".*"$/) exit 1
    value=substr(value,2,length(value)-2)
    sub(/\/+$/, "", value)
    if (value !~ /^https:\/\// || value ~ /[[:space:]\\"\047]/) exit 1
    found=1
}
END {
    if (!found) exit 1
    print "RPR_BASE_URL=" value
    print "RPR_PUBLIC_KEY=leonos-rpr.rsa.pub"
}' "$1" || { echo 'CONFIG_RPR_BASE_URL must be a quoted HTTPS URL without whitespace, quotes or backslashes' >&2; exit 1; }
