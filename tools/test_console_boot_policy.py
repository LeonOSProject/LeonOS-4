#!/usr/bin/env python3
"""Source-level regression checks for console boot mode selection."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ConsoleBootPolicyTests(unittest.TestCase):
    def test_installed_tty_executes_login_without_getty(self):
        script = (ROOT / "system/rootfs/usr/lib/leonos/console-session").read_text()
        tty_case = script.split("tty)", 1)[1].split("installer-tty)", 1)[0]
        self.assertIn("exec /usr/lib/leonos/apps/login/login.elf", tty_case)
        self.assertNotIn("getty", tty_case)

    def test_installer_tty_starts_login_shell(self):
        script = (ROOT / "system/rootfs/usr/lib/leonos/console-session").read_text()
        installer_case = script.split("installer-tty)", 1)[1].split("default|installer|'')", 1)[0]
        self.assertIn("test -f /etc/leonos/installer-runtime", installer_case)
        self.assertIn("cd \"$HOME\"", installer_case)
        self.assertIn("login/login.elf --installer-shell", installer_case)
        self.assertNotIn("installer.elf", installer_case)

    def test_login_launcher_claims_the_console_before_exec(self):
        source = (ROOT / "userland/apps/login/main.c").read_text()
        self.assertIn("ioctl(STDIN_FILENO, TIOCSCTTY, 1)", source)
        self.assertIn('access("/etc/leonos/installer-runtime", F_OK)', source)
        self.assertIn('execl("/bin/sh", "sh", "-l"', source)

    def test_installer_grub_has_one_shell_tty_entry(self):
        config = (ROOT / "boot/grub/installer.cfg").read_text()
        entries = re.findall(r'^menuentry "([^"]+)"', config, re.MULTILINE)
        self.assertEqual([entry for entry in entries if "TTY" in entry],
                         ["Install LeonOS 4 (TTY mode)"])
        self.assertNotIn("installer_advanced=1", config)
        tty_entry = config.split('menuentry "Install LeonOS 4 (TTY mode)"', 1)[1]
        tty_entry = tty_entry.split("}", 1)[0]
        self.assertIn("mode=installer startup=tty", tty_entry)


if __name__ == "__main__":
    unittest.main()
