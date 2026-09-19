#!/bin/sh
# Reset runtime state and restore the shared rootfs layout before ownership scan.
set -eu
[ "$#" = 2 ] || exit 2
root=$1 layout=$2
for name in run tmp; do
    [ ! -L "$root/$name" ] || { echo "invalid runtime symlink: $name" >&2; exit 1; }
    rm -rf "$root/$name"
done
plan=$(mktemp "$root/.layout.XXXXXX")
trap 'rm -f "$plan"' EXIT HUP INT TERM
"$layout" > "$plan"
tab=$(printf '\t')
while IFS="$tab" read -r type source destination mode owner policy; do
    case $type in
        d)
            [ ! -L "$root$destination" ] || { echo "invalid layout symlink: $destination" >&2; exit 1; }
            mkdir -p "$root$destination"
            chmod "$mode" "$root$destination"
            ;;
        l)
            if [ -L "$root$destination" ]; then
                [ "$(readlink "$root$destination")" = "$source" ] || {
                    echo "conflicting layout link: $destination" >&2; exit 1;
                }
                continue
            elif [ -e "$root$destination" ]; then
                echo "invalid layout file: $destination" >&2; exit 1
            fi
            ln -s "$source" "$root$destination"
            ;;
        *) echo 'invalid layout plan' >&2; exit 1 ;;
    esac
done < "$plan"
