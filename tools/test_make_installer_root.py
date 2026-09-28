#!/usr/bin/env python3
"""Verify canonical installer staging and retained rollback payloads."""
import tempfile
import unittest
from pathlib import Path

from make_installer_root import stage_runtime_payload, stage_installed_payloads


class InstallerLayoutTests(unittest.TestCase):
    def test_runtime_libraries_and_dual_esp_split(self):
        with tempfile.TemporaryDirectory(prefix="reliefos-installer-layout-") as directory:
            work = Path(directory)
            source = work / "source"
            source.mkdir()
            apps = work / "apps"
            apps.mkdir()
            for name in ("imd", "windowd", "desktop", "installer", "busybox"):
                (apps / f"{name}.elf").write_bytes(b"fixture")
            for name in (
                "usr/lib/reliefos/apps/dynlinkerror/dynlinkerror.elf",
                "EFI/BOOT/BOOTX64.EFI", "grub/grub.cfg", "loader.elf",
                "reliefos/kernel.sys", "reliefos/loader.elf",
                "leonos/kernel.sys", "leonos/loader.elf",
            ):
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
            runtime = work / "libleonos.so.2"
            runtime.write_bytes(b"legacy SONAME fixture")
            canonical_runtime = work / "libreliefos.so.2"
            canonical_runtime.write_bytes(b"canonical SONAME fixture")
            gpt = work / "gptinit"
            gpt.write_bytes(b"gpt")
            root = work / "runtime-root"
            stage_runtime_payload(source, root, runtime, apps, gpt, work / "icons", ())
            self.assertEqual((root / "etc/reliefos/installer-runtime").read_text(), "installer\n")
            self.assertEqual((root / "usr/lib/reliefos/libreliefos.so.2").read_bytes(), canonical_runtime.read_bytes())
            self.assertEqual((root / "usr/lib/leonos/libleonos.so.2").read_bytes(), runtime.read_bytes())
            self.assertFalse((root / "usr/lib/reliefos/libleonos.so.2").exists())
            stage_installed_payloads(source, work / "stage")
            for namespace in ("reliefos", "leonos"):
                for name in ("kernel.sys", "loader.elf"):
                    self.assertEqual(
                        (work / "stage/install/esp" / namespace / name).read_bytes(),
                        (source / namespace / name).read_bytes(),
                    )
                self.assertFalse((work / "stage/install/root" / namespace).exists())


if __name__ == "__main__":
    unittest.main()
