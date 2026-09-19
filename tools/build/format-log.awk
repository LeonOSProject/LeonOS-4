# Render upstream lines without dropping diagnostics. Raw bytes are logged by
# run-logged before this filter. CR progress updates become ordinary lines.
BEGIN { RS="\r|\n" }
{
    gsub(/\033\[[0-9;?]*[ -\/]*[@-~]/, "")
    sub(/^[[:space:]]+/, "")
    sub(/[[:space:]]+$/, "")
    if ($0 == "") next
    label=tag
    text=$0
    if (tag == "XORRISO") sub(/^xorriso[[:space:]]*:[[:space:]]*/, "", text)
    if (text ~ /(^|[[:space:]:])(ERROR|FAILURE|FATAL|ABORT)([[:space:]:]|$)/) {
        label="ERROR"; text=tag ": " text
    } else if (text ~ /(^|[[:space:]:])(WARNING|WARN)([[:space:]:]|$)/) {
        label="WARN"; text=tag ": " text
    }
    printf "  %-8s %s\n", label, text
    fflush()
}
