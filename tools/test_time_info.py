#!/usr/bin/env python3
"""Run the actual SDK calendar conversion with controlled clock input."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = ROOT / "build/musl-gcc/root/opt/dyne/gcc-musl/bin/x86_64-linux-musl-gcc"
with tempfile.TemporaryDirectory(prefix="leonos-time-info-") as temporary:
    binary = Path(temporary) / "time-info"
    subprocess.run([str(compiler), "-static", "-O1", "-g", "-Wall", "-Wextra",
                    "-D_GNU_SOURCE", "-DLEONOS_USE_MUSL", "-ffunction-sections", "-fdata-sections",
                    "-Wl,--gc-sections", "-Iinclude", "-Iinclude/uapi",
                    "-idirafter", "userland/runtime/include", "tools/tests/time_info_test.c", "-o", str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
