# Shared helpers for the LeonOS static GitHub Pages generators.
#
# Everything here is plain POSIX sh and produces static HTML/CSS only. There is
# deliberately no JavaScript, no client-side rendering and no runtime API use:
# all page content is fixed at build time from real build artifacts (plan §12,
# §22). Generators source this file; they must not re-implement these pieces.

# site_html_escape: HTML-escape text on stdin so it is safe to place inside an
# HTML text node or a quoted attribute. Package names, versions, filenames and
# hashes are build-controlled, but we still escape every dynamic value so the
# generators can never turn a stray character into markup (plan §26, §27).
site_html_escape() {
    sed -e 's/&/\&amp;/g' \
        -e 's/</\&lt;/g' \
        -e 's/>/\&gt;/g' \
        -e 's/"/\&quot;/g' \
        -e "s/'/\&#39;/g"
}

# site_bytes_human <size>: print a byte count as a short human string. Uses
# integer arithmetic only and always appends the unit, keeping the value
# unambiguous for a technical audience.
site_bytes_human() {
    bytes=${1:-0}
    case $bytes in ''|*[!0-9]*) bytes=0 ;; esac
    kb=$(( (bytes + 1023) / 1024 ))
    mb=$(( (bytes + 1048575) / 1048576 ))
    if [ "$bytes" -lt 1024 ]; then
        printf '%s B\n' "$bytes"
    elif [ "$mb" -lt 1024 ]; then
        printf '%s KiB\n' "$kb"
    else
        printf '%s MiB\n' "$mb"
    fi
}

# site_page_begin <css_href> <rel_home> <title> <heading>
#
# Emit the shared document chrome: doctype, a single stylesheet <link>, header,
# top navigation and the opening <main>. Callers feed this with the appropriate
# relative stylesheet path (site_css_href) and relative home link (site_home_href)
# so that a project-page served under /<repo>/ still resolves every link with
# purely relative URLs -- the base path is never baked into the HTML (plan §25).
site_page_begin() {
    css=$1
    home=$2
    title=$3
    heading=$4
    cat <<HEAD
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${title}</title>
<link rel="stylesheet" href="${css}">
</head>
<body>
<div class="page">
<header class="site">
<h1>${heading}</h1>
</header>
<nav class="site">
<a href="${home}index.html">Home</a> &middot;
<a href="${home}download/index.html">Download</a> &middot;
<a href="${home}rpr/index.html">RPR</a>
</nav>
<main>
HEAD
}

# site_page_end: close <main>, emit the shared footer and finish the document.
site_page_end() {
    cat <<'FOOT'
</main>
<footer class="site">
LeonOS Project &mdash; a free and open operating system.
</footer>
</div>
</body>
</html>
FOOT
}

# site_css_href <depth>: relative path from a page at the given directory depth
# (0 = site root, 1 = one level down such as /download/, 2 = /rpr/packages/) to
# the shared stylesheet at the site root. Kept as a helper so no generator
# hardcodes a wrong number of "../".
site_css_href() {
    depth=${1:-0}
    prefix=
    i=0
    while [ "$i" -lt "$depth" ]; do
        prefix="../$prefix"
        i=$((i + 1))
    done
    printf '%scss/leonos.css\n' "$prefix"
}

# site_home_href <depth>: relative path from a page at the given depth back to
# the site root directory (with a trailing slash), used as the nav prefix.
site_home_href() {
    depth=${1:-0}
    prefix=
    i=0
    while [ "$i" -lt "$depth" ]; do
        prefix="../$prefix"
        i=$((i + 1))
    done
    printf '%s\n' "$prefix"
}

# site_hash_for <sha256sums-file> <filename>: read a SHA256SUMS file and echo the
# recorded hash for <filename>, or nothing if absent. Centralised so the RPR and
# download pages all derive their displayed hashes from the same file that ships
# next to them, guaranteeing the page and the checksum agree (plan §28, §46.B).
site_hash_for() {
    [ "$#" = 2 ] || return 0
    sums=$1
    want=$2
    [ -f "$sums" ] || return 0
    awk -v w="$want" '$2==w {print $1; exit}' "$sums"
}
