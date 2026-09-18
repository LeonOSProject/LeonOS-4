#!/usr/bin/env python3
"""Build the repository-pinned kconfig-frontends mconf host tool."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def run(command: list[str], *, cwd: Path, environment: dict[str, str]) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=environment, check=True)


def require_source(source: Path) -> None:
    required = (
        source / "bootstrap",
        source / "configure.ac",
        source / "frontends/mconf/mconf.c",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SystemExit(
            "kconfig-frontends source is not initialized; run "
            "`git submodule update --init third_party/kconfig-frontends`"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, required=True)
    args = parser.parse_args()

    source = args.source.resolve()
    work_dir = args.work_dir.resolve()
    prefix = args.prefix.resolve()
    require_source(source)

    if work_dir.exists():
        shutil.rmtree(work_dir)
    if prefix.exists():
        shutil.rmtree(prefix)
    work_dir.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, work_dir, ignore=shutil.ignore_patterns(".git"))
    prefix.mkdir(parents=True, exist_ok=True)

    environment = os.environ.copy()
    environment.setdefault("LC_ALL", "C")
    run(["./bootstrap"], cwd=work_dir, environment=environment)
    run(
        [
            "./configure",
            f"--prefix={prefix}",
            "--enable-frontends=mconf",
            "--disable-utils",
            "--disable-L10n",
            "--disable-shared",
            "--enable-static",
            "--disable-werror",
        ],
        cwd=work_dir,
        environment=environment,
    )
    jobs = max(1, os.cpu_count() or 1)
    run(["make", f"-j{jobs}"], cwd=work_dir, environment=environment)
    run(["make", "install"], cwd=work_dir, environment=environment)

    executable = prefix / "bin/kconfig-mconf"
    if not executable.is_file():
        raise SystemExit(f"kconfig-frontends did not produce {executable}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
