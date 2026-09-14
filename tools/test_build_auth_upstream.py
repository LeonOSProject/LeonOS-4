#!/usr/bin/env python3
"""Unit tests for target authentication package staging helpers."""

from pathlib import Path
import tempfile
import unittest

from build_auth_upstream import normalize_libbsd_linker_script


class LibbsdLinkerScriptTests(unittest.TestCase):
    def test_rewrites_staged_absolute_soname_without_dropping_libmd(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            script = root / "lib/libbsd.so"
            script.parent.mkdir(parents=True)
            script.write_text(
                "/* GNU ld script */\n"
                "OUTPUT_FORMAT(elf64-x86-64)\n"
                "GROUP(/lib/libbsd.so.0.12.2 AS_NEEDED(-lmd))\n",
                encoding="utf-8",
            )

            normalize_libbsd_linker_script(root)

            self.assertEqual(
                script.read_text(encoding="utf-8"),
                "/* GNU ld script */\n"
                "OUTPUT_FORMAT(elf64-x86-64)\n"
                "GROUP(libbsd.so.0.12.2 AS_NEEDED(-lmd))\n",
            )

    def test_rejects_unexpected_script_layout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            script = root / "lib/libbsd.so"
            script.parent.mkdir(parents=True)
            script.write_text("GROUP(libbsd.so.0 AS_NEEDED(-lmd))\n", encoding="utf-8")

            with self.assertRaisesRegex(RuntimeError, "unexpected libbsd linker script"):
                normalize_libbsd_linker_script(root)


if __name__ == "__main__":
    unittest.main()
