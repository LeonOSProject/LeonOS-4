#!/usr/bin/env python3
"""Assemble the signed LeonOS GitHub Pages remote package repository."""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from apk_distribution import bootstrap


VERSION_RE = re.compile(r'^#define LEONOS_KERNEL_VERSION "([0-9]+\.[0-9]+\.[0-9]+-[0-9]+)"$', re.MULTILINE)
BUILD_RE = re.compile(r"^#define LEONOS_BUILD_NUMBER ([0-9]+)$", re.MULTILINE)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(arguments: list[object], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run([str(argument) for argument in arguments], check=True, **kwargs)


def signing_key() -> Path:
    configured = os.environ.get("LEONOS_APK_SIGNING_KEY", "")
    if not configured:
        raise RuntimeError(
            "LEONOS_APK_SIGNING_KEY must name the abuild-keygen private key"
        )
    key = Path(configured).absolute()
    if key.is_symlink() or not key.is_file():
        raise RuntimeError("APK signing key must be a regular, non-symlink file")
    if key.stat().st_mode & 0o077:
        raise RuntimeError("APK signing key must have mode 0600")
    return key


def parse_version(header: Path) -> tuple[str, int]:
    text = header.read_text(encoding="utf-8")
    version_match = VERSION_RE.search(text)
    build_match = BUILD_RE.search(text)
    if not version_match or not build_match:
        raise RuntimeError(f"missing LeonOS version metadata in {header}")
    version = version_match.group(1)
    build = int(build_match.group(1))
    if version.rsplit("-", 1)[1] != str(build):
        raise RuntimeError("kernel version and build number disagree")
    return version, build


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def build_pages(repository: Path, kernel: Path, middlelayer: Path,
                build_info: Path, output: Path) -> None:
    key = signing_key()
    version, build_number = parse_version(build_info)
    image_version = version.rsplit("-", 1)[0]
    if not repository.is_dir():
        raise RuntimeError(f"APK repository does not exist: {repository}")
    if not kernel.is_file() or not middlelayer.is_file():
        raise RuntimeError("kernel and middle-layer artifacts must both exist")

    packages = sorted(repository.glob("leonos-*.apk"))
    if not packages:
        raise RuntimeError("APK repository contains no LeonOS packages")

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".rpr-pages-", dir=output.parent) as directory:
        stage = Path(directory) / "site"
        apk_directory = stage / "apk"
        kernel_directory = stage / "kernel"
        apk_directory.mkdir(parents=True)
        kernel_directory.mkdir(parents=True)

        public_key = apk_directory / "leonos-rpr.rsa.pub"
        run(["openssl", "pkey", "-in", key, "-pubout", "-out", public_key],
            stderr=subprocess.DEVNULL)
        public_key.chmod(0o644)
        for package in packages:
            shutil.copy2(package, apk_directory / package.name)

        apk = bootstrap()
        run([
            apk, "mkndx", "--keys-dir", apk_directory, "--sign-key", key,
            "--output", apk_directory / "packages.adb",
            *(apk_directory / package.name for package in packages),
        ])

        apk_files = [public_key, apk_directory / "packages.adb",
                     *(apk_directory / package.name for package in packages)]
        apk_checksums = "".join(
            f"{digest(path)}  {path.name}\n" for path in sorted(apk_files)
        )
        (apk_directory / "SHA256SUMS").write_text(apk_checksums, encoding="ascii")
        write_json(apk_directory / "repository.json", {
            "architecture": "x86_64",
            "index": "packages.adb",
            "packages": [package.name for package in packages],
            "public_key": public_key.name,
            "schema": 1,
        })

        published_kernel = kernel_directory / "kernel.sys"
        published_middlelayer = kernel_directory / "middlelayer.sys"
        shutil.copy2(kernel, published_kernel)
        shutil.copy2(middlelayer, published_middlelayer)
        kernel_hash = digest(published_kernel)
        middlelayer_hash = digest(published_middlelayer)
        release_lines = (
            "format_version=1\n"
            f"image_version={image_version}\n"
            f"version={version}\n"
            f"build_number={build_number}\n"
            "kernel_file=kernel.sys\n"
            f"kernel_sha256={kernel_hash}\n"
            "middlelayer_file=middlelayer.sys\n"
            f"middlelayer_sha256={middlelayer_hash}\n"
        )
        (kernel_directory / "release.txt").write_text(release_lines, encoding="ascii")
        (kernel_directory / "SHA256SUMS").write_text(
            f"{kernel_hash}  kernel.sys\n{middlelayer_hash}  middlelayer.sys\n",
            encoding="ascii",
        )
        write_json(kernel_directory / "release.json", {
            "architecture": "x86_64",
            "build_number": build_number,
            "image_version": image_version,
            "files": {
                "kernel.sys": {"sha256": kernel_hash},
                "middlelayer.sys": {"sha256": middlelayer_hash},
            },
            "schema": 1,
            "version": version,
        })

        (stage / ".nojekyll").write_text("", encoding="ascii")
        (stage / "health.txt").write_text("leonos-rpr-ok\n", encoding="ascii")
        (stage / "index.html").write_text(
            "<!doctype html><meta charset=utf-8><title>LeonOS RPR</title>"
            "<h1>LeonOS 4 Remote Package Repository</h1>"
            f"<p>Latest kernel: {html.escape(version)}</p>"
            "<ul><li><a href=\"apk/repository.json\">APK repository</a></li>"
            "<li><a href=\"kernel/release.json\">Kernel release</a></li></ul>\n",
            encoding="utf-8",
        )
        write_json(stage / "manifest.json", {
            "apk": "/apk/packages.adb",
            "kernel": "/kernel/release.txt",
            "schema": 1,
            "version": version,
        })
        (stage / ".complete").write_text(version + "\n", encoding="ascii")

        if output.exists():
            if output.is_symlink() or not output.is_dir():
                raise RuntimeError(f"refusing to replace non-directory output: {output}")
            shutil.rmtree(output)
        stage.replace(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--kernel", required=True, type=Path)
    parser.add_argument("--middlelayer", required=True, type=Path)
    parser.add_argument("--build-info", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    build_pages(arguments.repository.absolute(), arguments.kernel.absolute(),
                arguments.middlelayer.absolute(), arguments.build_info.absolute(),
                arguments.output.absolute())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
