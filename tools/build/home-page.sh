#!/bin/sh
# Generate the LeonOS 4 site home page at the Pages root.
#
# Minimal by design (plan §9, §40): an entry point with the version and links to
# Download, RPR/packages, Source and Documentation. It is not a README and it
# carries no commit history, badges or marketing. Version comes from the real
# build artifact; every link is relative so the page works under any Pages base
# path (plan §25). No JavaScript, single shared stylesheet.
set -eu
[ "$#" = 2 ] || exit 2
build=$1 output=$2
version=$(sed -n 's/^#define LEONOS_KERNEL_VERSION "\([0-9.]*\)"$/\1/p' "$build")
printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo 'invalid release version' >&2; exit 1; }
# shellcheck source=/dev/null
. "$(dirname "$0")/site-common.sh"

mkdir -p "$output"
{
    site_page_begin "$(site_css_href 0)" "$(site_home_href 0)" \
        "LeonOS 4" "LeonOS 4"
    printf '<p class="subtitle">A free and open operating system. &middot; Latest version %s</p>\n' \
        "$(printf '%s' "$version" | site_html_escape)"
    cat <<'HTML'
<section>
<h2>Get started</h2>
<p>
<a class="button" href="download/index.html">Download</a>
<a class="button" href="rpr/packages/index.html">Packages</a>
</p>
<ul>
<li><a href="rpr/index.html">RPR</a> &mdash; the Remote Package Repository.</li>
<li><a href="download/index.html">Download</a> &mdash; installer ISO with checksums.</li>
<li><a href="https://github.com/LeonOSProject/LeonOS-4">Source</a> &mdash; project repository.</li>
<li><a href="https://github.com/LeonOSProject/LeonOS-4/tree/main/docs">Documentation</a>.</li>
</ul>
</section>
HTML
    site_page_end
} > "$output/index.html"
