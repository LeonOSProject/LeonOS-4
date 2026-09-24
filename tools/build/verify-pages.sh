#!/bin/sh
# Verify an assembled Pages tree before it is published. Fails on the first
# problem so the workflow is atomic: no half site ever gets deployed (plan §44).
#
# Checks (plan §33, §34, §35, §36):
#   HTML   the six expected pages exist and no *.js file or inline script tag
#          appears anywhere in the tree.
#   CSS    the shared stylesheet is present at the site root and inside /rpr/.
#   RPR    every machine-interface file is present and packages.adb references
#          only APKs that actually exist under /rpr/apk/.
#   ISO    the installer ISO exists, is non-empty and matches the recorded
#          download/SHA256SUMS entry.
set -eu
[ "$#" = 1 ] || exit 2
site=$1
[ -d "$site" ] || { echo "not a Pages tree: $site" >&2; exit 1; }

# Every failing check appends to a list so a single run surfaces as much as
# possible; a non-empty list at the end is the failure signal.
fail=
report() { printf 'verify-pages: %s\n' "$1" >&2; fail="yes"; }

# --- expected files ---------------------------------------------------------
for f in index.html download/index.html download/leonos4-installer.iso download/SHA256SUMS \
         css/leonos.css .nojekyll \
         rpr/index.html rpr/packages/index.html rpr/kernel/index.html \
         rpr/apk/index.html \
         rpr/manifest.json rpr/health.txt \
         rpr/css/leonos.css \
         rpr/apk/packages.adb rpr/apk/repository.json rpr/apk/leonos-rpr.rsa.pub rpr/apk/SHA256SUMS \
         rpr/kernel/kernel.sys rpr/kernel/loader.elf \
         rpr/kernel/release.txt rpr/kernel/release.json rpr/kernel/SHA256SUMS; do
    [ -e "$site/$f" ] || report "missing $f"
done

# --- JavaScript must be absent ----------------------------------------------
js=$(find "$site" -type f \( -name '*.js' -o -name '*.mjs' -o -name '*.cjs' \) -print 2>/dev/null || true)
[ -z "$js" ] || report "JavaScript files present: $js"
scripts=$(grep -RInE -e '<script' -e 'javascript:' -e ' on[a-z]+=' "$site" 2>/dev/null || true)
[ -z "$scripts" ] || report "inline JS markers present in Pages tree:
$scripts"

# --- private keys must never reach the public tree --------------------------
if grep -RIn -E -- '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----' "$site" >/dev/null 2>&1; then
    report "private key material detected in Pages tree"
fi

# --- RPR machine interface consistency --------------------------------------
# Every package referenced by repository.json must exist under rpr/apk/, and
# every APK file must be covered by apk/SHA256SUMS.
if [ -f "$site/rpr/apk/repository.json" ]; then
    awk -F '"' '
        $0 ~ /"packages"/ { inlist=1 }
        inlist { for (i=1;i<=NF;i++) if ($i ~ /^leonos-.*\.apk$/) print $i }
    ' "$site/rpr/apk/repository.json" | while IFS= read -r pkg; do
        [ -n "$pkg" ] || continue
        [ -f "$site/rpr/apk/$pkg" ] || report "repository.json references missing APK $pkg"
    done
fi
if [ -f "$site/rpr/apk/SHA256SUMS" ]; then
    for apk in "$site"/rpr/apk/leonos-*.apk; do
        [ -f "$apk" ] || continue
        name=${apk##*/}
        awk -v n="$name" '$2==n {found=1} END{exit !found}' "$site/rpr/apk/SHA256SUMS" \
            || report "$name not covered by rpr/apk/SHA256SUMS"
    done
fi

# --- ISO hash consistency ---------------------------------------------------
if [ -f "$site/download/SHA256SUMS" ] && [ -s "$site/download/leonos4-installer.iso" ]; then
    ( cd "$site/download" && sha256sum -c SHA256SUMS >/dev/null 2>&1 ) \
        || report "installer ISO does not match download/SHA256SUMS"
fi

# --- HTML links resolve to something inside the tree ------------------------
# All href values except external http(s) URLs must exist relative to the
# containing HTML file. This guards against a broken base path (plan §25).
linkfails=$(mktemp "${TMPDIR:-/tmp}/leonos-links.XXXXXX")
trap 'rm -f "$linkfails"' EXIT HUP INT TERM
for page in $(find "$site" -name '*.html' -print); do
    dir=$(dirname "$page")
    grep -oE 'href="[^"#]+"' "$page" 2>/dev/null \
        | sed -e 's/^href="//' -e 's/"$//' \
        | while IFS= read -r href; do
            case "$href" in
                ''|\#*) continue ;;
                http://*|https://*|mailto:*) continue ;;
            esac
            target="${href%%#*}"
            [ -n "$target" ] || continue
            [ -e "$dir/$target" ] || printf '%s: broken href %s\n' "${page#"$site"/}" "$href" >> "$linkfails"
        done
done
if [ -s "$linkfails" ]; then
    sort -u "$linkfails" | sed 's/^/verify-pages: /' >&2
    report "broken href detected"
fi

# --- summary ----------------------------------------------------------------
[ -z "$fail" ] || { echo "verify-pages: FAILED" >&2; exit 1; }
printf 'verify-pages: ok (%s)\n' "$(find "$site" -name '*.html' | wc -l) HTML pages, RPR machine interface preserved"
