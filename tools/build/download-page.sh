#!/bin/sh
# Generate the /download/ page and its SHA256SUMS from the real installer ISO.
#
# The page is a static build artifact: the ISO hash, size and version are
# computed here from the file that will actually be published, so what the page
# shows can never disagree with the downloadable file (plan §28, §46.B). The
# ISO download link is a bare relative "leonos4-installer.iso", so it works at
# whatever base path Pages serves the site from (plan §25).
set -eu
[ "$#" = 3 ] || exit 2
iso=$1 build=$2 output=$3
# output is the site root; the download page lives in <output>/download.
site=$output
[ -s "$iso" ] || { echo "missing installer ISO $iso" >&2; exit 1; }
version=$(sed -n 's/^#define LEONOS_KERNEL_VERSION "\([0-9.]*\)"$/\1/p' "$build")
printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo 'invalid release version' >&2; exit 1; }
# shellcheck source=/dev/null
. "$(dirname "$0")/site-common.sh"

mkdir -p "$site/download"
name=${iso##*/}
size=$(stat -c %s "$iso")
iso_hash=$(sha256sum "$iso" | cut -d' ' -f1)
# SHA256SUMS lists the bare filename so `sha256sum -c` runs inside download/.
printf '%s  %s\n' "$iso_hash" "$name" > "$site/download/SHA256SUMS"

{
    site_page_begin "$(site_css_href 1)" "$(site_home_href 1)" \
        "LeonOS 4 Download" "LeonOS 4 Download"
    printf '<p class="subtitle">Latest release &middot; version %s &middot; x86_64</p>\n' \
        "$(printf '%s' "$version" | site_html_escape)"
    cat <<HTML
<section>
<h2>Installer ISO</h2>
<dl class="meta">
<dt>File</dt><dd>$(printf '%s' "$name" | site_html_escape)</dd>
<dt>Architecture</dt><dd>x86_64</dd>
<dt>Size</dt><dd>$(site_bytes_human "$size")</dd>
<dt>SHA256</dt><dd class="hash">$(printf '%s' "$iso_hash" | site_html_escape)</dd>
<div class="clear"></div>
</dl>
<p><a class="button" href="$(printf '%s' "$name" | site_html_escape)">Download</a></p>
<p><a href="SHA256SUMS">SHA256SUMS</a> &mdash; verify with
<code>sha256sum -c SHA256SUMS</code> inside this directory.</p>
</section>
HTML
    site_page_end
} > "$site/download/index.html"
