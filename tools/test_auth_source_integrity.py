#!/usr/bin/env python3
"""Verify source integrity and downloads with isolated local HTTP fixtures."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import os
from pathlib import Path
import tarfile
import tempfile
import threading
import unittest
from unittest.mock import patch

from fetch_auth_upstream import fetch


class SourceIntegrity(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="auth-source-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "official.tar.xz"
        with tarfile.open(self.archive, "w:xz") as archive:
            for path, contents in (("official/COPYING", b"license"), ("official/main.c", b"int main(void){return 0;}")):
                member = tarfile.TarInfo(path)
                member.size = len(contents)
                member.mode = 0o644
                archive.addfile(member, io.BytesIO(contents))
            link = tarfile.TarInfo("official/alias.c")
            link.type = tarfile.SYMTYPE
            link.linkname = "./main.c"
            archive.addfile(link)
        self.entry = {"url": "https://example.invalid/official.tar.xz", "version": "1",
            "sha256": hashlib.sha256(self.archive.read_bytes()).hexdigest(),
            "directory": "official", "license_file": "COPYING"}

    def fetch(self):
        return fetch("fixture", self.entry, self.root, self.root / "sources")

    def test_verified_reuse(self):
        self.assertEqual(self.fetch(), self.fetch())

    def test_download_direct_and_environment_proxy(self):
        payload = self.archive.read_bytes()
        requests = []

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                requests.append(self.path)
                self.send_response(200)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, *args):
                pass

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        endpoint = f"http://127.0.0.1:{server.server_port}"
        try:
            for mode in ("direct", "proxy", "bypass"):
                with self.subTest(mode=mode):
                    env = {name: "" for name in ("http_proxy", "https_proxy", "all_proxy", "no_proxy",
                                                "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY")}
                    env["CURL_HOME"] = str(self.root)
                    entry = dict(self.entry, url=endpoint + "/official.tar.xz")
                    if mode == "proxy":
                        env["http_proxy"] = endpoint
                        entry["url"] = "http://upstream.invalid/official.tar.xz"
                    elif mode == "bypass":
                        env["http_proxy"] = "http://127.0.0.1:1"
                        env["no_proxy"] = "127.0.0.1"
                    with patch.dict(os.environ, env):
                        result = fetch("fixture", entry, self.root / (mode + "-cache"),
                                       self.root / (mode + "-sources"))
                    self.assertEqual((result / "main.c").read_bytes(), b"int main(void){return 0;}")
                    self.assertEqual(requests[-1], entry["url"] if mode == "proxy" else "/official.tar.xz")
        finally:
            server.shutdown()
            worker.join()
            server.server_close()

    def test_archive_tampering(self):
        self.archive.write_bytes(b"replaced")
        with self.assertRaises(ValueError):
            self.fetch()

    def test_source_tampering(self):
        source = self.fetch()
        (source / "main.c").write_text("injected")
        with self.assertRaises(ValueError):
            self.fetch()

    def test_injected_file(self):
        source = self.fetch()
        (source / "config.h").write_text("injected")
        with self.assertRaises(ValueError):
            self.fetch()

    def test_replaced_symlink(self):
        source = self.fetch()
        (source / "main.c").unlink()
        (source / "main.c").symlink_to(self.archive)
        with self.assertRaises(ValueError):
            self.fetch()


if __name__ == "__main__":
    unittest.main()
