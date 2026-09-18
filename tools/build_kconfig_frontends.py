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


def patch_gperf_compatibility(work_dir: Path) -> None:
    """Adapt the pinned gperf input to current gperf's size_t signature.

    kconfig-frontends declares the generated lookup function with an
    ``unsigned int`` length, while modern gperf emits its definition with
    ``size_t``.  The declaration and definition must match under current
    host compilers.  Patch only the disposable build copy so the upstream
    submodule remains byte-for-byte unchanged.
    """
    path = work_dir / "libs/parser/hconf.gperf"
    text = path.read_text(encoding="utf-8")
    old = "static const struct kconf_id *kconf_id_lookup(register const char *str, register unsigned int len);"
    new = "static const struct kconf_id *kconf_id_lookup(register const char *str, register size_t len);"
    if old not in text:
        if new in text:
            return
        raise SystemExit(f"unexpected kconfig-frontends gperf input: {path}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


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
    patch_gperf_compatibility(work_dir)
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
