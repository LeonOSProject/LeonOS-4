"""Installer input validation and retained legacy account regressions."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CRYPTO = [f"third_party/mbedtls/library/{name}.c" for name in
          ("md", "pkcs5", "sha1", "sha256", "sha512", "platform_util", "aes")]


class InstallerSetupTests(unittest.TestCase):
    def test_tty_input(self):
        with tempfile.TemporaryDirectory(prefix="leonos-login-input-") as temporary:
            executable = Path(temporary) / "login-input"
            subprocess.run([
                "cc", "-std=c11", "-D_GNU_SOURCE", "-O1", "-g",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "-Wl,--wrap=read,--wrap=write,--wrap=tcgetattr,--wrap=tcsetattr",
                "-Iinclude", "-Iinclude/uapi", "-idirafter", "userland/libc/include",
                "tools/tests/installer_tty_input_test.c", "-o", str(executable),
            ], cwd=ROOT, check=True)
            subprocess.run([str(executable)], check=True, timeout=10)

    def test_setup_validation(self):
        with tempfile.TemporaryDirectory(prefix="leonos-components-test-") as temporary:
            root = Path(temporary)
            executable = root / "setup"
            subprocess.run([
                "cc", "-std=gnu11", "-O1", "-g",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "-Iinclude", "-Iinclude/uapi",
                "-idirafter", "userland/libc/include", "tools/tests/installer_setup_validation_test.c",
                "userland/apps/installer/installer_setup.c", "userland/auth/standard_accounts.c",
                "userland/libc/src/auth_password.c", "-o", str(executable),
            ], cwd=ROOT, check=True)
            subprocess.run([str(executable)], check=True, timeout=30)

    def test_legacy_accounts(self):
        with tempfile.TemporaryDirectory(prefix="leonos-setup-test-") as temporary:
            executable = Path(temporary) / "accounts"
            subprocess.run([
                "cc", "-std=c11", "-D_GNU_SOURCE", "-O1", "-g",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                '-DMBEDTLS_CONFIG_FILE="leonos_mbedtls_config.h"',
                "-Iinclude", "-Iinclude/uapi", "-Ithird_party/mbedtls/include",
                "-idirafter", "userland/libc/include", "tools/tests/installer_accounts_test.c",
                "-Itools/tests/legacy_authd/include",
                "tools/tests/legacy_authd/accounts.c", "userland/libc/src/auth_password.c",
                *CRYPTO, "-o", str(executable),
            ], cwd=ROOT, check=True)
            salt = bytes(range(16))
            derived = hashlib.pbkdf2_hmac("sha256", b"reference-password", salt, 100000)
            reference = f"$pbkdf2-sha256$100000${salt.hex()}${derived.hex()}"
            subprocess.run([str(executable)], check=True, timeout=30,
                           env={**os.environ, "LEONOS_TEST_PASSWORD_HASH": reference})


if __name__ == "__main__":
    unittest.main()
