#!/usr/bin/env python3
"""Exercise actual descriptor helpers and scheduler table growth with sanitizers."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="leonos-descriptors-") as directory:
    binary = str(Path(directory) / "descriptors")
    subprocess.run([
        "clang", "-std=c11", "-g", "-O1", "-ffunction-sections", "-fdata-sections",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi", "-Ikernel/ntclks/kernel/ntclks/include", "-Wl,--gc-sections",
        "tools/tests/descriptor_table_test.c", "kernel/ntclks/kernel/ntclks/sched/sched.c",
        "kernel/ntclks/kernel/ntclks/wait.c", "kernel/ntclks/kernel/ntclks/syscall_sysv_msg.c",
        "kernel/ntclks/kernel/ntclks/syscall_sysv_sem.c", "kernel/ntclks/kernel/ntclks/syscall_locks.c", "-o", binary,
    ], cwd=ROOT, check=True)
    subprocess.run([binary], cwd=ROOT, check=True, timeout=20)
