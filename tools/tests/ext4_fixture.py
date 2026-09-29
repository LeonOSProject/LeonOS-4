#!/usr/bin/env python3
"""Host-side image fixtures for ReliefOS ext4 tests.

Stable wrappers around the e2fsprogs commands every ext4 test driver needs
(signatures fixed by the ext4 plan, task 1 step 3).  Image creation pins the
feature set with ``-O none,<features>``, so generated images never depend on
the host's mke2fs defaults.
"""

from __future__ import annotations

import hashlib
import subprocess
from pathlib import Path


def run_checked(argv: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    """Run *argv*, raise subprocess.CalledProcessError on failure.

    stdout and stderr are captured and text-decoded on the returned
    CompletedProcess so callers can assert on them.
    """
    return subprocess.run(argv, cwd=cwd, check=True, text=True, capture_output=True)


def create_image(path: Path, size_mib: int, features: list[str], inode_size: int = 256) -> None:
    """Create a fresh ext4 image at *path* with an explicit feature set.

    The file is truncated to *size_mib* MiB first and mke2fs derives the block
    count from the file size.  *features* is joined behind ``-O none,`` so the
    image's feature set is decided only by the argument and never by the host
    mke2fs.conf defaults.  An empty list is rejected because it cannot express
    a feature set.
    """
    if not features:
        raise ValueError(
            "features must name explicit mke2fs features; host defaults are not allowed"
        )
    image = Path(path)
    with image.open("wb") as handle:
        handle.truncate(size_mib * 1024 * 1024)
    run_checked(
        [
            "mke2fs",
            "-t", "ext4",
            "-F",
            "-b", "4096",
            "-I", str(inode_size),
            "-O", "none," + ",".join(features),
            "-m", "0",
            str(image),
        ]
    )


def e2fsck_read_only(path: Path) -> None:
    """Run ``e2fsck -f -n`` on *path*; raise unless the image is clean."""
    run_checked(["e2fsck", "-f", "-n", str(path)])


def debugfs(path: Path, request: str) -> str:
    """Run one ``debugfs -R`` request against *path* and return its stdout."""
    return run_checked(["debugfs", "-R", request, str(path)]).stdout


def sha256_tree(root: Path) -> dict[str, str]:
    """Map each regular file below *root* to the sha256 hex digest of its content.

    Keys are POSIX-style paths relative to *root* in sorted order; directories,
    symlinks and special files are not part of the map.
    """
    base = Path(root)
    digests: dict[str, str] = {}
    for entry in sorted(base.rglob("*")):
        if entry.is_symlink() or not entry.is_file():
            continue
        with entry.open("rb") as handle:
            digest = hashlib.file_digest(handle, "sha256").hexdigest()
        digests[entry.relative_to(base).as_posix()] = digest
    return digests
