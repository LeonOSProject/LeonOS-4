#!/bin/sh
# Loud placeholder for build phases that have not been migrated yet.
#
# A stub that exits 0 would turn a half-finished migration into a green build,
# which is exactly what the plan forbids (section 8 and section 14.7).
set -u

printf '%s: not migrated yet; it belongs to %s.\n' "${1:-target}" "${2:-a later phase}" >&2
printf 'See docs/build/migration-inventory.md for the old-to-new mapping.\n' >&2
exit 2
