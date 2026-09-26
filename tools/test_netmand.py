#!/usr/bin/env python3
"""Exercise the production network adapter and its error propagation on host Linux."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="leonos-netmand-") as temporary:
    work = Path(temporary)
    headers = work / "include/leonos"
    headers.mkdir(parents=True)
    for name in ("unix_ipc",):
        (headers / f"{name}.h").symlink_to(ROOT / f"userland/runtime/include/leonos/{name}.h")
    binary = work / "netmand-test"
    subprocess.run(["cc", "-std=c11", "-O1", "-g", "-D_GNU_SOURCE", "-DLEONOS_USE_MUSL",
                    "-fsanitize=address,undefined", "-ffunction-sections", "-fdata-sections",
                    "-Wl,--gc-sections", "-I" + str(work / "include"), "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi",
                    "tools/tests/netmand_client_test.c", "userland/runtime/src/unix_ipc.c",
                    "-o", str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True, timeout=20)
    compiler = ROOT / "build/musl-gcc/root/opt/dyne/gcc-musl/bin/x86_64-linux-musl-gcc"
    binary = work / "netctl-test"
    subprocess.run([str(compiler), "-static", "-O1", "-g", "-D_GNU_SOURCE", "-DLEONOS_USE_MUSL",
                    "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-Ikernel/ntclks/include", "-Iinclude", "-Ikernel/ntclks/include/uapi",
                    "-idirafter", "userland/runtime/include", "tools/tests/netctl_dhcp_test.c", "-o", str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True, timeout=20)
