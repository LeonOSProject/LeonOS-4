#!/bin/sh
# Rootless GPT/FAT/ext2 disk composition through upstream format tools.
set -eu
[ "$#" = 5 ] || { echo 'usage: disk ESP ROOT_EXT2 OUTPUT_RAW OUTPUT_VMDK EPOCH' >&2; exit 2; }
esp=$1 root=$2 raw=$3 vmdk=$4 epoch=$5
export SOURCE_DATE_EPOCH=$epoch TZ=UTC LC_ALL=C
mkdir -p "$(dirname "$raw")" "$(dirname "$vmdk")"
work=$(mktemp -d "$raw.work.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cp -a "$esp" "$work/esp"
find "$work/esp" -exec touch -h -d "@$epoch" {} +
esp_mib=$(( ($(du -sb "$work/esp" | cut -f1) + 33554431) / 33554432 * 32 + 32 ))
[ "$esp_mib" -ge 128 ] || esp_mib=128
root_sectors=$(( ($(stat -c %s "$root") + 511) / 512 ))
esp_sectors=$((esp_mib * 2048))
root_start=$((2048 + esp_sectors))
total_sectors=$((root_start + root_sectors + 2048))
minimum=${IMAGE_MIN_MIB:-0}
case $minimum in ''|*[!0-9]*) echo 'IMAGE_MIN_MIB must be an integer' >&2; exit 2;; esac
if [ "$((minimum * 2048))" -gt "$total_sectors" ]; then total_sectors=$((minimum * 2048)); fi
truncate -s "$((total_sectors * 512))" "$work/disk.raw"
sfdisk "$work/disk.raw" <<TABLE
label: gpt
label-id: B4A70D6A-278B-4A2C-986B-D79B8044C840
unit: sectors

start=2048,size=$esp_sectors,type=U,uuid=41A3EE19-BA85-47A0-9705-A5C128374021,name=LEONOS4_ESP
start=$root_start,size=$root_sectors,type=L,uuid=5C13543B-732C-4F81-8652-621124484420,name=LEONOS4_ROOT
TABLE
truncate -s "${esp_mib}M" "$work/esp.fat"
mkfs.fat --invariant -F 32 -s 2 -n LEONOS4ESP "$work/esp.fat"
for entry in "$work/esp"/*; do mcopy -s -m -i "$work/esp.fat" "$entry" ::/; done
dd if="$work/esp.fat" of="$work/disk.raw" bs=512 seek=2048 conv=notrunc status=none
dd if="$root" of="$work/disk.raw" bs=512 seek="$root_start" conv=notrunc status=none
qemu-img convert -f raw -O vmdk "$work/disk.raw" "$work/disk.vmdk"
qemu-img check -f vmdk "$work/disk.vmdk"
mv "$work/disk.raw" "$raw"
mv "$work/disk.vmdk" "$vmdk"
