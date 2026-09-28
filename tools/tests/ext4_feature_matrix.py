#!/usr/bin/env python3
"""ReliefOS ext4 feature support matrix (machine readable).

This module is the source of truth for how the 2026-09-28 ext4 plan treats
every ext4 feature bit it names, so later tasks never have to guess a bit
value.  Masks are the values from the in-tree Linux reference
``linux/fs/ext4/ext4.h`` (Linux v7.3-rc5); ``--check-schema`` cross-checks
every matrix row against that tree.

Row schema (module-level ``FEATURES`` entries).  Required fields:

``name``
    e2fsprogs/mke2fs feature name.  ``uninit_bg`` and ``gdt_csum`` are two
    names for the same RO_COMPAT bit; ``dax`` has no ext4 feature bit.
``mask``
    Feature bit value from ``linux/fs/ext4/ext4.h`` (0 for ``dax``).
``class``
    Superblock word: ``compat``, ``incompat``, ``ro_compat``, or ``none``
    when the feature has no on-disk feature bit.
``scope``
    Primary on-disk object affected: ``superblock``, ``group-descriptor``,
    ``inode``, ``extent``, ``directory``, ``metadata`` (checksums on every
    metadata object), ``journal``, ``allocation``, ``file-data``, or
    ``vfs-api`` (no on-disk object; mount option / per-call API only).
``status``
    ``rw`` (read-write supported by this plan), ``reject`` (feature bit
    present -> mount rejected with -EOPNOTSUPP, volume left unmodified) or
    ``api-eopnotsupp`` (accepted at mount; related API calls return
    -EOPNOTSUPP).
``reader_tests``
    Plan test files covering the feature on read/mount paths, including
    mount-rejection assertions for unsupported features.  ``[]`` when the
    plan has no coverage.
``writer_tests``
    Plan test files covering the feature on write/image-generation paths.
    ``[]`` when the plan has no coverage.

Optional fields: ``note`` (required for non-``rw`` rows; records the trigger
error) and ``alias_of`` (name of the canonical row sharing the same bit).

Usage:

    python3 tools/tests/ext4_feature_matrix.py --check-schema

``--check-schema`` validates the row schema and masks against the Linux
reference tree, then builds images through ``ext4_fixture.create_image`` and
verifies their superblock feature words equal the requested mke2fs features
(no host-default bleed-through).
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
import tempfile
from pathlib import Path

try:
    import ext4_fixture
except ModuleNotFoundError:  # imported as a module from outside tools/tests
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import ext4_fixture

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LINUX_ROOT = REPO_ROOT / "linux"

REQUIRED_FIELDS = (
    "name",
    "mask",
    "class",
    "scope",
    "status",
    "reader_tests",
    "writer_tests",
)
FEATURE_CLASSES = ("compat", "incompat", "ro_compat", "none")
FEATURE_SCOPES = (
    "superblock",
    "group-descriptor",
    "inode",
    "extent",
    "directory",
    "metadata",
    "journal",
    "allocation",
    "file-data",
    "vfs-api",
)
FEATURE_STATUSES = ("rw", "reject", "api-eopnotsupp")

# Status lists fixed verbatim by the plan (task 1, step 2).
RW_FEATURES = (
    "extents",
    "64bit",
    "flex_bg",
    "sparse_super",
    "sparse_super2",
    "uninit_bg",
    "metadata_csum",
    "gdt_csum",
    "large_file",
    "huge_file",
    "extra_isize",
    "dir_nlink",
    "dir_index",
    "has_journal",
)
UNSUPPORTED_FEATURES = (
    "bigalloc",
    "inline_data",
    "casefold",
    "encrypt",
    "verity",
    "quota",
    "project",
    "fast_commit",
    "mmp",
    "dax",
)

# e2fsprogs names whose Linux constant spells the feature differently.
LINUX_CONSTANT_ALIASES = {"uninit_bg": "gdt_csum"}

_ERRNO_RE = re.compile(r"-E[A-Z]+")
_FEATURE_DEFINE_RE = re.compile(
    r"^\s*#define\s+(EXT4_FEATURE_(?:COMPAT|RO_COMPAT|INCOMPAT)_[A-Z0-9_]+)"
    r"\s+(0[xX][0-9A-Fa-f]+|[0-9]+)\b"
)
_STRUCT_RE = re.compile(r"^\s*struct\s+(ext4_[A-Za-z0-9_]+)\s*\{")

FEATURES = [
    # -- rw: supported read-write by this plan ------------------------------
    {
        "name": "extents",
        "mask": 0x0040,
        "class": "incompat",
        "scope": "extent",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_extent_test.c", "tools/test_ext4_read.py"],
        "writer_tests": [
            "tools/tests/ext4_extent_test.c",
            "tools/tests/ext4_write_test.c",
            "tools/test_ext4_write.py",
            "tools/tests/ext4_format_test.py",
        ],
    },
    {
        "name": "64bit",
        "mask": 0x0080,
        "class": "incompat",
        "scope": "group-descriptor",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c", "tools/tests/ext4_mount_test.c"],
        "writer_tests": ["tools/tests/ext4_alloc_test.c", "tools/tests/ext4_format_test.py"],
    },
    {
        "name": "flex_bg",
        "mask": 0x0200,
        "class": "incompat",
        "scope": "group-descriptor",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c", "tools/tests/ext4_mount_test.c"],
        "writer_tests": ["tools/tests/ext4_alloc_test.c", "tools/tests/ext4_format_test.py"],
    },
    {
        "name": "sparse_super",
        "mask": 0x0001,
        "class": "ro_compat",
        "scope": "superblock",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c"],
        "writer_tests": [],
    },
    {
        "name": "sparse_super2",
        "mask": 0x0200,
        "class": "compat",
        "scope": "superblock",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c"],
        "writer_tests": [],
    },
    {
        "name": "uninit_bg",
        "mask": 0x0010,
        "class": "ro_compat",
        "scope": "group-descriptor",
        "status": "rw",
        "alias_of": "gdt_csum",
        "note": "e2fsprogs alias of gdt_csum; same RO_COMPAT bit 0x0010",
        "reader_tests": ["tools/tests/ext4_format_test.c", "tools/tests/ext4_checksum_test.c"],
        "writer_tests": ["tools/tests/ext4_alloc_test.c"],
    },
    {
        "name": "metadata_csum",
        "mask": 0x0400,
        "class": "ro_compat",
        "scope": "metadata",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_checksum_test.c", "tools/tests/ext4_mount_test.c"],
        "writer_tests": ["tools/tests/ext4_checksum_test.c", "tools/tests/ext4_format_test.py"],
    },
    {
        "name": "gdt_csum",
        "mask": 0x0010,
        "class": "ro_compat",
        "scope": "group-descriptor",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c", "tools/tests/ext4_checksum_test.c"],
        "writer_tests": ["tools/tests/ext4_checksum_test.c", "tools/tests/ext4_alloc_test.c"],
    },
    {
        "name": "large_file",
        "mask": 0x0002,
        "class": "ro_compat",
        "scope": "inode",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c", "tools/test_ext4_read.py"],
        "writer_tests": [
            "tools/tests/ext4_write_test.c",
            "tools/test_ext4_write.py",
            "tools/tests/ext4_format_test.py",
        ],
    },
    {
        "name": "huge_file",
        "mask": 0x0008,
        "class": "ro_compat",
        "scope": "inode",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c"],
        "writer_tests": [
            "tools/tests/ext4_write_test.c",
            "tools/tests/ext4_alloc_test.c",
            "tools/tests/ext4_format_test.py",
        ],
    },
    {
        "name": "extra_isize",
        "mask": 0x0040,
        "class": "ro_compat",
        "scope": "inode",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_format_test.c"],
        "writer_tests": ["tools/tests/ext4_write_test.c", "tools/tests/ext4_format_test.py"],
    },
    {
        "name": "dir_nlink",
        "mask": 0x0020,
        "class": "ro_compat",
        "scope": "directory",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_dir_test.c"],
        "writer_tests": ["tools/tests/ext4_dir_test.c"],
    },
    {
        "name": "dir_index",
        "mask": 0x0020,
        "class": "compat",
        "scope": "directory",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_dir_test.c", "tools/test_ext4_dir.py"],
        "writer_tests": [
            "tools/tests/ext4_dir_test.c",
            "tools/test_ext4_dir.py",
            "tools/tests/ext4_format_test.py",
        ],
    },
    {
        "name": "has_journal",
        "mask": 0x0004,
        "class": "compat",
        "scope": "journal",
        "status": "rw",
        "reader_tests": ["tools/tests/ext4_journal_test.c", "tools/tests/ext4_mount_test.c"],
        "writer_tests": [
            "tools/tests/ext4_journal_test.c",
            "tools/tests/ext4_crash_replay_test.py",
            "tools/tests/ext4_format_test.py",
        ],
    },
    # -- reject / api-eopnotsupp: not supported by this plan ----------------
    {
        "name": "bigalloc",
        "mask": 0x0200,
        "class": "ro_compat",
        "scope": "allocation",
        "status": "reject",
        "note": "mount rejected: -EOPNOTSUPP (cluster-based allocation unimplemented)",
        "reader_tests": ["tools/tests/ext4_mount_test.c"],
        "writer_tests": [],
    },
    {
        "name": "inline_data",
        "mask": 0x8000,
        "class": "incompat",
        "scope": "inode",
        "status": "reject",
        "note": "mount rejected: -EOPNOTSUPP (data stored in inode unimplemented)",
        "reader_tests": ["tools/tests/ext4_mount_test.c"],
        "writer_tests": [],
    },
    {
        "name": "casefold",
        "mask": 0x20000,
        "class": "incompat",
        "scope": "directory",
        "status": "reject",
        "note": "mount rejected: -EOPNOTSUPP (casefolded directory lookup unimplemented)",
        "reader_tests": ["tools/tests/ext4_mount_test.c"],
        "writer_tests": [],
    },
    {
        "name": "encrypt",
        "mask": 0x10000,
        "class": "incompat",
        "scope": "file-data",
        "status": "reject",
        "note": (
            "mount rejected: -EOPNOTSUPP (fscrypt unimplemented); "
            "FS_IOC_SET_ENCRYPTION_POLICY: -EOPNOTSUPP"
        ),
        "reader_tests": ["tools/tests/ext4_mount_test.c", "tools/tests/ext4_metadata_api_test.py"],
        "writer_tests": [],
    },
    {
        "name": "verity",
        "mask": 0x8000,
        "class": "ro_compat",
        "scope": "file-data",
        "status": "reject",
        "note": (
            "mount rejected: -EOPNOTSUPP (fs-verity verification unimplemented); "
            "FS_IOC_ENABLE_VERITY: -EOPNOTSUPP"
        ),
        "reader_tests": ["tools/tests/ext4_mount_test.c", "tools/tests/ext4_metadata_api_test.py"],
        "writer_tests": [],
    },
    {
        "name": "quota",
        "mask": 0x0100,
        "class": "ro_compat",
        "scope": "allocation",
        "status": "reject",
        "note": (
            "mount rejected: -EOPNOTSUPP (quota accounting unimplemented); "
            "quotactl: -EOPNOTSUPP"
        ),
        "reader_tests": ["tools/tests/ext4_mount_test.c", "tools/tests/ext4_metadata_api_test.py"],
        "writer_tests": [],
    },
    {
        "name": "project",
        "mask": 0x2000,
        "class": "ro_compat",
        "scope": "allocation",
        "status": "reject",
        "note": (
            "mount rejected: -EOPNOTSUPP (project quota unimplemented); "
            "FS_IOC_FSSETXATTR project id: -EOPNOTSUPP"
        ),
        "reader_tests": ["tools/tests/ext4_mount_test.c", "tools/tests/ext4_metadata_api_test.py"],
        "writer_tests": [],
    },
    {
        "name": "fast_commit",
        "mask": 0x0400,
        "class": "compat",
        "scope": "journal",
        "status": "reject",
        "note": "mount rejected: -EOPNOTSUPP (journal fast-commit region unimplemented)",
        "reader_tests": ["tools/tests/ext4_journal_test.c"],
        "writer_tests": [],
    },
    {
        "name": "mmp",
        "mask": 0x0100,
        "class": "incompat",
        "scope": "superblock",
        "status": "reject",
        "note": "mount rejected: -EOPNOTSUPP (MMP block monitoring unimplemented)",
        "reader_tests": ["tools/tests/ext4_mount_test.c"],
        "writer_tests": [],
    },
    {
        "name": "dax",
        "mask": 0,
        "class": "none",
        "scope": "vfs-api",
        "status": "api-eopnotsupp",
        "note": (
            "no ext4 feature bit (mount option / FS_XFLAG_DAX); "
            "mount -o dax and FS_IOC_FSSETXATTR DAX flag: -EOPNOTSUPP"
        ),
        "reader_tests": ["tools/tests/ext4_metadata_api_test.py"],
        "writer_tests": [],
    },
]

# s_feature_compat/s_feature_incompat/s_feature_ro_compat: three little-endian
# u32 words at superblock file offsets 1024+0x5C/0x60/0x64 (ext4.h layout).
_SUPERBLOCK_FEATURE_OFFSET = 1024 + 0x5C
_SUPERBLOCK_FEATURE_WORDS = ("compat", "incompat", "ro_compat")

# Linux treats METADATA_CSUM and GDT_CSUM as mutually exclusive (comment above
# EXT4_FEATURE_RO_COMPAT_METADATA_CSUM in fs/ext4/ext4.h).  mke2fs resolves a
# request naming both by keeping metadata_csum and dropping the shared
# gdt_csum/uninit_bg bit; this documented resolution is the only feature-set
# difference the image regression tolerates.
MKE2FS_FEATURE_RESOLUTIONS = {"metadata_csum": ("gdt_csum", "uninit_bg")}

# mke2fs -O names for the full-list image regression: every plan rw feature
# except the gdt_csum alias (mke2fs only accepts the uninit_bg spelling for
# the shared RO_COMPAT bit).
MKE2FS_IMAGE_FEATURES = tuple(name for name in RW_FEATURES if name != "gdt_csum")


def load_linux_reference(root: Path) -> dict:
    """Read the Linux reference tree and extract its ext4 feature constants.

    Sources consulted: ``fs/ext4/ext4.h`` (feature mask constants and on-disk
    struct inventory), ``fs/ext4/super.c``, ``include/uapi/linux/ext4.h`` when
    present, and every file under ``Documentation/filesystems/ext4``.

    Returns a dict with ``root``, ``sources`` (paths actually read),
    ``feature_constants`` (``EXT4_FEATURE_*`` name -> mask), ``objects``
    (on-disk ``struct ext4_*`` names) and ``doc_topics`` (documentation file
    names).
    """
    base = Path(root)
    ext4_header = base / "fs/ext4/ext4.h"
    if not ext4_header.is_file():
        raise FileNotFoundError(f"linux reference header not found: {ext4_header}")

    sources: list[Path] = []
    feature_constants: dict[str, int] = {}
    objects: set[str] = set()
    for line in ext4_header.read_text(encoding="utf-8", errors="replace").splitlines():
        define = _FEATURE_DEFINE_RE.match(line)
        if define:
            feature_constants[define.group(1)] = int(define.group(2), 0)
            continue
        struct = _STRUCT_RE.match(line)
        if struct:
            objects.add(struct.group(1))
    sources.append(ext4_header)

    for relative in ("fs/ext4/super.c", "include/uapi/linux/ext4.h"):
        candidate = base / relative
        if candidate.is_file():
            candidate.read_text(encoding="utf-8", errors="replace")
            sources.append(candidate)

    doc_topics: list[str] = []
    doc_dir = base / "Documentation/filesystems/ext4"
    if doc_dir.is_dir():
        for entry in sorted(doc_dir.iterdir()):
            if entry.is_file():
                entry.read_text(encoding="utf-8", errors="replace")
                sources.append(entry)
                doc_topics.append(entry.name)

    return {
        "root": base,
        "sources": sources,
        "feature_constants": dict(sorted(feature_constants.items())),
        "objects": sorted(objects),
        "doc_topics": doc_topics,
    }


def _linux_constant_name(entry: dict) -> str | None:
    """Linux constant name for a matrix row, or None when it has no bit."""
    if entry["class"] == "none":
        return None
    feature = LINUX_CONSTANT_ALIASES.get(entry["name"], entry["name"])
    return f"EXT4_FEATURE_{entry['class'].upper()}_{feature.upper()}"


def check_schema(features: list, reference: dict) -> list[str]:
    """Validate matrix rows; return a list of error strings (empty when good).

    Checks field completeness and value domains, the plan's verbatim rw /
    reject status lists, alias mask consistency, and that every mask matches
    the constant parsed from the Linux reference tree.
    """
    errors: list[str] = []
    constants = reference["feature_constants"]
    if not constants:
        errors.append("linux reference has no EXT4_FEATURE_* constants")
    by_name: dict[str, dict] = {}

    for index, entry in enumerate(features):
        where = f"FEATURES[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{where}: entry must be a dict")
            continue
        missing = [field for field in REQUIRED_FIELDS if field not in entry]
        if missing:
            errors.append(f"{where}: missing fields {missing}")
            continue
        name = entry["name"]
        where = f"FEATURES[{index}] ({name})"
        if not isinstance(name, str) or not name:
            errors.append(f"{where}: name must be a non-empty string")
            continue
        if name in by_name:
            errors.append(f"{where}: duplicate feature name")
            continue
        by_name[name] = entry

        if not isinstance(entry["mask"], int) or isinstance(entry["mask"], bool) or entry["mask"] < 0:
            errors.append(f"{where}: mask must be a non-negative int")
        if entry["class"] not in FEATURE_CLASSES:
            errors.append(f"{where}: class must be one of {FEATURE_CLASSES}")
        if entry["scope"] not in FEATURE_SCOPES:
            errors.append(f"{where}: scope must be one of {FEATURE_SCOPES}")
        if entry["status"] not in FEATURE_STATUSES:
            errors.append(f"{where}: status must be one of {FEATURE_STATUSES}")
        for field in ("reader_tests", "writer_tests"):
            tests = entry[field]
            if not isinstance(tests, list) or any(not isinstance(item, str) or not item for item in tests):
                errors.append(f"{where}: {field} must be a list of non-empty strings")
        if entry["status"] != "rw":
            note = entry.get("note")
            if not isinstance(note, str) or not note.strip():
                errors.append(f"{where}: non-rw rows must note the trigger error")
            elif not _ERRNO_RE.search(note):
                errors.append(f"{where}: note must name the triggered errno")

        if entry["class"] == "none":
            if entry["mask"] != 0:
                errors.append(f"{where}: class 'none' rows must have mask 0")
        elif entry["class"] in ("compat", "incompat", "ro_compat"):
            constant = _linux_constant_name(entry)
            if constant not in constants:
                errors.append(f"{where}: {constant} not found in linux reference")
            elif entry["mask"] != constants[constant]:
                errors.append(
                    f"{where}: mask {entry['mask']:#x} != {constant} "
                    f"{constants[constant]:#x} in linux reference"
                )

    planned = set(RW_FEATURES) | set(UNSUPPORTED_FEATURES)
    for name in RW_FEATURES:
        entry = by_name.get(name)
        if entry is None:
            errors.append(f"plan rw feature missing from matrix: {name}")
        elif entry["status"] != "rw":
            errors.append(f"{name}: plan requires status rw, got {entry['status']}")
    for name in UNSUPPORTED_FEATURES:
        entry = by_name.get(name)
        if entry is None:
            errors.append(f"plan unsupported feature missing from matrix: {name}")
        elif entry["status"] not in ("reject", "api-eopnotsupp"):
            errors.append(f"{name}: plan requires reject/api-eopnotsupp, got {entry['status']}")
    for name in sorted(set(by_name) - planned):
        errors.append(f"{name}: not named by the plan status lists")

    for name, entry in by_name.items():
        alias_of = entry.get("alias_of")
        if alias_of is None:
            continue
        target = by_name.get(alias_of)
        if target is None:
            errors.append(f"{name}: alias_of {alias_of} is not a matrix row")
            continue
        if entry["mask"] != target["mask"] or entry["class"] != target["class"]:
            errors.append(f"{name}: alias mask/class differs from {alias_of}")

    groups: dict[tuple, list[dict]] = {}
    for entry in by_name.values():
        if entry["class"] != "none":
            groups.setdefault((entry["class"], entry["mask"]), []).append(entry)
    for (entry_class, mask), members in groups.items():
        if len(members) < 2:
            continue
        canonical = [m for m in members if "alias_of" not in m]
        if len(canonical) != 1:
            errors.append(
                f"{entry_class} mask {mask:#x}: shared by "
                f"{sorted(m['name'] for m in members)} without a single canonical row"
            )
            continue
        for member in members:
            if "alias_of" in member and member["alias_of"] != canonical[0]["name"]:
                errors.append(f"{member['name']}: must alias {canonical[0]['name']}")

    return errors


def read_superblock_feature_masks(image: Path) -> dict[str, int]:
    """Parse the three superblock feature words of *image*.

    Reads s_feature_compat/s_feature_incompat/s_feature_ro_compat straight
    from the file offsets 1024+0x5C/0x60/0x64 (little-endian u32); no tune2fs
    needed.
    """
    with Path(image).open("rb") as handle:
        handle.seek(_SUPERBLOCK_FEATURE_OFFSET)
        raw = handle.read(12)
    if len(raw) != 12:
        raise ValueError(f"{image}: superblock feature words are truncated")
    compat, incompat, ro_compat = struct.unpack("<III", raw)
    return {"compat": compat, "incompat": incompat, "ro_compat": ro_compat}


def _feature_name_for_bit(word: str, bit: int) -> str:
    for entry in FEATURES:
        if entry["class"] == word and entry["mask"] == bit:
            return entry["name"]
    return f"{word} bit {bit:#x}"


def check_image_feature_masks(features: list[str], image: Path) -> list[str]:
    """Compare *image*'s superblock feature words against the requested set.

    Every set bit must belong to a requested feature (modulo documented mke2fs
    resolutions) and every requested bit must be present, so host mke2fs
    defaults can never leak into a generated image.  Returns error strings.
    """
    errors: list[str] = []
    by_name = {entry["name"]: entry for entry in FEATURES}
    requested = dict.fromkeys(_SUPERBLOCK_FEATURE_WORDS, 0)
    for name in features:
        entry = by_name.get(name)
        if entry is None:
            errors.append(f"{image.name}: requested feature {name!r} is not a matrix row")
            continue
        if entry["class"] == "none":
            errors.append(f"{image.name}: feature {name} has no on-disk feature bit")
            continue
        requested[entry["class"]] |= entry["mask"]

    resolved = dict.fromkeys(_SUPERBLOCK_FEATURE_WORDS, 0)
    for name in features:
        for loser in MKE2FS_FEATURE_RESOLUTIONS.get(name, ()):
            if loser in features:
                entry = by_name[loser]
                resolved[entry["class"]] |= entry["mask"]

    actual = read_superblock_feature_masks(image)
    for word in _SUPERBLOCK_FEATURE_WORDS:
        extra = actual[word] & ~requested[word]
        if extra:
            names = [
                _feature_name_for_bit(word, 1 << bit)
                for bit in range(32)
                if extra & (1 << bit)
            ]
            errors.append(
                f"{image.name}: unexpected {word} bits {extra:#x} "
                f"({', '.join(names)}) outside the requested feature set"
            )
        missing = requested[word] & ~resolved[word] & ~actual[word]
        if missing:
            errors.append(
                f"{image.name}: requested {word} bits {missing:#x} missing from image"
            )
    return errors


def check_create_image_regression() -> list[str]:
    """Regression for ext4_fixture.create_image's ``-O none,<features>`` pinning.

    Builds a single-feature image (extents only: metadata_csum, 64bit, flex_bg
    and every other host default must not appear) and a full-list image (every
    plan rw bit: the three superblock words must equal the requested set,
    modulo documented mke2fs resolutions).
    """
    errors: list[str] = []
    with tempfile.TemporaryDirectory(prefix="ext4-feature-matrix-") as work:
        tmp = Path(work)
        isolation = tmp / "isolation.ext4"
        ext4_fixture.create_image(isolation, 64, ["extents"])
        errors.extend(check_image_feature_masks(["extents"], isolation))
        full = tmp / "full.ext4"
        ext4_fixture.create_image(full, 64, list(MKE2FS_IMAGE_FEATURES))
        errors.extend(check_image_feature_masks(list(MKE2FS_IMAGE_FEATURES), full))
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate the ReliefOS ext4 feature support matrix."
    )
    parser.add_argument(
        "--check-schema",
        action="store_true",
        help="validate FEATURES fields and masks against the Linux reference tree",
    )
    parser.add_argument(
        "--linux-root",
        type=Path,
        default=DEFAULT_LINUX_ROOT,
        help="Linux reference tree root (default: <repo>/linux)",
    )
    args = parser.parse_args(argv)
    if not args.check_schema:
        parser.error("--check-schema is required")

    try:
        reference = load_linux_reference(args.linux_root)
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    errors = check_schema(FEATURES, reference)
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    try:
        image_errors = check_create_image_regression()
    except (OSError, ValueError) as error:
        print(f"FAIL: create_image regression could not run: {error}", file=sys.stderr)
        return 1
    if image_errors:
        for error in image_errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print(
        f"PASS: {len(FEATURES)} feature rows; masks match "
        f"{reference['root'] / 'fs/ext4/ext4.h'} "
        f"({len(reference['feature_constants'])} EXT4_FEATURE_* constants); "
        f"create_image feature isolation verified (2 images)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
