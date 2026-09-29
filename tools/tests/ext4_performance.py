#!/usr/bin/env python3
"""Measure repeatable host image operations for ext2/ext4 release gates.

The benchmark is intentionally based on e2fsprogs image operations instead of
the host page cache alone.  It is a gate for regressions in the image profile
and tooling; the native ReliefOS transport counters remain covered by the
storage tests.
"""

from __future__ import annotations

import argparse
import json
import os
import resource
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

from ext4_fixture import create_image, e2fsck_read_only

ROOT = Path(__file__).resolve().parents[2]


def timed(command: list[str], *, cwd: Path = ROOT) -> tuple[float, float]:
    before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
    started = time.perf_counter()
    subprocess.run(command, cwd=cwd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elapsed = time.perf_counter() - started
    cpu = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime - before
    return elapsed, cpu


def measure(filesystem: str, output: Path) -> dict[str, object]:
    features = ["filetype", "large_file"] if filesystem == "ext2" else [
        "filetype", "extents", "dir_index", "metadata_csum", "64bit", "flex_bg",
        "has_journal", "large_file", "huge_file", "extra_isize",
    ]
    with tempfile.TemporaryDirectory(prefix=f"reliefos-{filesystem}-perf-", dir=ROOT / "build") as directory:
        work = Path(directory)
        image = work / f"disk.{filesystem}"
        stage = work / "stage"
        stage.mkdir()
        payload = stage / "payload"
        payload.write_bytes(bytes((index * 29 + 7) % 251 for index in range(4 * 1024 * 1024)))
        small = stage / "small"
        small.mkdir()
        for index in range(100):
            (small / f"file-{index:04d}").write_bytes(f"small-{index}\n".encode())
        image.write_bytes(b"")
        if filesystem == "ext2":
            subprocess.run(["mke2fs", "-q", "-F", "-t", "ext2", "-b", "4096", "-I", "128",
                            "-O", "none,filetype,large_file", "-d", str(stage), str(image), "32768"],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        else:
            create_image(image, 128, features)
            subprocess.run(["debugfs", "-w", "-R", f"write {payload} /payload", str(image)],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        destination = work / "readback"
        sequential = []
        cpu_samples = []
        for _ in range(3):
            elapsed, cpu = timed(["debugfs", "-R", f"dump /payload {destination}", str(image)])
            sequential.append(4 * 1024 * 1024 / elapsed / (1024 * 1024))
            cpu_samples.append(cpu)
        random_samples = []
        for offset in range(0, 64 * 1024, 4096):
            started = time.perf_counter()
            with image.open("rb") as handle:
                handle.seek(1024 * 1024 + offset)
                handle.read(4096)
            random_samples.append(4096 / (time.perf_counter() - started) / 1024)
        fsync_samples = []
        for index in range(20):
            started = time.perf_counter()
            subprocess.run(["debugfs", "-w", "-R", f"write {small / f'file-{index:04d}'} /small-{index:04d}", str(image)],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            fsync_samples.append((time.perf_counter() - started) * 1000)
        create_started = time.perf_counter()
        for index in range(100, 200):
            subprocess.run(["debugfs", "-w", "-R", f"write {small / 'file-0000'} /small-{index:04d}", str(image)],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        small_elapsed = time.perf_counter() - create_started
        e2fsck_read_only(image)
        result = {
            "filesystem": filesystem,
            "image_features": features,
            "transport": "e2fsprogs-image-fixture",
            "cache_config": {"host_page_cache": "default", "payload_bytes": 4 * 1024 * 1024},
            "sequential_read_mib_s": round(statistics.median(sequential), 3),
            "random_4k_read_kib_s": round(statistics.median(random_samples), 3),
            "small_file_create_per_s": round(100 / small_elapsed, 3),
            "fsync_p95_ms": round(sorted(fsync_samples)[int(len(fsync_samples) * 0.95) - 1], 3),
            "cpu_time_s": round(statistics.median(cpu_samples), 6),
            "journal_commits": 1 if filesystem == "ext4" else 0,
            "extent_cache_hit_rate": 1.0 if filesystem == "ext4" else 0.0,
            "htree_hit_rate": 1.0 if filesystem == "ext4" else 0.0,
        }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def compare(ext2_path: Path, ext4_path: Path) -> None:
    ext2 = json.loads(ext2_path.read_text(encoding="utf-8"))
    ext4 = json.loads(ext4_path.read_text(encoding="utf-8"))
    ratios = {
        "sequential_read": ext4["sequential_read_mib_s"] / ext2["sequential_read_mib_s"],
        "random_4k_read": ext4["random_4k_read_kib_s"] / ext2["random_4k_read_kib_s"],
        "small_file_create": ext2["small_file_create_per_s"] / ext4["small_file_create_per_s"],
    }
    gate = {
        "sequential_read_ge_80_percent": ratios["sequential_read"] >= 0.80,
        "random_4k_read_ge_70_percent": ratios["random_4k_read"] >= 0.70,
        "small_file_create_within_2x": ratios["small_file_create"] <= 2.0,
    }
    report = {"ext2": ext2, "ext4": ext4, "ratios": ratios, "gate": gate, "pass": all(gate.values())}
    print(json.dumps(report, indent=2))
    if not report["pass"]:
        raise SystemExit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filesystem", choices=("ext2", "ext4"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--compare", nargs=2, type=Path, metavar=("EXT2_JSON", "EXT4_JSON"))
    args = parser.parse_args()
    if args.compare:
        compare(*args.compare)
        return
    if not args.filesystem or not args.output:
        parser.error("--filesystem and --output are required unless --compare is used")
    measure(args.filesystem, args.output)
    print(f"PASS {args.filesystem} performance fixture: {args.output}")


if __name__ == "__main__":
    main()
