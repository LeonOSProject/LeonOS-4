#!/bin/sh
# Optional official application packages; all cryptography/formats use apk.
set -eu
[ "$#" = 8 ] || exit 2
src=$1 out=$2 build=$3 index=$4 apk=$5 key=$6 output=$7 epoch=$8
export SOURCE_DATE_EPOCH=$epoch
version=$(sed -n 's/^#define LEONOS_KERNEL_VERSION "\([0-9.]*-[0-9]*\)"$/\1/p' "$build")
[ -n "$version" ] || { echo 'invalid numeric build version' >&2; exit 1; }
package_version=${version%-*}-r${version##*-}
mkdir -p "$(dirname "$output")"
work=$(mktemp -d "$output.new.XXXXXX")
work=$(CDPATH= cd -- "$work" && pwd -P)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/repository"
manifest() {
    printf '[app]\nid=%s\nname=%s\nversion=%s\ncategory=%s\nexec=%s\nicon=%s\nentry=%s\nterminal=0\nsystem=0\nhidden=0\nopen_with=0\ncommands=%s\nextensions=\n' "$1" "$2" "$version" "$3" "$4" "$5" "$6" "$7" > "$root/manifest.ini"
}
for app in helloworld doom oschinpt; do
    payload=$work/$app
    root=$payload/usr/lib/leonos/apps/$app
    mkdir -p "$root" "$payload/usr/bin"
    cp "$out/userland/$app.elf" "$root/$app.elf"
    case $app in
    helloworld)
        cp "$src/resources/build-art/app-icons/helloworld.bmp" "$root/"
        manifest helloworld 'Hello World' 'Developer applications' helloworld.elf helloworld.bmp 1 helloworld
        ln -s ../lib/leonos/apps/helloworld/helloworld.elf "$payload/usr/bin/helloworld"
        ;;
    doom)
        cp "$out/userland/doomlauncher.elf" "$root/"
        cp "$src/third_party/doomgeneric/freedoom1.wad" "$src/third_party/doomgeneric/FREEDOOM-COPYING.txt" "$root/"
        cp "$src/third_party/doomgeneric/LICENSE" "$root/DOOMGENERIC-LICENSE"
        cp "$src/resources/build-art/app-icons/doom.bmp" "$root/"
        manifest doom DOOM Games doomlauncher.elf doom.bmp 1 doom,doomlauncher
        ln -s ../lib/leonos/apps/doom/doomlauncher.elf "$payload/usr/bin/doom"
        ln -s ../lib/leonos/apps/doom/doomlauncher.elf "$payload/usr/bin/doomlauncher"
        ;;
    oschinpt)
        cp "$src/third_party/rime-pinyin-simp/pinyin_simp.dict.yaml" "$src/third_party/rime-pinyin-simp/LICENSE" "$src/third_party/rime-pinyin-simp/ATTRIBUTION.txt" "$root/"
        cp "$src/userland/apps/oschinpt/settings.ini" "$root/"
        cp "$index" "$root/oscp.idx"
        manifest oschinpt 'LeonOS 4 Chinese Input' 'Input methods' oschinpt.elf '' 0 oschinpt
        printf 'input_method=1\n' >> "$root/manifest.ini"
        ln -s ../lib/leonos/apps/oschinpt/oschinpt.elf "$payload/usr/bin/oschinpt"
        ;;
    esac
    find "$payload" -exec touch -h -d "@$epoch" {} +
    set -- mkpkg --files "$payload" --output "$work/repository/leonos-$app.apk" --info "name:leonos-$app" \
      --info "version:$package_version" --info arch:x86_64 --info "origin:leonos-$app" \
      --info "description:LeonOS application $app" --info license:LicenseRef-See-Bundled-Notices \
      --info 'depends:leonos-apps leonos-musl' --sign-key "$key"
    if [ "$app" = oschinpt ]; then
        set -- "$@" --script "post-install:$src/tools/oschinpt-apk-post-install" \
          --script "post-upgrade:$src/tools/oschinpt-apk-post-install" \
          --script "post-deinstall:$src/tools/oschinpt-apk-post-deinstall"
    fi
    if unshare -Ur true >/dev/null 2>&1; then unshare -Ur "$apk" "$@"
    else fakeroot "$apk" "$@"
    fi
    printf 'leonos-%s.apk\n' "$app" >> "$work/repository/packages.list"
done
if [ -d "$output.previous" ] && [ ! -e "$output" ]; then mv "$output.previous" "$output"; fi
rm -rf "$output.previous"
if [ -e "$output" ]; then mv "$output" "$output.previous"; fi
mv "$work/repository" "$output"
rm -rf "$output.previous"
