#!/bin/sh
# UEFI live/installer ISO. xorriso and dosfstools own the on-disk formats.
set -eu
[ "$#" = 8 ] || { echo 'usage: iso SRC MODULES ESP ROOT CONFIG OUTPUT EPOCH VOLUME' >&2; exit 2; }
src=$1 modules=$2 esp=$3 root=$4 config=$5 output=$6 epoch=$7 volume=$8
export SOURCE_DATE_EPOCH=$epoch TZ=UTC LC_ALL=C
mkdir -p "$(dirname "$output")"
work=$(mktemp -d "$output.work.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cp -a "$esp" "$work/tree"
mkdir -p "$work/tree/install" "$work/tree/boot" "$work/efi/EFI/BOOT"
grub-mkstandalone -d "$modules" -O x86_64-efi -o "$work/efi/EFI/BOOT/BOOTX64.EFI" \
 --modules='part_gpt fat iso9660 multiboot2 normal search search_fs_file configfile echo serial terminal video video_bochs video_cirrus efi_gop efi_uga all_video font gfxterm gfxmenu' \
 "boot/grub/grub.cfg=$src/boot/grub/installer_embedded.cfg"
cp "$work/efi/EFI/BOOT/BOOTX64.EFI" "$work/tree/EFI/BOOT/BOOTX64.EFI"
cp "$root" "$work/tree/install/root.fat"
cp "$config" "$work/tree/grub/grub.cfg"
printf 'LeonOS installer ISO volume\n' > "$work/tree/leonos-installer-iso.marker"
find "$work/efi" "$work/tree" -exec touch -h -d "@$epoch" {} +
size=$(( ($(stat -c %s "$work/efi/EFI/BOOT/BOOTX64.EFI") + 1048575) / 1048576 + 8 ))
[ "$size" -ge 16 ] || size=16
truncate -s "${size}M" "$work/tree/boot/efiboot.img"
mkfs.fat --invariant -F 16 -n LEONOSINST "$work/tree/boot/efiboot.img"
mcopy -s -m -i "$work/tree/boot/efiboot.img" "$work/efi/EFI" ::/
touch -d "@$epoch" "$work/tree/boot/efiboot.img"
printf '  ISO      %s\n' "$output"
sh "$src/tools/build/run-logged.sh" --tag XORRISO "${output%.iso}.xorriso.log" \
 xorriso -as mkisofs -iso-level 3 -R -J -V "$volume" \
 -uid 0 -gid 0 --set_all_file_dates "$(date -u -d "@$epoch" +%Y%m%d%H%M%S)00" \
 --modification-date="$(date -u -d "@$epoch" +%Y%m%d%H%M%S)00" \
 -e boot/efiboot.img -no-emul-boot -o "$work/output.iso" "$work/tree"
mv "$work/output.iso" "$output"
