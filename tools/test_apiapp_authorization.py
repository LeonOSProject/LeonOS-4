#!/usr/bin/env python3
"""Exercise API install control flow with real child processes and pipes."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = ROOT / "build/musl-gcc/root/opt/dyne/gcc-musl/bin/x86_64-linux-musl-gcc"
with tempfile.TemporaryDirectory(prefix="reliefos-apiapp-") as temporary:
    binary = Path(temporary) / "apiapp-authorization"
    subprocess.run([str(compiler), "-static", "-O1", "-g", "-Wall", "-Wextra",
                    "-DRELIEFOS_USE_MUSL", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                    "-Ikernel/reliefnt/include", "-Iinclude", "-Ikernel/reliefnt/include/uapi", "-idirafter", "userland/runtime/include",
                    "tools/tests/apiapp_authorization_test.c", "-o", str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True, timeout=15)
