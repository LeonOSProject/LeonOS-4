#!/bin/sh
# Contract test for the GitHub Pages assembly: build a full tree from synthetic
# build artifacts, verify it passes verify-pages.sh, and confirm verify-pages.sh
# rejects the failure modes that must block a deployment (plan §46, §44).
#
# No cross toolchain is needed: rpr-pages.sh is exercised here only through the
# site.sh assembly of an already-built RPR subtree, and the APK index is produced
# by a stub `apk` exactly like test-rpr-packages.sh does.
set -eu

src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
fail=0
ok()   { printf 'ok - %s\n' "$1"; }
bad()  { printf 'FAIL - %s\n' "$1"; fail=1; }

build="$tmp/build_info.h"
printf '#define LEONOS_KERNEL_VERSION "4.9.1"\n' > "$build"
head -c 8192 /dev/zero > "$tmp/kernel.sys"
head -c 4096 /dev/zero > "$tmp/loader.elf"
head -c 123456 /dev/zero > "$tmp/leonos4-installer.iso"

# Stub apk: mkndx just creates the requested output, as in the RPR test.
mkdir -p "$tmp/fake-bin"
cat > "$tmp/fake-bin/apk" <<'EOF'
#!/bin/sh
set -eu
[ "$1" = mkndx ] || exit 0
out=
while [ "$#" -gt 0 ]; do [ "$1" = --output ] && out=$2; shift; done
[ -n "$out" ] && : > "$out"
EOF
chmod 755 "$tmp/fake-bin/apk"

# Real 0600 signing key so the pub-key export path is exercised.
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$tmp/key" 2>/dev/null
chmod 600 "$tmp/key"

# Two package sources so the generated list has >1 row.
mkdir -p "$tmp/repository" "$tmp/apps"
printf aaa > "$tmp/repository/leonos-musl-4.9.1-r5.apk"
printf bb  > "$tmp/apps/leonos-helloworld-4.9.1-r5.apk"

# 1. Build the RPR subtree, then assemble the full Pages tree.
sh "$src/tools/build/rpr-pages.sh" "$tmp/repository" "$tmp/apps" \
    "$tmp/kernel.sys" "$tmp/loader.elf" "$build" \
    "$tmp/fake-bin/apk" "$tmp/key" "$tmp/rpr-pages" \
    || { echo 'rpr-pages.sh failed' >&2; exit 1; }
sh "$src/tools/build/site.sh" "$tmp/rpr-pages" "$tmp/leonos4-installer.iso" \
    "$build" "$src/resources/pages/css/leonos.css" "$tmp/pages" \
    || { echo 'site.sh failed' >&2; exit 1; }

# 2. The assembled tree must pass verification.
if sh "$src/tools/build/verify-pages.sh" "$tmp/pages" >/dev/null 2>&1; then
    ok 'verify-pages.sh accepts the assembled tree'
else
    bad 'verify-pages.sh rejected a good tree'
fi

# 3. Every machine-interface file survived the move under /rpr/.
for f in rpr/apk/packages.adb rpr/apk/repository.json rpr/apk/leonos-rpr.rsa.pub \
         rpr/apk/SHA256SUMS rpr/kernel/release.txt rpr/kernel/release.json \
         rpr/kernel/kernel.sys rpr/kernel/loader.elf rpr/kernel/SHA256SUMS \
         rpr/manifest.json rpr/health.txt; do
    [ -f "$tmp/pages/$f" ] || bad "machine interface lost $f"
done
[ -f "$tmp/pages/rpr/manifest.json" ] && ok 'RPR machine interface preserved'

# 4. Release.txt version is the real 3-part version (machine contract, §37).
grep -q '^version=4.9.1$' "$tmp/pages/rpr/kernel/release.txt" \
    && ok 'release.txt version matches build' \
    || bad 'release.txt version drifted from build'

# 5. No JavaScript anywhere and the ISO hash matches its SHA256SUMS.
[ -z "$(find "$tmp/pages" -type f -name '*.js' 2>/dev/null)" ] \
    && ok 'no .js files in Pages tree' || bad 'a .js file leaked into Pages'
( cd "$tmp/pages/download" && sha256sum -c SHA256SUMS >/dev/null 2>&1 ) \
    && ok 'installer ISO matches download/SHA256SUMS' \
    || bad 'installer ISO checksum mismatch'

# 6. Negative: a broken link must fail verification.
cp -R "$tmp/pages" "$tmp/broken"
sed -i 's#href="download/index.html"#href="download/nope.html"#' "$tmp/broken/index.html"
if sh "$src/tools/build/verify-pages.sh" "$tmp/broken" >/dev/null 2>&1; then
    bad 'verify-pages.sh accepted a broken link'
else
    ok 'verify-pages.sh rejects a broken link'
fi

# 7. Negative: injected JavaScript must fail verification.
cp -R "$tmp/pages" "$tmp/js"
printf 'alert(1)\n' > "$tmp/js/tracker.js"
if sh "$src/tools/build/verify-pages.sh" "$tmp/js" >/dev/null 2>&1; then
    bad 'verify-pages.sh accepted a .js file'
else
    ok 'verify-pages.sh rejects a .js file'
fi

# 8. Negative: a missing machine file must fail verification.
cp -R "$tmp/pages" "$tmp/noindex"
rm -f "$tmp/noindex/rpr/apk/packages.adb"
if sh "$src/tools/build/verify-pages.sh" "$tmp/noindex" >/dev/null 2>&1; then
    bad 'verify-pages.sh accepted a missing packages.adb'
else
    ok 'verify-pages.sh rejects a missing packages.adb'
fi

[ "$fail" = 0 ] || exit 1
printf 'Pages assembly and verification contract passed\n'
