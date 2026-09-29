#!/usr/bin/env python3
"""Build a deterministic, interoperable ext4 root image.

The image profile is deliberately explicit.  Host mke2fs defaults are not
part of the ReliefOS image ABI and may change between e2fsprogs releases.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

from image_test_accounts import apply_test_home_ownership

FEATURES = "none,filetype,extents,dir_index,metadata_csum,64bit,flex_bg,has_journal,large_file,huge_file,extra_isize"


def _normalise_tree(stage: Path) -> None:
    os.chown(stage, 0, 0, follow_symlinks=False)
    for directory, dirs, files in os.walk(stage, followlinks=False):
        for name in dirs + files:
            os.chown(Path(directory) / name, 0, 0, follow_symlinks=False)
    apply_test_home_ownership(stage)


def _populate_in_fakeroot(stage: Path, image: Path, inode_count: int, uuid_text: str) -> None:
    if not os.environ.get("FAKEROOTKEY"):
        raise RuntimeError("image ownership normalization requires fakeroot")
    _normalise_tree(stage)
    subprocess.run([
        "mke2fs", "-q", "-t", "ext4", "-F", "-b", "4096", "-I", "256",
        "-O", FEATURES, "-m", "0", "-U", uuid_text,
        "-E", f"root_owner=0:0,hash_seed={uuid_text}", "-N", str(inode_count),
        "-d", str(stage), str(image),
    ], check=True)


def populate_ext4(stage: Path, image: Path, inode_count: int, uuid_text: str) -> None:
    """Populate an image through fakeroot so guest ownership is root:root."""
    subprocess.run([
        "fakeroot", "--", sys.executable, str(Path(__file__).resolve()),
        "--populate", str(stage.resolve()), str(image.resolve()),
        "--inodes", str(inode_count), "--uuid", uuid_text,
    ], check=True)


def write_ext4_root(stage: Path, out: Path, uuid_text: str,
                    minimum_mib: int = 64) -> None:
    entries = [p.lstat() for p in stage.rglob("*")]
    unique = {(s.st_dev, s.st_ino): s for s in entries if stat.S_ISREG(s.st_mode)}
    allocated = sum(((s.st_size + 4095) // 4096) * 4096 for s in unique.values())
    required = allocated + len(entries) * 768 + sum(stat.S_ISDIR(s.st_mode) for s in entries) * 4096 + (64 << 20)
    size_mib = max(minimum_mib, ((required + (32 << 20) - 1) // (32 << 20)) * 32)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".ext4-root-", dir=out.parent) as directory:
        image = Path(directory) / "root.ext4"
        image.write_bytes(b"")
        with image.open("r+b") as stream:
            stream.truncate(size_mib << 20)
        populate_ext4(stage, image, max(8192, len(entries) * 2), uuid_text)
        subprocess.run(["e2fsck", "-f", "-n", str(image)], check=True)
        image.replace(out)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--populate", nargs=2, type=Path, metavar=("STAGE", "IMAGE"))
    parser.add_argument("--inodes", type=int)
    parser.add_argument("--uuid", required=True)
    args = parser.parse_args()
    if args.populate is None or args.inodes is None:
        parser.error("--populate STAGE IMAGE, --inodes and --uuid are required")
    _populate_in_fakeroot(*args.populate, args.inodes, args.uuid)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
