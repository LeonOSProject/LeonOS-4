#!/usr/bin/env python3
"""Run the resolver publisher in a private root using the built BusyBox."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class Resolver(unittest.TestCase):
    def test_policy_and_rejected_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for folder in ('bin', 'etc/udhcpc', 'run/leonos', 'dev'):
                (root / folder).mkdir(parents=True)
            shutil.copy2(ROOT / 'build/userland/busybox.elf', root / 'bin/busybox')
            for name in ('sh', 'cat', 'rm', 'mktemp', 'chmod', 'mv'):
                (root / 'bin' / name).symlink_to('busybox')
            (root / 'dev/null').touch()
            shutil.copy2(ROOT / 'system/rootfs/usr/lib/leonos/publish-resolver', root / 'publish')
            (root / 'run/leonos/dhcp-dns').write_text('10.0.2.3\n')
            def publish(policy):
                (root / 'etc/udhcpc/leonos-dns').write_text(policy)
                return subprocess.run(['unshare', '-Ur', 'chroot', root, '/bin/sh', '/publish']).returncode
            for policy, expected in [('0 0.0.0.0\n','1.1.1.1'), ('1 0.0.0.0\n','10.0.2.3'), ('2 8.8.8.8\n','8.8.8.8')]:
                self.assertEqual(publish(policy), 0)
                self.assertIn('nameserver ' + expected + '\n', (root / 'etc/resolv.conf').read_text())
            before = (root / 'etc/resolv.conf').read_bytes()
            self.assertNotEqual(publish('2 $(touch_/owned)\n'), 0)
            self.assertEqual(before, (root / 'etc/resolv.conf').read_bytes())
            self.assertFalse((root / 'owned').exists())
            for address in ('999.1.2.3', '1.2.3', '1..2.3', '1.2.3.4.5', '.1.2.3', '1.2.3.4.'):
                with self.subTest(address=address):
                    self.assertNotEqual(publish('2 ' + address + '\n'), 0)
                    self.assertEqual(before, (root / 'etc/resolv.conf').read_bytes())
if __name__ == '__main__': unittest.main()
