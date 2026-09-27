#!/usr/bin/env python3
"""Check that all boot environments use six ordinary VT sessions."""
from pathlib import Path
import unittest
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT / "system/rootfs"


class ConsoleBootPolicyTests(unittest.TestCase):
    def test_console_cursor_and_background_output(self):
        with tempfile.TemporaryDirectory(prefix="leonos-vt-console-") as tmp:
            binary = str(Path(tmp) / "console")
            subprocess.run(["clang", "-std=c11", "-g", "-O1",
                            "-ffunction-sections", "-fdata-sections", "-fsanitize=address,undefined",
                            "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi", "-Ikernel/ntclks/kernel/ntclks/include",
                            "-Wl,--gc-sections", "tools/tests/vt_console_test.c", "-o", binary],
                           cwd=ROOT, check=True)
            subprocess.run([binary], check=True, timeout=10)

    def test_init_respawns_a_session_on_each_virtual_terminal(self):
        config = (ROOTFS / "etc/inittab").read_text()
        for number in range(1, 7):
            self.assertIn(
                f"tty{number}::respawn:/usr/lib/leonos/console-session tty{number}",
                config,
            )
        self.assertEqual(config.count("::respawn:"), 6)

    def test_console_session_restores_login_after_graphical_exit(self):
        script = (ROOTFS / "usr/lib/leonos/console-session").read_text()
        self.assertIn("login.elf --graphical-session", script)
        self.assertIn("/run/leonos/graphical-session-started", script)
        self.assertIn("login.elf --installer-shell", script)
        self.assertIn("exec /sbin/getty -n -l", script)
        self.assertLess(script.index("login.elf --graphical-session"),
                        script.index("exec /sbin/getty"))
        self.assertNotIn("LEONOS_BOOT_MODE", script)
        self.assertNotIn("/bin/sleep", script)

    def test_graphical_and_installer_sessions_claim_a_controlling_terminal(self):
        source = (ROOT / "userland/apps/login/main.c").read_text()
        for item in ("setsid()", "TIOCSCTTY", "tcsetpgrp", "VT_ACTIVATE",
                     "KDSETMODE", "KD_GRAPHICS", "KD_TEXT"):
            self.assertIn(item, source)

    def test_runlevels_do_not_start_the_desktop_as_a_service(self):
        runlevels = ROOTFS / "etc/runlevels"
        for name in ("default", "installer"):
            self.assertFalse((runlevels / name / "leonos-desktop").exists())
        self.assertFalse((runlevels / "tty").exists())
        self.assertFalse((runlevels / "installer-tty").exists())
        self.assertTrue((ROOTFS / "etc/leonos/desktop-session").exists())
        self.assertIn("installer-runtime", (ROOTFS / "usr/lib/leonos/rc-default").read_text())

    def test_grub_uses_only_installer_session_selection(self):
        configs = [ROOT / f"boot/grub/{name}.cfg" for name in ("grub", "live", "installer")]
        for config in configs:
            self.assertNotIn("startup=", config.read_text())
        self.assertIn("installer-session=tui", configs[-1].read_text())


if __name__ == "__main__":
    unittest.main()
