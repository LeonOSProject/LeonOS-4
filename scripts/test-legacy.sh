#!/bin/sh
# Explicit, non-production Python regression checks. Guest-specific historical
# harnesses remain individual tools and are not silently treated as passes.
set -eu
. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/logging.sh"
src=$1
cd "$src"
for suite in test_uapi test_regular_file_io test_storage_metadata test_storage_rename test_storage_mkdir_mount test_storage_sync test_record_locks test_unix_ipc; do
    leonos_log LEGACY "$suite"
    python3 "tools/$suite.py"
done
