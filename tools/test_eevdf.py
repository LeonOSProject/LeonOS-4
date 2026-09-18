#!/usr/bin/env python3
"""Deterministic EEVDF tests using the kernel's actual integer policy."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="leonos-eevdf-") as tmp:
    for name in ("eevdf", "eevdf_scheduler"):
        binary = str(Path(tmp) / name)
        subprocess.run(["cc", "-std=c11", "-O2", "-g", "-Wall", "-Wextra",
                    "-fsanitize=address,undefined", "-fno-pie", "-no-pie",
                    "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                    "-Iinclude", "-Iinclude/uapi", "-Ikernel/ntclks/include",
                    f"tools/tests/{name}_test.c", "-o", binary],
                   cwd=ROOT, check=True)
        subprocess.run([binary], check=True, timeout=30)
