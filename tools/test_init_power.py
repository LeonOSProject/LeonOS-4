#!/usr/bin/env python3
"""Exercise GUI/installer power-to-PID-1 dispatch; never signal the host init."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="init-power-") as directory:
    binary = Path(directory) / "init-power"
    subprocess.run(["cc", "-std=gnu11", "-O1", "-g", "-ffunction-sections", "-fdata-sections",
                    "-Wl,--gc-sections", "-fsanitize=address,undefined",
                    "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi", "-idirafter", "userland/runtime/include",
                    "tools/tests/init_power_test.c", "-o", str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
