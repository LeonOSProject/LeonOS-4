#!/usr/bin/env python3
"""Check the executable boot payload, not config source strings."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BootApplets(unittest.TestCase):
    def test_boot_and_network_applets_execute(self):
        binary = ROOT / 'build/userland/busybox.elf'
        applets = subprocess.check_output([binary, '--list'], text=True).splitlines()
        for name in ('init', 'getty', 'ifup', 'ifdown', 'udhcpc', 'ntpd', 'ip', 'awk', 'sed'):
            with self.subTest(applet=name):
                self.assertIn(name, applets)

    def test_environment_probe_uses_exec(self):
        binary = ROOT / 'build/userland/busybox.elf'
        output = subprocess.check_output([binary, 'sh', '-c',
            'VAR=a md5sum /proc/self/environ; VAR=b md5sum /proc/self/environ'], text=True)
        lines = output.splitlines()
        self.assertEqual(len(lines), 2)
        self.assertNotEqual(lines[0].split()[0], lines[1].split()[0],
            'OpenRC must observe each child exec environment, not ash original stack')

if __name__ == '__main__': unittest.main()
