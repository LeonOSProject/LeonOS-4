#!/bin/sh
# Build a standalone EFI loader with upstream GRUB, then assemble the ESP tree.
set -eu
[ "$#" = 9 ] || { echo 'usage: efi-stage SRC MODULES LOADER KERNEL MIDDLE FONT DISPLAY STAGE EPOCH' >&2; exit 2; }
src=$1 modules=$2 loader=$3 kernel=$4 middle=$5 font=$6 display=$7 stage=$8 epoch=$9
mkdir -p "$(dirname "$stage")"
work=$(mktemp -d "$stage.new.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/EFI/BOOT" "$work/grub/fonts" "$work/grub/theme" "$work/leonos/config"
SOURCE_DATE_EPOCH=$epoch grub-mkstandalone -d "$modules" -O x86_64-efi -o "$work/EFI/BOOT/BOOTX64.EFI" \
 --modules='part_gpt fat iso9660 multiboot2 normal search search_fs_file configfile echo serial terminal video video_bochs video_cirrus efi_gop efi_uga all_video font gfxterm gfxmenu' \
 "boot/grub/grub.cfg=$src/boot/grub/embedded.cfg"
cp "$loader" "$work/loader.elf"
cp "$kernel" "$work/leonos/kernel.sys"
cp "$middle" "$work/leonos/middlelayer.sys"
cp "$font" "$work/grub/fonts/leonos-unicode.pf2"
cp "$display" "$work/leonos/config/display.conf"
cp "$src/boot/grub/grub.cfg" "$work/grub/grub.cfg"
cp "$src/boot/grub/theme/theme.txt" "$work/grub/theme/theme.txt"
find "$work" -exec touch -h -d "@$epoch" {} +
# The private stage is owned by this rule; a previous stage is retained until
# the complete replacement is available.
if [ -d "$stage.previous" ] && [ ! -e "$stage" ]; then mv "$stage.previous" "$stage"; fi
rm -rf "$stage.previous"
if [ -e "$stage" ]; then mv "$stage" "$stage.previous"; fi
mv "$work" "$stage"
rm -rf "$stage.previous"
