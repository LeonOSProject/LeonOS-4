#!/bin/sh
set -eu

src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/src/third_party/rime-pinyin-simp" \
    "$tmp/src/third_party/doomgeneric" "$tmp/src/userland/apps/oschinpt" \
    "$tmp/src/tools" "$tmp/src/resources/build-art/app-icons" \
    "$tmp/out/userland" "$tmp/fake-bin" "$tmp/repository"
printf 'dictionary\n' > "$tmp/src/third_party/rime-pinyin-simp/pinyin_simp.dict.yaml"
printf 'license\n' > "$tmp/src/third_party/rime-pinyin-simp/LICENSE"
printf 'attribution\n' > "$tmp/src/third_party/rime-pinyin-simp/ATTRIBUTION.txt"
printf 'settings\n' > "$tmp/src/userland/apps/oschinpt/settings.ini"
printf 'wad\n' > "$tmp/src/third_party/doomgeneric/freedoom1.wad"
printf 'license\n' > "$tmp/src/third_party/doomgeneric/LICENSE"
printf 'copying\n' > "$tmp/src/third_party/doomgeneric/FREEDOOM-COPYING.txt"
printf 'icon\n' > "$tmp/src/resources/build-art/app-icons/helloworld.bmp"
printf 'icon\n' > "$tmp/src/resources/build-art/app-icons/doom.bmp"
printf 'index\n' > "$tmp/index"
for app in helloworld doom doomlauncher oschinpt; do
    printf '%s\n' "$app" > "$tmp/out/userland/$app.elf"
done
cat > "$tmp/build_info.h" <<'EOF'
#define LEONOS_KERNEL_VERSION "4.7.1"
EOF
printf 'key\n' > "$tmp/key"
chmod 600 "$tmp/key"

# The fixture does not need a real APK encoder. It records each requested
# output path so the package naming contract can be checked without downloads.
cat > "$tmp/fake-bin/apk" <<'EOF'
#!/bin/sh
set -eu
output=
while [ "$#" -gt 0 ]; do
    case $1 in
        --output) output=$2; shift 2 ;;
        *) shift ;;
    esac
done
[ -n "$output" ]
: > "$output"
EOF
chmod 755 "$tmp/fake-bin/apk"

sh "$src/tools/build/rpr-apps.sh" "$tmp/src" "$tmp/out" "$tmp/build_info.h" \
    "$tmp/index" "$tmp/fake-bin/apk" "$tmp/key" "$tmp/repository" 123

for app in helloworld doom oschinpt; do
    test -f "$tmp/repository/leonos-$app-4.7.1-r123.apk"
    test ! -e "$tmp/repository/leonos-$app.apk"
done
printf '%s\n' \
    leonos-helloworld-4.7.1-r123.apk \
    leonos-doom-4.7.1-r123.apk \
    leonos-oschinpt-4.7.1-r123.apk > "$tmp/expected.list"
cmp "$tmp/expected.list" "$tmp/repository/packages.list"
test "$(cat "$tmp/repository/.complete")" = 4.7.1

# The Pages assembler must reject a versionless package before publication;
# this protects every package source, including future RPR applications.
mkdir -p "$tmp/pages-repository" "$tmp/pages-apps"
printf package > "$tmp/pages-repository/leonos-base-1-r0.apk"
printf package > "$tmp/pages-apps/leonos-broken.apk"
printf leonos-broken.apk > "$tmp/pages-apps/packages.list"
printf kernel > "$tmp/kernel.sys"
printf loader > "$tmp/loader.elf"
cat > "$tmp/pages-build.h" <<'EOF'
#define LEONOS_KERNEL_VERSION "4.7.1"
EOF
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 \
    -out "$tmp/pages-key" 2>/dev/null
chmod 600 "$tmp/pages-key"
if sh "$src/tools/build/rpr-pages.sh" "$tmp/pages-repository" "$tmp/pages-apps" \
    "$tmp/kernel.sys" "$tmp/loader.elf" "$tmp/pages-build.h" \
    "$tmp/fake-bin/apk" "$tmp/pages-key" "$tmp/pages-output" \
    2>"$tmp/pages-error"; then
    echo 'versionless RPR package was accepted' >&2
    exit 1
fi
grep -q 'missing its version' "$tmp/pages-error"
test ! -e "$tmp/pages-output"

printf 'RPR package filenames and publication validation passed\n'
