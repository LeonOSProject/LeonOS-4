#!/usr/bin/env python3
"""Unit tests for APK bootstrap downloads."""

import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from apk_distribution import download_verified


class DownloadVerifiedTests(unittest.TestCase):
    def test_uses_fallback_and_atomically_installs_verified_download(self) -> None:
        payload = b"verified fixture\n"
        expected = hashlib.sha256(payload).hexdigest()
        attempted = []

        def fake_run(command, **_kwargs):
            attempted.append(command[-1])
            output = Path(command[command.index("--output") + 1])
            if command[-1] == "primary":
                output.write_bytes(b"wrong fixture\n")
            else:
                output.write_bytes(payload)

        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "LICENSE"
            with mock.patch("apk_distribution.run", side_effect=fake_run):
                download_verified(("primary", "fallback"), destination, expected)

            self.assertEqual(attempted, ["primary", "fallback"])
            self.assertEqual(destination.read_bytes(), payload)
            self.assertFalse((destination.parent / "LICENSE.download").exists())

    def test_removes_partial_file_when_all_downloads_fail(self) -> None:
        def fake_run(command, **_kwargs):
            output = Path(command[command.index("--output") + 1])
            output.write_bytes(b"partial\n")
            raise subprocess.CalledProcessError(22, command)

        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "LICENSE"
            with mock.patch("apk_distribution.run", side_effect=fake_run):
                with self.assertRaisesRegex(RuntimeError, "unable to download verified file"):
                    download_verified(("primary", "fallback"), destination, "0" * 64)

            self.assertFalse(destination.exists())
            self.assertFalse((destination.parent / "LICENSE.download").exists())


if __name__ == "__main__":
    unittest.main()
