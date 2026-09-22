#!/usr/bin/env python3
"""Run the built Linux ncurses binaries on the host and check their runtime data.

Vim used to be validated here as well; it is a signed upstream Alpine package
now, so only the LeonOS ncurses build keeps a host-side runtime check.
"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--musl", required=True, type=Path)
    parser.add_argument("--ncurses", required=True, type=Path)
    args = parser.parse_args()
    musl, ncurses = args.musl.resolve(), args.ncurses.resolve()
    database = ncurses / "share/terminfo"
    env = {**os.environ, "TERM": "xterm-256color", "TERMINFO": str(database),
           "TERMINFO_DIRS": str(database)}
    clear = ncurses / "bin/clear"
    infocmp = ncurses / "bin/infocmp"
    for binary in (clear, infocmp, ncurses / "bin/tput"):
        headers = subprocess.check_output(["readelf", "-l", "-d", str(binary)], text=True)
        assert "INTERP" not in headers and "(NEEDED)" not in headers, binary
    terminfo = subprocess.check_output([str(infocmp), "xterm-256color"], env=env)
    assert b"colors#0x100" in terminfo or b"colors#256" in terminfo
    cleared = subprocess.check_output([str(clear)], env=env)
    assert cleared.startswith(b"\x1b[")

    # LeonOS must retain its native terminal types even if the external
    # database is missing or damaged; this is the failure mode from issue #27.
    fallback_env = {**env, "TERMINFO": "/nonexistent/terminfo",
                    "TERMINFO_DIRS": "/nonexistent/terminfo"}
    fallback_env["TERM"] = "xterm"
    fallback_clear = subprocess.check_output([str(clear)], env=fallback_env)
    assert fallback_clear.startswith(b"\x1b[")
    fallback_columns = subprocess.check_output(
        [str(ncurses / "bin/tput"), "cols"], env=fallback_env)
    assert int(fallback_columns) > 0
    with tempfile.TemporaryDirectory(prefix="leonos-terminal-test-") as directory:
        work = Path(directory)
        probe = work / "ncurses-test"
        subprocess.run([
            "clang", "--target=x86_64-linux-musl", "--gcc-toolchain=/nonexistent",
            f"--sysroot={musl}", "--rtlib=compiler-rt", "--unwindlib=none",
            "-fuse-ld=lld", "-static",
            "-I" + str(ncurses / "include"), str(ROOT / "tools/tests/ncurses_runtime_test.c"),
            "-L" + str(ncurses / "lib"), "-lncursesw", "-ltinfow", "-o", str(probe)],
            check=True)
        subprocess.run([str(probe)], env=env, check=True, timeout=30)
    print("PASS Linux host: static ncurses runtime data and embedded fallback terminfo")


if __name__ == "__main__":
    main()
