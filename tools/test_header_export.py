#!/usr/bin/env python3
"""Verify the headers_install export boundary.

Three properties, all against the checked-in whitelist
(configs/header-export.list — the single source of truth):

1. exact set: the install emits exactly the whitelisted files, no extras
   (an extra file is a private-header leak) and no missing files;
2. self-contained: every installed header compiles standalone as C and as
   C++ with ONLY the export directory on the include path, so nothing may
   depend on kernel-private trees or unexported headers;
3. no residue: re-installing after a whitelist entry is removed deletes
   the stale installed copy.

Run `make O=<dir> headers_install` first or let this test do it.
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WHITELIST = ROOT / "configs/header-export.list"

PRIVATE_INCLUDE = re.compile(r'#\s*include\s*[<"](?:ntclks/|\.\./)')


def whitelist_entries(path=WHITELIST):
    entries = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        entries.append(line)
    return entries


def install_relative(entry):
    """include/uapi/leonos/x.h -> leonos/x.h (path inside the export include dir)."""
    prefix = "include/uapi/"
    if not entry.startswith(prefix):
        raise SystemExit(f"{entry}: not under {prefix}")
    return entry[len(prefix):]


def run_install(outdir, whitelist=WHITELIST):
    return subprocess.run(["make", f"O={outdir}", "headers_install",
                           f"HEADER_EXPORT_LIST={whitelist}"],
                          cwd=ROOT, capture_output=True, text=True)


def installed_files(export_include):
    return {str(p.relative_to(export_include))
            for p in export_include.rglob("*") if p.is_file()}


def compile_standalone(export_include, relative, language):
    compiler = "clang" if language == "c" else "clang++"
    result = subprocess.run(
        [compiler, "-x", language, "-fsyntax-only", "-Werror",
         "-I", str(export_include), str(export_include / relative)],
        capture_output=True, text=True)
    return result


def check_private_includes(export_include):
    problems = []
    for path in sorted(export_include.rglob("*.h")):
        text = path.read_text()
        for match in PRIVATE_INCLUDE.finditer(text):
            problems.append(f"{path.name}: private include {match.group(0).strip()}")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path,
                        help="existing headers_install output to check "
                             "(default: run the install into a temp dir)")
    args = parser.parse_args()

    entries = whitelist_entries()
    expected = {install_relative(entry) for entry in entries}

    with tempfile.TemporaryDirectory(prefix="leonos-hexport-") as directory:
        temp = Path(directory)
        if args.out:
            outdir = args.out
            export_include = outdir / "kernel-export" / "include"
        else:
            outdir = temp / "out"
            result = run_install(outdir)
            if result.returncode != 0:
                sys.stderr.write(result.stdout + result.stderr)
                raise SystemExit("make headers_install failed")
            export_include = outdir / "kernel-export" / "include"

        failures = []
        if not export_include.is_dir():
            raise SystemExit(f"no export directory at {export_include}")
        have = installed_files(export_include)
        for leak in sorted(have - expected):
            failures.append(f"PRIVATE LEAK: installed but not whitelisted: {leak}")
        for missing in sorted(expected - have):
            failures.append(f"MISSING: whitelisted but not installed: {missing}")

        for relative in sorted(expected & have):
            for language in ("c", "c++"):
                result = compile_standalone(export_include, relative, language)
                if result.returncode != 0:
                    first = (result.stderr.strip().splitlines() or [""])[0]
                    failures.append(
                        f"NOT SELF-CONTAINED ({language}): {relative}: {first}")

        failures.extend(check_private_includes(export_include))

        # 3. no residue: drop one entry and re-install into the same tree.
        if not args.out:
            dropped = entries[-1]
            reduced = temp / "reduced.list"
            reduced.write_text("\n".join(e for e in entries if e != dropped) + "\n")
            result = run_install(outdir, whitelist=reduced)
            if result.returncode != 0:
                sys.stderr.write(result.stdout + result.stderr)
                raise SystemExit("re-install with reduced list failed")
            residue = install_relative(dropped)
            if (export_include / residue).exists():
                failures.append(f"RESIDUE: {residue} survived its removal "
                                "from the whitelist")

        if failures:
            print("HEADER EXPORT BOUNDARY violations:")
            for line in failures:
                print("  " + line)
            raise SystemExit(1)
        print(f"PASS header export: {len(expected)} headers, exact whitelist, "
              "self-contained C/C++, no private includes, no residue")


if __name__ == "__main__":
    main()
