#!/usr/bin/env python3
"""Run the production BusyBox as PID 1 in an isolated Linux namespace.

This tests upstream inittab and power semantics on Linux, not NTCLKS support.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='leonos-pid1-') as directory:
    root = Path(directory)
    for name in ('bin','sbin','etc','proc','dev','run'):
        (root / name).mkdir()
    shutil.copy2(ROOT / 'build/userland/busybox.elf', root / 'bin/busybox')
    for name in ('sh','cat','awk','sleep','reboot','sync','readlink'):
        (root / 'bin' / name).symlink_to('busybox')
    (root / 'sbin/init').symlink_to('../bin/busybox')
    (root / 'dev/null').touch()
    (root / 'etc/inittab').write_text('::sysinit:/bin/sh /sysinit\n::wait:/bin/sh /boot\n::wait:/bin/sh /default\n::shutdown:/bin/sh /shutdown\n')
    (root / 'sysinit').write_text('echo sysinit > /order\nreadlink /proc/1/exe > /pid1\n')
    (root / 'boot').write_text('echo boot >> /order\n/bin/sh /orphan &\n')
    (root / 'orphan').write_text('sleep 1\nawk \'/^PPid:/ {print $2}\' /proc/$$/status > /parent\n')
    (root / 'default').write_text('echo default >> /order\nsleep 2\nreboot\n')
    (root / 'shutdown').write_text('echo shutdown >> /order\nsync\n')
    result = subprocess.run(['unshare','-Urmpf', '--mount-proc=' + str(root / 'proc'),
        'chroot', root, '/sbin/init'], timeout=20, capture_output=True, text=True)
    print(result.stdout, result.stderr)
    assert (root / 'pid1').read_text().strip() == '/bin/busybox'
    assert (root / 'parent').read_text().strip() == '1'
    assert (root / 'order').read_text().splitlines() == ['sysinit','boot','default','shutdown']
    print('PASS real BusyBox PID 1, ordered inittab, orphan adoption and shutdown signal on host Linux')
