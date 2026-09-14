#!/usr/bin/env python3
"""Exercise signed packages and ownership using the unmodified apk executable."""
import importlib.util
import json
import os
import re
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


class DistributionTests(unittest.TestCase):
    def test_apk_runner_falls_back_to_fakeroot_when_user_namespace_is_unavailable(self):
        module = ROOT / "tools/apk_distribution.py"
        spec = importlib.util.spec_from_file_location("apk_distribution_fallback", module)
        distribution = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(distribution)
        commands = []

        def fake_run(command, **_kwargs):
            commands.append([str(argument) for argument in command])

        with mock.patch.object(distribution, "run", side_effect=fake_run), \
                mock.patch.object(distribution.shutil, "which", return_value="/usr/bin/fakeroot"):
            distribution._USER_NAMESPACE_AVAILABLE = False
            distribution.make_package(
                "/tmp/apk.static", None, Path("/tmp/payload"), Path("/tmp/output.apk"),
                "fixture", "1.0-r0", [])
            distribution._run_apk("/tmp/apk.static", ["--root", "/tmp/root", "add", "fixture"],
                                  usermode=True)

        self.assertEqual(commands[0][0:2], ["fakeroot", "/tmp/apk.static"])
        self.assertEqual(commands[0][2], "mkpkg")
        self.assertEqual(commands[1][0:4],
                         ["fakeroot", "/tmp/apk.static", "--usermode", "--force-no-chroot"])
        self.assertEqual(commands[1][4:], ["--root", "/tmp/root", "add", "fixture"])

    def test_signed_install_and_fastfetch_conflict(self):
        module = ROOT / "tools/apk_distribution.py"
        self.assertTrue(module.is_file(), "real signed APK distribution builder is missing")
        spec = importlib.util.spec_from_file_location("apk_distribution", module)
        distribution = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(distribution)
        with tempfile.TemporaryDirectory(prefix="apk-transaction-") as directory:
            work = Path(directory)
            source = work / "source"
            for name, content in {"usr/bin/fastfetch": b"LeonOS Logo fixture\n",
                                  "etc/leonos/test.conf": b"original\n",
                                  "usr/bin/sl": b"optional tool\n"}.items():
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
            (source / "etc/sudoers.d").mkdir(mode=0o750)
            (source / "bin").mkdir()
            shutil.copy2(ROOT / "build/userland/busybox.elf", source / "bin/busybox")
            (source / "bin/sh").symlink_to("busybox")
            (source / "bin/bash").symlink_to("busybox")
            (source / "lib").mkdir()
            shutil.copy2(ROOT / "build/musl/sysroot/lib/libc.so", source / "lib/ld-musl-x86_64.so.1")
            for applet in ("ar", "strings"):
                (source / "usr/bin" / applet).symlink_to("../../bin/busybox")
            apk = distribution.bootstrap()
            key = distribution.signing_key(work / "signing/key.pem")
            output = work / "managed"
            distribution.build_distribution(source, output, work / "packages", apk, key)
            header = (ROOT / "include/uapi/leonos/rootfs.h").read_text()
            desktop_path = re.search(r'#define LEONOS_DEFAULT_PATH "([^"]+)"', header).group(1)
            login_defs = (ROOT / "system/rootfs/etc/login.defs").read_text()
            tty_path = re.search(r'^ENV_PATH PATH=(.+)$', login_defs, re.MULTILINE).group(1)
            for session_path in (desktop_path, tty_path):
                result = subprocess.run(
                    ["unshare", "-Ur", "chroot", str(output), "/bin/sh", "-c", "apk --version"],
                    env={**os.environ, "PATH": session_path},
                    text=True, capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("apk-tools", result.stdout)
            local_config = output / "etc/apk/repositories.test"
            local_config.write_text((output / "etc/apk/repositories").read_text().splitlines()[0] + "\n")
            default_repo = subprocess.run(["unshare", "-Ur", "chroot", str(output), "/sbin/apk",
                                           "--repositories-file", "/etc/apk/repositories.test", "update"],
                                          text=True, capture_output=True, timeout=30)
            self.assertEqual(default_repo.returncode, 0, default_repo.stderr)
            self.assertNotIn("WARNING", default_repo.stderr)
            local_config.unlink()
            run = lambda *args: subprocess.run(
                ["unshare", "-Ur", str(apk), "--root", str(output),
                 "--cache-dir", str(output / "var/cache/apk"),
                 "--repositories-file", "/dev/null", *args],
                text=True, capture_output=True, timeout=90)
            result = run("info", "--who-owns", "/usr/bin/fastfetch")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("leonos-fastfetch-", result.stdout)
            self.assertEqual((output / "usr/bin/fastfetch").read_bytes(), b"LeonOS Logo fixture\n")
            self.assertTrue((output / "lib/apk/db/installed").stat().st_size)
            self.assertFalse((output / "bin/bash").exists())
            result = run("info", "--provides", "leonos-busybox")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("/bin/sh", result.stdout.splitlines())
            shell_consumer = distribution.make_package(apk, key, work / "empty",
                work / "shell-consumer.apk", "shell-consumer", "1.0-r0", ["/bin/sh"])
            result = run("add", str(shell_consumer))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(run("del", "shell-consumer").returncode, 0)
            self.assertEqual((output / "etc/sudoers.d").stat().st_mode & 0o7777, 0o750)
            result = run("info", "--exists", "leonos-musl-dev")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("leonos-musl-dev", (output / "etc/apk/world").read_text().splitlines())
            self.assertEqual((output / "usr/include/stdio.h").read_bytes(),
                             (ROOT / "build/musl/sysroot/include/stdio.h").read_bytes())
            self.assertEqual((output / "usr/lib/libc.a").read_bytes(),
                             (ROOT / "build/musl/sysroot/lib/libc.a").read_bytes())
            self.assertEqual((output / "usr/lib/libc.so").resolve(),
                             output / "lib/ld-musl-x86_64.so.1")
            restaged = work / "restaged"
            distribution.build_distribution(output, restaged, work / "restaged-packages", apk, key)
            for tree in (output, restaged):
                for path in ("/usr/include/stdio.h", "/usr/include/crypt.h", "/usr/lib/crt1.o",
                             "/usr/lib/libc.a", "/usr/lib/libc.so", "/usr/lib/libcrypt.a"):
                    result = subprocess.run([str(apk), "--root", str(tree), "--repositories-file",
                        "/dev/null", "info", "--who-owns", path], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("leonos-musl-dev-", result.stdout)
            manifest = json.loads((work / "restaged-packages/manifest.json").read_text())
            self.assertIn("leonos-musl-dev", manifest["packages"])
            self.assertNotIn("leonos-musl-dev", manifest["optional_packages"])
            self.assertEqual(next(entry["group"] for entry in manifest["files"]
                                  if entry["path"] == "/usr/include/stdio.h"), "leonos-musl-dev")
            consumer = distribution.make_package(apk, key, work / "empty", work / "dev-consumer.apk",
                "dev-consumer", "1.0-r0", ["musl-dev", "libc-dev"])
            result = run("add", str(consumer))
            self.assertEqual(result.returncode, 0, result.stderr)
            result = run("del", "dev-consumer")
            self.assertEqual(result.returncode, 0, result.stderr)
            replacement = work / "binutils-payload/usr/bin"
            replacement.mkdir(parents=True)
            for applet in ("ar", "strings"):
                (replacement / applet).write_text("standalone tool\n")
                self.assertTrue((output / "usr/bin" / applet).is_symlink())
            package = distribution.make_package(apk, key, work / "binutils-payload",
                                                 work / "binutils.apk", "binutils", "1.0-r0", [])
            result = run("add", str(package))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((output / "usr/bin/ar").read_text(), "standalone tool\n")
            result = run("del", "binutils")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((output / "usr/bin/ar").is_symlink())
            conflict = distribution.make_package(apk, key, work / "empty", work / "fastfetch.apk",
                                                   "fastfetch", "1.0-r0", [])
            result = run("add", str(conflict))
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("fastfetch", result.stderr)
            self.assertEqual((output / "usr/bin/fastfetch").read_bytes(), b"LeonOS Logo fixture\n")
            result = run("del", "leonos-sl")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((output / "usr/bin/sl").exists())
            (output / "etc/leonos/test.conf").write_text("administrator\n")
            (source / "etc/leonos/test.conf").write_text("new default\n")
            distribution.build_distribution(source, work / "next", work / "next-packages", apk, key)
            repo = work / "next-packages/repository/packages.adb"
            shutil.rmtree(output / "usr/share/leonos/apk/repository")
            shutil.copytree(repo.parent, output / "usr/share/leonos/apk/repository")
            shutil.copy2(ROOT / "userland/storage/leonos-apk-update", output / "usr/lib/leonos/leonos-apk-update")
            result = subprocess.run(["unshare", "-Ur", "chroot", str(output),
                                     "/bin/sh", "/usr/lib/leonos/leonos-apk-update", "/",
                                     "/usr/share/leonos/apk/repository"],
                                    capture_output=True, text=True, timeout=90)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            self.assertFalse((output / "usr/bin/sl").exists(), "update must not reinstall removed packages")
            self.assertEqual((output / "etc/leonos/test.conf").read_text(), "administrator\n")
            self.assertEqual((output / "etc/leonos/test.conf.apk-new").read_text(), "new default\n")
            unsigned = distribution.make_package(apk, None, work / "empty", work / "unsigned.apk",
                                                   "unsigned-test", "1.0-r0", [])
            result = run("add", str(unsigned))
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("UNTRUSTED", result.stderr.upper())


if __name__ == "__main__":
    unittest.main()
