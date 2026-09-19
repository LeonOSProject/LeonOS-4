#!/bin/sh
# Explicit UEFI guest check. A timeout alone never counts as a successful boot.
set -eu
[ "$#" = 3 ] || exit 2
images=$1 logs=$2 firmware=$3
[ -f "$firmware" ] || { echo 'test-smoke: QEMU_FIRMWARE is required' >&2; exit 1; }
mkdir -p "$logs"
failures=0
for variant in disk live installer; do
    set -- "${QEMU:-qemu-system-x86_64}" -machine q35 -cpu max -m 4096 -smp 2 \
        -bios "$firmware" -display none -serial stdio -no-reboot -no-shutdown
    case $variant in
    disk) set -- "$@" -snapshot -drive "file=$images/leonos4.vmdk,if=none,id=disk0,format=vmdk" -device ich9-ahci,id=ahci -device ide-hd,drive=disk0,bus=ahci.0 ;;
    *) set -- "$@" -cdrom "$images/leonos4-$variant.iso" ;;
    esac
    result=0
    timeout --kill-after=5 "${SMOKE_TIMEOUT:-90}" "$@" > "$logs/smoke-$variant.log" 2>&1 || result=$?
    if { [ "$result" = 0 ] || [ "$result" = 124 ]; } &&
        grep -F '[ntclks] boot complete:' "$logs/smoke-$variant.log" >/dev/null &&
        grep -F '[ntclks] PID 1 path=' "$logs/smoke-$variant.log" >/dev/null; then
        printf 'ok - %s reached kernel boot and PID 1\n' "$variant"
    else
        printf 'FAIL - %s did not reach boot and PID 1; see %s/smoke-%s.log\n' "$variant" "$logs" "$variant" >&2
        failures=$((failures + 1))
    fi
done
[ "$failures" = 0 ]
