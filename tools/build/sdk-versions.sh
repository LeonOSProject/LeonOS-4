#!/bin/sh
# Describe the same locked inputs used by the build, not hard-coded SDK versions.
set -eu
[ "$#" = 4 ] || exit 2
deps=$1 lock=$2 metadata=$3 output=$4
mkdir -p "$output"
for spec in musl:MUSL zlib:ZLIB libpng:LIBPNG file:LIBMAGIC lua:LUA sqlite:SQLITE portablegl:PORTABLEGL stardustui:STARDUSTUI; do
    id=${spec%:*} name=${spec#*:}
    case $id in
        musl|zlib|libpng) ;;
        *) awk -F '\t' -v id="$id" '$1==id && $6==1 {found=1} END{exit !found}' "$metadata" || continue ;;
    esac
    # Every record must refer to an existing, validated dependency.
    directory=$("$deps" --lock "$lock" --id "$id" --print directory)
    {
        printf 'Component: %s\nSource directory: %s\n' "$id" "$directory"
        for field in version commit url sha256; do
            if value=$("$deps" --lock "$lock" --id "$id" --print "$field" 2>/dev/null); then
                printf '%s: %s\n' "$field" "$value"
            fi
        done
    } > "$output/$name-VERSION.txt"
done
cp "$lock" "$output/dependencies.lock.json"
