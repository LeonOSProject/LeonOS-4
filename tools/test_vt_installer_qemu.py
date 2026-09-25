#!/usr/bin/env python3
"""Exercise installer VT switching and optional installation on a new scratch disk."""
import argparse
from pathlib import Path
import subprocess
import time

from test_installer_accounts_qemu import boot, next_page
from test_vt_qemu import wait_text as wait_screen, ocr

ROOT = Path(__file__).resolve().parents[1]


def wait_text(probe, process, name, needle, timeout=30):
    return wait_screen(probe, process, name, needle, timeout, psm=11)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--iso', type=Path, default=ROOT / 'out/x86_64/release/images/leonos4-installer.iso')
    parser.add_argument('--tui', action='store_true')
    parser.add_argument('--install', action='store_true')
    # The install copy phase runs as one long busybox pass; on slow hosts the
    # 20-minute default truncates a healthy install at ~85% progress. Override
    # per run (e.g. --install-timeout 3600) instead of silently stretching it.
    parser.add_argument('--install-timeout', type=int, default=1200)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    disk = args.output.resolve() / 'scratch.raw'
    subprocess.run(['qemu-img', 'create', '-f', 'raw', str(disk), '4G'], check=True)
    with boot(args.output.resolve(), disk, args.iso.resolve()) as (probe, serial, process):
        if args.tui:
            time.sleep(3)
            probe.key('down')
            probe.key('ret')
            wait_text(probe, process, 'installer-shell', 'built-in shell', timeout=100)
        else:
            wait_text(probe, process, 'installer-language', 'English', timeout=100)
            probe.click(320, 164 if probe.height > 640 else 144)
            wait_text(probe, process, 'installer-english', 'Select Language')
        for number in range(2, 7):
            probe.key(f'ctrl-alt-f{number}')
            wait_text(probe, process, f'installer-tty{number}', 'built-in shell')
        probe.key('ctrl-alt-f1')
        if args.tui:
            probe.text('/usr/lib/leonos/apps/installer/installer.elf')
            probe.key('ret')
            wait_text(probe, process, 'installer-tui-mode', 'Mode [install/update')
            probe.text('install'); probe.key('ret')
            wait_text(probe, process, 'installer-tui-disk', 'Select disk number')
            probe.text('q'); probe.key('ret')
            print('PASS installer TUI and tty1–tty6 shells', flush=True)
            return
        wait_text(probe, process, 'installer-restored', 'Select Language')
        for _ in range(6): next_page(probe)
        wait_text(probe, process, 'installer-accounts', 'Standard user name')
        if args.install:
            # The accounts page initially focuses the username. Keep field
            # changes and typing on the same keyboard queue; USB pointer and
            # PS/2 keyboard commands have independent delivery order.
            for index, text in enumerate(('alice', 'password', 'password', 'rootpass', 'rootpass')):
                probe.text(text)
                if index == 0:
                    wait_text(probe, process, 'installer-username', 'alice')
                if index < 4:
                    probe.key('tab')
            next_page(probe)
            wait_text(probe, process, 'installer-confirm', 'INSTALL')
            probe.key('caps_lock')
            probe.text('install')
            probe.key('caps_lock')
            probe.frame('installer-confirm-typed')
            next_page(probe)
            deadline = time.monotonic() + args.install_timeout
            while time.monotonic() < deadline:
                visible = ocr(probe.frame('installer-progress'), psm=11)
                if 'Installation finished' in visible or 'Installation Complete' in visible:
                    print('PASS installer completed on scratch disk', flush=True)
                    break
                if 'failed' in visible.lower(): raise AssertionError(visible)
                assert process.poll() is None
                time.sleep(5)
            else: raise AssertionError('Installation timed out')
        print(f'PASS installer GUI, VT restore and account input: {disk}', flush=True)


if __name__ == '__main__':
    main()
