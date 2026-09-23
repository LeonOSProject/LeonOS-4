#!/usr/bin/env python3
"""Verify boot and authenticated VT sessions on the disposable installed disk."""
import argparse
import time
from pathlib import Path

from test_installer_accounts_qemu import boot
from test_installer_window_qemu import wait_log
from test_vt_qemu import wait_text, wait_graphical


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    with boot(args.output.resolve(), args.disk.resolve()) as (probe, serial, process):
        wait_graphical(probe, process, 'installed-login', timeout=100)
        wait_text(probe, process, 'installed-accounts', 'alice')
        for number in range(2, 7):
            probe.key(f'ctrl-alt-f{number}')
            wait_text(probe, process, f'installed-tty{number}', 'login:')
        probe.key('ctrl-alt-f2')
        probe.text('root'); probe.key('ret')
        wait_text(probe, process, 'installed-root-password', 'Password:')
        probe.text('rootpass'); probe.key('ret')
        wait_text(probe, process, 'installed-root-shell', 'built-in shell')
        probe.text('test ! -e /etc/leonos/installer-runtime && test -t 0 && echo INSTALLED-ROOT-OK >/dev/ttyS0')
        probe.key('ret')
        wait_log(serial, 'INSTALLED-ROOT-OK', process, timeout=30)
        before = serial.read_text().count('path=/bin/login ')
        probe.text('exit'); probe.key('ret')
        deadline = time.monotonic() + 30
        while serial.read_text().count('path=/bin/login ') <= before:
            assert process.poll() is None and time.monotonic() < deadline
            time.sleep(.2)
        wait_text(probe, process, 'installed-login-again', 'login:')
        probe.text('alice'); probe.key('ret')
        wait_text(probe, process, 'installed-user-password', 'Password:')
        probe.text('password'); probe.key('ret')
        time.sleep(2)
        probe.text('clear; id -u'); probe.key('ret')
        # A bare numeric UID avoids OCR confusing the d in "uid" with g.
        visible = wait_text(probe, process, 'installed-user-id', '1000')
        assert '1000' in visible.splitlines(), visible
        probe.text('tty'); probe.key('ret')
        wait_text(probe, process, 'installed-user-tty', 'tty2')
        probe.key('ctrl-alt-f1')
        wait_graphical(probe, process, 'installed-gui-restored')
        wait_text(probe, process, 'installed-gui-accounts', 'alice')
        print('PASS installed boot, six VTs, root/user authentication and tty2 ownership', flush=True)


if __name__ == '__main__':
    main()
