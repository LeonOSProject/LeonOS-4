#!/usr/bin/env python3
"""Check that LeonOS supervise-daemon services stop within a bounded schedule."""

from pathlib import Path
import re
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
INIT_DIR = ROOT / "system/rootfs/etc/init.d"
EXPECTED_RETRY = "TERM/1/KILL/1"


class OpenRCShutdownTests(unittest.TestCase):
    def test_supervised_services_have_bounded_stop_schedule(self) -> None:
        services = sorted(INIT_DIR.iterdir())
        supervised = []
        for path in services:
            if not path.is_file():
                continue
            text = path.read_text(encoding="utf-8")
            if "supervisor=supervise-daemon" not in text:
                continue
            supervised.append(path.name)
            match = re.search(r"^retry=(\S+)$", text, re.MULTILINE)
            self.assertIsNotNone(match, path.name)
            self.assertEqual(EXPECTED_RETRY, match.group(1), path.name)
        self.assertGreaterEqual(len(supervised), 1)

    def test_init_scripts_are_shell_syntax_valid(self) -> None:
        for path in sorted(INIT_DIR.iterdir()):
            if not path.is_file():
                continue
            text = path.read_text(encoding="utf-8")
            if text.startswith("#!"):
                self.assertEqual(0, subprocess.run(
                    ["sh", "-n", str(path)], check=False,
                ).returncode, path.name)


if __name__ == "__main__":
    unittest.main()
