#!/usr/bin/env python3
"""Keep retired bundled toolchains out of production images and SDKs."""
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from build import build_graph
from buildsystem.core.state import BuildPaths


class BuiltinToolRemovalTests(unittest.TestCase):
    def test_incremental_staging_removes_retired_payloads(self):
        with tempfile.TemporaryDirectory(prefix="tool-removal-", dir=ROOT / "build") as directory:
            paths = BuildPaths(Path(directory))
            config = paths.root / "config"
            config.write_text("")
            retired = ("opt/python/bin/python3.14", "opt/dyne/bin/leonos-musl-cc",
                       "opt/tcc/tcc.elf", "usr/bin/python3.14", "usr/bin/gcc",
                       "usr/bin/x86_64-linux-musl-ld", "usr/bin/tcc",
                       "usr/lib/leonos/apps/tcc/manifest.ini",
                       "usr/share/licenses/python/LICENSE",
                       "usr/share/licenses/musl-gcc/COPYING3",
                       "usr/share/licenses/tcc/COPYING",
                       "usr/share/examples/python/hello.py",
                       "usr/share/examples/musl-gcc/hello.c")
            keep = "opt/unrelated/program"
            for name in (*retired, keep):
                entry = paths.staging / name
                entry.parent.mkdir(parents=True, exist_ok=True)
                entry.write_text("fixture")
            alias = paths.staging / "usr/bin/python3"
            alias.symlink_to("python3.14")
            graph = build_graph(paths, config)
            graph.targets["staging-prune"].action(SimpleNamespace(detail=lambda text: None))
            for name in (*retired, "usr/bin/python3"):
                entry = paths.staging / name
                self.assertFalse(entry.exists() or entry.is_symlink(), name)
            self.assertTrue((paths.staging / keep).is_file())

    def test_production_graph_excludes_bundled_toolchains(self):
        with tempfile.TemporaryDirectory(prefix="tool-graph-", dir=ROOT / "build") as directory:
            paths = BuildPaths(Path(directory))
            config = paths.root / "config"
            config.write_text("")
            graph = build_graph(paths, config)
            pending = ["userland", "esp", "sdk"]
            visited = set()
            while pending:
                name = pending.pop()
                if name in visited:
                    continue
                visited.add(name)
                target = graph.targets[name]
                self.assertNotIn(name, {"musl-gcc", "python", "tcc"})
                for output in target.outputs:
                    self.assertFalse(any(part in output.parts for part in
                                         ("musl-gcc", "python", "tcc-runtime")), str(output))
                pending.extend(target.depends_on)
            self.assertIn("musl", visited)


def check_images():
    def stat(image, name):
        result = subprocess.run(["debugfs", "-R", f"stat /{name}", str(image)],
                                capture_output=True, text=True, check=True)
        return result.stdout, result.stderr

    for image, prefixes in ((ROOT / "build/live/root.ext2", ("",)),
                            (ROOT / "build/install/root.fat", ("", "install/root/"))):
        if not image.is_file():
            raise AssertionError(f"Build images first: {image}")
        for prefix in prefixes:
            for name in ("opt/python", "opt/dyne", "opt/tcc", "usr/bin/python",
                         "usr/bin/python3", "usr/bin/python3.14", "usr/bin/gcc",
                         "usr/bin/g++", "usr/bin/musl-gcc", "usr/bin/ld", "usr/bin/as",
                         "usr/bin/tcc", "usr/share/licenses/python",
                         "usr/share/licenses/musl-gcc", "usr/share/licenses/tcc"):
                output, error = stat(image, prefix + name)
                assert not output and "File not found by ext2_lookup" in error, (image, prefix, name, output, error)
            for name in ("bin/busybox", "lib/ld-musl-x86_64.so.1", "usr/bin/vim", "etc/apk/world"):
                output, error = stat(image, prefix + name)
                assert "Inode:" in output, (image, prefix, name, output, error)
            print(f"PASS no bundled Python/GCC/TCC: {image}:{prefix or '/'}")


if __name__ == "__main__":
    images = "--images" in sys.argv
    if images:
        sys.argv.remove("--images")
    result = unittest.main(exit=False).result
    if not result.wasSuccessful():
        raise SystemExit(1)
    if images:
        check_images()
