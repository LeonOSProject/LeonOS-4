#!/usr/bin/env python3
"""Regression checks for the repository-built menuconfig frontend."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class KconfigFrontendsTests(unittest.TestCase):
    def test_submodule_is_declared(self) -> None:
        modules = (ROOT / ".gitmodules").read_text(encoding="utf-8")
        self.assertIn('[submodule "third_party/kconfig-frontends"]', modules)
        self.assertIn("https://github.com/movidius/kconfig-frontends.git", modules)

    def test_menuconfig_uses_repository_built_mconf(self) -> None:
        source = (ROOT / "build.py").read_text(encoding="utf-8")
        self.assertIn('name="kconfig-mconf"', source)
        self.assertIn('depends_on=("kconfig-mconf",)', source)
        self.assertIn('if task in {"kconfig-mconf", "menuconfig"}:', source)
        self.assertIn('selected_tests.append("test-kconfig-frontends")', source)
        self.assertIn('"kconfig-frontends",', source)
        self.assertIn('str(kconfig_mconf)', source)
        self.assertNotIn('(\"kconfig-mconf\", \"Kconfig\")', source)
        self.assertNotIn('return (\"kconfig-mconf\",)', source)

    def test_host_builder_only_enables_mconf(self) -> None:
        source = (ROOT / "tools/build_kconfig_frontends.py").read_text(encoding="utf-8")
        self.assertIn('"--enable-frontends=mconf"', source)
        self.assertIn('"--disable-utils"', source)
        self.assertIn('work_dir.parent.mkdir(parents=True, exist_ok=True)', source)
        self.assertIn('shutil.copytree(source, work_dir, ignore=shutil.ignore_patterns(".git"))', source)
        self.assertIn("patch_gperf_compatibility(work_dir)", source)
        self.assertIn("register size_t len", source)
        self.assertIn('prefix / "bin/kconfig-mconf"', source)


if __name__ == "__main__":
    unittest.main()
