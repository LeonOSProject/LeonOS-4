#!/bin/sh
# Run after make leonos-pam; verify the product module's target ABI.
set -eu
: "${PAM_ROOT:?new-chain PAM staging root required}"
module=$PAM_ROOT/lib/security/pam_leonos_password.so
test -f "$module"
readelf -h "$module" | grep -q 'Advanced Micro Devices X86-64'
readelf -d "$module" | grep -q 'NEEDED.*libpam.so.0'
readelf --dyn-syms --wide "$module" | grep -q 'GLOBAL.*DEFAULT.*pam_sm_chauthtok$'
readelf --dyn-syms --wide "$module" | grep -q 'GLOBAL.*DEFAULT.*leonos_auth_password_valid$'
printf '%s\n' 'ok - LeonOS PAM product module ABI'
