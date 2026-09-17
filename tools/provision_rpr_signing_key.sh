#!/bin/sh
# Generate the RPR APK key with Alpine abuild-keygen and store it in GitHub Secrets.
set -eu

if [ "$#" -ne 1 ]; then
    printf '%s\n' 'Usage: provision_rpr_signing_key.sh OWNER/REPOSITORY' >&2
    exit 2
fi
repository=$1
case "$repository" in
    */*) ;;
    *) printf '%s\n' 'Repository must be OWNER/REPOSITORY' >&2; exit 2 ;;
esac
command -v abuild-keygen >/dev/null 2>&1 || {
    printf '%s\n' 'abuild-keygen is required (install Alpine alpine-sdk)' >&2
    exit 1
}
command -v gh >/dev/null 2>&1 || {
    printf '%s\n' 'GitHub CLI is required and must already be authenticated' >&2
    exit 1
}
command -v openssl >/dev/null 2>&1 || {
    printf '%s\n' 'OpenSSL is required' >&2
    exit 1
}

work=$(mktemp -d "${TMPDIR:-/tmp}/leonos-rpr-key.XXXXXX")
cleanup() {
    if command -v shred >/dev/null 2>&1; then
        find "$work" -type f -exec shred -u -- {} \; 2>/dev/null || true
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT HUP INT TERM
key_dir="$work/keys"
mkdir -p "$key_dir"
chmod 0700 "$work" "$key_dir"

# ABUILD_USERDIR is abuild's dedicated per-user key/config directory. Keep all
# generated material in the disposable workspace without changing HOME.
env ABUILD_USERDIR="$key_dir" abuild-keygen -a -n -b 4096
private_key=$(find "$key_dir" -maxdepth 1 -type f -name '*.rsa' ! -name '*.rsa.pub' -print)
public_key=$(find "$key_dir" -maxdepth 1 -type f -name '*.rsa.pub' -print)
[ "$(printf '%s\n' "$private_key" | wc -l)" -eq 1 ] && [ -f "$private_key" ] || {
    printf '%s\n' 'abuild-keygen did not create exactly one private key' >&2
    exit 1
}
[ "$(printf '%s\n' "$public_key" | wc -l)" -eq 1 ] && [ -f "$public_key" ] || {
    printf '%s\n' 'abuild-keygen did not create exactly one public key' >&2
    exit 1
}
chmod 0600 "$private_key"
openssl pkey -in "$private_key" -noout -check >/dev/null
openssl base64 -A -in "$private_key" |
    gh secret set LEONOS_APK_SIGNING_KEY_B64 --repo "$repository"

fingerprint=$(openssl pkey -pubin -in "$public_key" -outform DER |
    openssl dgst -sha256 | sed 's/^.*= //')
printf 'Stored LEONOS_APK_SIGNING_KEY_B64 for %s\n' "$repository"
printf 'Public-key SHA-256: %s\n' "$fingerprint"
printf '%s\n' 'The temporary keypair will now be removed.'
