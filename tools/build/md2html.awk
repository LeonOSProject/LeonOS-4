# md2html.awk — a dependency-free Markdown-to-HTML converter.
#
# The LeonOS production build must not shell out to Python, Node or any other
# runtime (Makefile contract), so this is a self-contained awk program covering
# exactly the Markdown surface the docs/ tree actually uses:
#   - ATX headings (#..######) with GitHub-style slug ids (unique per page)
#   - fenced code blocks (``` with optional info string) as <pre><code>, and
#     indented fences inside list items (docs use those)
#   - GitHub pipe tables (header row + |---| separator)
#   - ordered / unordered / task lists, with continuation lines folded into the
#     preceding <li> so multi-line list items render as one item
#   - blockquotes
#   - inline: [text](url), `code`, **bold**, *em*
# Raw text is HTML-escaped before any markup is added, so angle-bracket tokens
# that appear all over the docs (<app>, <pid>, <sha256>, ...) render literally
# and can never inject markup. Output is static HTML fragments only: no
# <script>, no event handler, no client-side anything.
#
# Compatibility target is mawk (the Debian default) as well as gawk: no brace
# intervals ({n,m}), no back-references, no POSIX character classes inside
# brackets. All constructs are anchored with a manual helper where needed.
#
# Usage:  awk -f md2html.awk file.md
# Link targets are sanitised (javascript: / data: / unknown schemes become
# inert "#"), and a relative ".md" link is rewritten to the per-file
# "NAME/index.html" convention docs-page.sh emits, so sibling documents link
# to sibling HTML pages instead of to raw Markdown.

function esc(s) {
    gsub(/&/, "\\&amp;", s)
    gsub(/</, "\\&lt;", s)
    gsub(/>/, "\\&gt;", s)
    return s
}

# slug: GitHub-flavoured heading id, made unique per page by an int counter map.
function slug(s,   o) {
    o = tolower(s)
    gsub(/[^a-z0-9 _-]/, "", o)
    gsub(/[ \t]+/, "-", o)
    gsub(/-+/, "-", o)
    sub(/^-/, "", o); sub(/-$/, "", o)
    if (o == "") o = "section"
    if (o in seen) { seen[o]++; o = o "-" seen[o] } else { seen[o] = 0 }
    return o
}

function find_char(s, ch, pos,   p) {
    p = index(substr(s, pos), ch)
    return p ? pos + p - 1 : 0
}

# --- inline pipeline ---------------------------------------------------------
# Text arrives HTML-escaped; we only add tags.
#
# Order: code spans first (so their inner characters are never reprocessed),
# then bold (**.. and __..), then single-star emphasis, then markdown links
# last (so a URL cannot accidentally get wrapped in <em> or <strong>).

function inline_md(s) {
    s = code_spans(s)
    s = wrap_pairs(s, "**", "strong")
    s = wrap_pairs(s, "__", "strong")
    s = wrap_em_single(s, "*")
    s = links(s)
    return s
}

# code_spans: replace `x` with <code>x</code>. Content is already escaped so we
# emit it verbatim inside <code>.
function code_spans(s,   out, i, c, j) {
    out = ""; i = 1
    while (i <= length(s)) {
        c = substr(s, i, 1)
        if (c == "`") {
            j = find_char(s, "`", i + 1)
            if (j == 0) { out = out c; i++; continue }
            out = out "<code>" substr(s, i + 1, j - i - 1) "</code>"
            i = j + 1; continue
        }
        out = out c; i++
    }
    return out
}

# wrap_pairs: for two-char delimiters like ** __, scan opener then closer.
function wrap_pairs(s, d, tag,   out, i, k, j, j2, openc) {
    out = ""; i = 1; k = length(d); openc = substr(d, 1, 1)
    while (i <= length(s)) {
        j = find_char(s, openc, i)
        if (j == 0) { out = out substr(s, i); break }
        if (substr(s, j, k) != d) { out = out substr(s, i, j - i + 1); i = j + 1; continue }
        j2 = find_char(s, openc, j + k)
        while (j2 && substr(s, j2, k) != d) j2 = find_char(s, openc, j2 + 1)
        if (j2 == 0) { out = out substr(s, i, j + k - i); i = j + k; continue }
        out = out substr(s, i, j - i) "<" tag ">" substr(s, j + k, j2 - j - k) "</" tag ">"
        i = j2 + k
    }
    return out
}

# wrap_em_single: single-char *...*, skipping doubled markers (which ** already
# handled). If a lone opener has no lone closer we leave the * as literal text.
function wrap_em_single(s, d,   out, i, j, k) {
    out = ""; i = 1
    while (i <= length(s)) {
        j = find_char(s, d, i)
        if (j == 0) { out = out substr(s, i); break }
        if (substr(s, j + 1, 1) == d) { out = out substr(s, i, j - i + 1); i = j + 2; continue }
        k = find_char(s, d, j + 1)
        while (k && substr(s, k + 1, 1) == d) k = find_char(s, d, k + 1)
        if (k == 0) { out = out substr(s, i); break }
        out = out substr(s, i, j - i) "<em>" substr(s, j + 1, k - j - 1) "</em>"
        i = k + 1
    }
    return out
}

# links: [text](url) -> <a href="sanitized">text</a>. Text is already escaped;
# we only build the anchor.
function links(s,   out, i, lb, rb, pe, url, txt) {
    out = ""; i = 1
    while (i <= length(s)) {
        lb = find_char(s, "[", i)
        if (lb == 0) { out = out substr(s, i); break }
        rb = find_char(s, "]", lb + 1)
        if (rb == 0) { out = out substr(s, i); break }
        if (substr(s, rb + 1, 1) != "(") { out = out substr(s, i, rb - i + 1); i = rb + 1; continue }
        pe = find_char(s, ")", rb + 2)
        if (pe == 0) { out = out substr(s, i); break }
        txt = substr(s, lb + 1, rb - lb - 1)
        url = substr(s, rb + 2, pe - rb - 2)
        out = out substr(s, i, lb - i) "<a href=\"" rewrite_href(url) "\">" txt "</a>"
        i = pe + 1
    }
    return out
}

# rewrite_href: scheme allowlist + localise relative ".md" links to the sibling
# "NAME/index.html" that docs-page.sh emits. Absolute and unknown-scheme URLs
# become inert "#". Double quotes in href values are percent-encoded.
#
# docs-page.sh lays out one directory per document (docs/<NAME>/index.html) and
# copies every asset to docs/<basename>. So from a rendered document, a sibling
# document or asset always lives one level up: a relative target becomes
# "../<basename>" (or "../<basename>/index.html" for a ".md" link). Anchors and
# external URLs are left untouched.
function rewrite_href(u,   frag, p, name) {
    gsub(/"/, "%22", u)
    if (u == "") return u
    if (u ~ /^#/) return u
    if (u ~ /^[a-zA-Z][a-zA-Z0-9+.-]*:/) {
        if (u ~ /^https?:/ || u ~ /^mailto:/) return u
        return "#"
    }
    frag = ""
    p = index(u, "#")
    if (p > 0) { frag = substr(u, p); u = substr(u, 1, p - 1) }
    # absolute filesystem path from an in-repo note: not servable, keep inert.
    if (u ~ /^\//) return "#"
    # flatten any ./ ../ or subdirectory prefix: the docs tree is by basename.
    sub(/^\.\//, "", u)
    while (u ~ /^\.\.\//) { sub(/^\.\.\//, "", u) }
    if (u ~ /\//) { sub(/^.*\//, "", u) }
    if (u == "") return "#"
    # relative sibling: rewrite ".md" to "NAME/index.html" and step up a level.
    if (u ~ /\.md$/) { sub(/\.md$/, "/index.html", u) }
    return "../" u frag
}

# --- block-level helpers -----------------------------------------------------

function cellize(s, arr,   a, m, i, c) {
    gsub(/^[ \t]*\|/, "", s); gsub(/\|[ \t]*$/, "", s)
    m = split(s, a, "|")
    for (i = 1; i <= m; i++) { c = a[i]; gsub(/^[ \t]+/, "", c); gsub(/[ \t]+$/, "", c); arr[i] = c }
    return m
}
# is_sep: table separator row of the form |---|:--|---:| (mawk-safe, no braces).
function is_sep(s,   t, i, c, dash) {
    t = s
    gsub(/^[ \t]*\|?/, "", t); gsub(/\|?[ \t]*$/, "", t)
    if (t == "") return 0
    # split on '|' and require every field to be :?-+:?
    m_split = split(t, cells, "|")
    for (i = 1; i <= m_split; i++) {
        c = cells[i]; gsub(/[ \t]/, "", c)
        if (c == "") return 0
        sub(/^:/, "", c); sub(/:$/, "", c)
        if (c == "") return 0
        for (j = 1; j <= length(c); j++) if (substr(c, j, 1) != "-") return 0
    }
    return 1
}
# is_hr: line of 3+ repeats of - * _ with any spaces (mawk has no back-refs).
function is_hr(s,   t, ch, i) {
    t = s; gsub(/[ \t]/, "", t)
    if (length(t) < 3) return 0
    ch = substr(t, 1, 1)
    if (ch != "-" && ch != "*" && ch != "_") return 0
    for (i = 2; i <= length(t); i++) if (substr(t, i, 1) != ch) return 0
    return 1
}

# --- list buffering ----------------------------------------------------------
# A list <li> may span multiple physical lines: the first carries the marker,
# subsequent indented lines continue it. We accumulate items in an array and
# flush at list_close().

function li_start(text, task) {
    li_count++
    li[li_count] = text
    li_task[li_count] = task
}
function li_append(text) {
    if (li_count == 0) return
    li[li_count] = li[li_count] " " text
}
function list_close() {
    if (lt == "") return
    if (lt == "ol") print "<ol>"
    else if (li_has_task) print "<ul class=\"task\">"
    else print "<ul>"
    for (i = 1; i <= li_count; i++) {
        if (li_task[i]) print "<li>" (li_task[i] == "x" ? "&#9745;" : "&#9744;") " " inline_md(li[i]) "</li>"
        else print "<li>" inline_md(li[i]) "</li>"
    }
    if (lt == "ol") print "</ol>"; else print "</ul>"
    lt = ""; li_count = 0; li_has_task = 0
}

function para_flush() {
    if (pbuf != "") { print "<p>" inline_md(pbuf) "</p>"; pbuf = "" }
}

BEGIN {
    incode = 0
    lt = ""; li_count = 0; li_has_task = 0
    pbuf = ""
    n = 0
    for (i = 1; i <= 8; i++) li[i] = ""
}

{ src[n++] = $0 }

END {
    for (ln = 0; ln < n; ln++) {
        raw = src[ln]

        # fenced code (top-level or indented; we only require the fence token
        # after optional leading spaces). Info string becomes language-* class.
        if (raw ~ /^[ \t]*```/) {
            if (!incode) {
                para_flush(); list_close()
                incode = 1
                fence_indent = raw; sub(/```.*/, "", fence_indent)   # leading spaces
                lang = raw; sub(/^[ \t]*```/, "", lang); gsub(/[^a-zA-Z0-9_-]/, "", lang)
                printf "<pre><code class=\"language-%s\">\n", lang
                continue
            }
            print "</code></pre>"; incode = 0; continue
        }
        if (incode) {
            strip = raw
            # de-indent code up to the fence's indent for readability.
            k = 0
            while (k < length(fence_indent) && substr(strip, k + 1, 1) == " ") k++
            if (k > length(fence_indent)) k = length(fence_indent)
            if (k > 0) strip = substr(strip, k + 1)
            print esc(strip)
            continue
        }

        # blank line
        if (raw ~ /^[ \t]*$/) { para_flush(); if (lt != "") { } ; continue }

        # table: current row has '|' AND next row is a separator
        if (raw ~ /\|/ && ln + 1 < n && is_sep(src[ln + 1])) {
            para_flush(); list_close()
            nh = cellize(raw, hdr)
            print "<table>"
            printf "<tr>"
            for (i = 1; i <= nh; i++) printf "<th>%s</th>", inline_md(esc(hdr[i]))
            print "</tr>"
            ln++
            while (ln + 1 < n && src[ln + 1] ~ /\|/ && src[ln + 1] !~ /^[ \t]*$/) {
                ln++
                nr = cellize(src[ln], row)
                printf "<tr>"
                for (i = 1; i <= nr; i++) printf "<td>%s</td>", inline_md(esc(row[i]))
                print "</tr>"
            }
            print "</table>"
            continue
        }

        # heading
        if (raw ~ /^[ \t]*#+[ \t]/) {
            para_flush(); list_close()
            lvl = raw; sub(/[^#].*/, "", lvl); L = length(lvl)
            title = raw; sub(/^[ \t]*#+[ \t]+/, "", title); sub(/[ \t]+#+[ \t]*$/, "", title)
            id = slug(title)
            printf "<h%d id=\"%s\">%s</h%d>\n", L, id, inline_md(esc(title)), L
            continue
        }

        # blockquote (single-line; the docs never use multi-line quotes)
        if (raw ~ /^[ \t]*>/) {
            para_flush(); list_close()
            q = raw; sub(/^[ \t]*>[ \t]?/, "", q)
            print "<blockquote><p>" inline_md(esc(q)) "</p></blockquote>"
            continue
        }

        # horizontal rule
        if (is_hr(raw)) { para_flush(); list_close(); print "<hr>"; continue }

        # task list item: - [ ] text / - [x] text
        if (raw ~ /^[ \t]*[-*+][ \t]+\[[ xX]\][ \t]/) {
            if (lt == "") { lt = "ul"; li_has_task = 1 }
            else if (lt != "ul") { list_close(); lt = "ul"; li_has_task = 1 }
            it = raw; sub(/^[ \t]*[-*+][ \t]+/, "", it)
            done = (it ~ /^\[[xX]\]/ ? "x" : " ")
            sub(/^\[[ xX]\][ \t]+/, "", it)
            li_start(esc(it), done)
            continue
        }

        # unordered list item
        if (raw ~ /^[ \t]*[-*+][ \t]/) {
            if (lt == "") lt = "ul"
            else if (lt != "ul") { list_close(); lt = "ul" }
            it = raw; sub(/^[ \t]*[-*+][ \t]+/, "", it)
            li_start(esc(it), "")
            continue
        }

        # ordered list item
        if (raw ~ /^[ \t]*[0-9]+\.[ \t]/) {
            if (lt == "") lt = "ol"
            else if (lt != "ol") { list_close(); lt = "ol" }
            it = raw; sub(/^[ \t]*[0-9]+\.[ \t]+/, "", it)
            li_start(esc(it), "")
            continue
        }

        # continuation of an open list item: indented plain text
        if (lt != "" && raw ~ /^[ \t]+[^ \t]/) {
            it = raw; sub(/^[ \t]+/, "", it)
            li_append(esc(it))
            continue
        }

        # anything else closes any open list.
        list_close()

        # paragraph accumulation
        pbuf = (pbuf == "") ? esc(raw) : pbuf " " esc(raw)
        # flush at next block boundary (we cannot lookahead cheaply, so the
        # next line's classification will flush on entry).
    }
    if (incode) print "</code></pre>"
    para_flush(); list_close()
}
