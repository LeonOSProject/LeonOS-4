#!/usr/bin/env python3
"""Exercise the real tmpfs backend with ASan/UBSan and bounded RAM fixtures."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
out = ROOT / "build/tmpfs-host"
out.mkdir(parents=True, exist_ok=True)
subprocess.run(["clang", "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi", "-Ikernel/ntclks/kernel/ntclks/include",
                "tools/tests/tmpfs_test.c", "kernel/ntclks/kernel/ntclks/tmpfs.c", "-o", str(out / "tmpfs-test")],
               cwd=ROOT, check=True)
subprocess.run([str(out / "tmpfs-test")], cwd=ROOT, check=True)
subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-DTMPFS_MMAP_STANDALONE",
                "tools/tests/tmpfs_mmap_test.c", "-o", str(out / "mmap-reference")],
               cwd=ROOT, check=True)
subprocess.run([str(out / "mmap-reference")], cwd=ROOT, check=True, timeout=30)
