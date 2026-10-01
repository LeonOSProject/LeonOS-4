#!/usr/bin/env python3
"""Verify the Xorg device probe sticks to the standard Linux fbdev/evdev ABI.

Four independent results are reported:

  source-check   static inspection of tools/tests/xorg_device_probe.c
  static-link    building the probe with a static C toolchain
  guest-run      executing the probe inside the target guest
  failure-errno  the errno diagnostics a guest run recorded

Only ``source-check`` runs with --source-only. The guest stages consume a
probe log produced inside the guest (``xorg-probe ...`` lines).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "tools/tests/xorg_device_probe.c"

ALLOWED_INCLUDES = {
    "errno.h", "fcntl.h", "linux/fb.h", "linux/input.h", "poll.h",
    "stddef.h", "stdio.h", "string.h", "sys/ioctl.h", "unistd.h",
}

REQUIRED_CODE_TOKENS = [
    "/dev/fb0",
    "/dev/input/event0",
    "/dev/input/event1",
    "FBIOGET_VSCREENINFO",
    "FBIOGET_FSCREENINFO",
    "EVIOCGNAME",
    "EVIOCGBIT",
    "struct input_event",
    "open(",
    "ioctl(",
    "poll(",
    "read(",
]

FORBIDDEN_CODE_TOKENS = [
    "RELIEFOS_",
    "LEONOS_",
    "reliefos/",
    "leonos/",
    "libinput",
    "wayland",
    "drm",
    "xf86",
    "syscall(",
    "mmap(",
]

HANDWRITTEN_UAPI = re.compile(
    r"struct\s+(input_event|fb_var_screeninfo|fb_fix_screeninfo|input_id|timeval)\s*\{")
NUMERIC_IOCTL = re.compile(r"ioctl\s*\(\s*[^,]+,\s*(?:0[xX][0-9a-fA-F]+|\d+)")


def check_fbdev_uapi_source() -> None:
    source = (ROOT / "kernel/reliefnt/include/uapi/linux/fb.h").read_text()
    required = {
        "struct fb_cmap",
        "FBIOGETCMAP",
        "FBIOPUTCMAP",
        "FBIOBLANK",
        "reserved[4]",
        "type_aux",
        "visual",
        "mmio_start",
        "capabilities",
    }
    missing = sorted(token for token in required if token not in source)
    if missing:
        raise AssertionError("fbdev UAPI is missing: " + ", ".join(missing))


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def stage_source_check() -> tuple[bool, list[str]]:
    source = PROBE.read_text()
    code = strip_comments(source)
    notes = []
    ok = True

    try:
        check_fbdev_uapi_source()
    except AssertionError as error:
        ok = False
        notes.append(str(error))

    includes = re.findall(r"#include\s*<([^>]+)>", source)
    unknown = sorted(set(includes) - ALLOWED_INCLUDES)
    if unknown:
        ok = False
        notes.append("non-standard includes: " + ", ".join(unknown))
    if re.search(r'#include\s*"', source):
        ok = False
        notes.append("local #include is not allowed; use the public UAPI headers")
    missing = [token for token in REQUIRED_CODE_TOKENS if token not in code]
    if missing:
        ok = False
        notes.append("missing required ABI usage: " + ", ".join(missing))
    present = [token for token in FORBIDDEN_CODE_TOKENS
               if token.lower() in code.lower()]
    if present:
        ok = False
        notes.append("forbidden ABI usage: " + ", ".join(present))
    handwritten = HANDWRITTEN_UAPI.search(code)
    if handwritten:
        ok = False
        notes.append("handwritten UAPI structure: struct " + handwritten.group(1))
    numeric = NUMERIC_IOCTL.search(code)
    if numeric:
        ok = False
        notes.append("raw numeric ioctl command: " + numeric.group(0))
    if ok:
        notes.append(f"{len(includes)} standard includes, "
                     f"{len(REQUIRED_CODE_TOKENS)} required ABI uses verified")
    return ok, notes


def find_compiler(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    if os.environ.get("RELIEFOS_MUSL_CC"):
        return os.environ["RELIEFOS_MUSL_CC"]
    for candidate in sorted(ROOT.glob("out/*/sdk/reliefos-musl-sdk/bin/reliefos-musl-cc")):
        return str(candidate)
    return "cc"


def stage_static_link(explicit: str | None) -> tuple[bool, list[str]]:
    compiler = find_compiler(explicit)
    with tempfile.TemporaryDirectory(prefix="reliefos-xorg-abi-") as tmp:
        output = Path(tmp) / "xorg-device-probe"
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-static", str(PROBE), "-o", str(output)]
        run = subprocess.run(command, capture_output=True, text=True)
        notes = ["compiler: " + " ".join(command)]
        if run.returncode != 0:
            notes.append((run.stderr or run.stdout).strip())
            return False, notes
        notes.append("static link succeeded")
        return True, notes


def stage_guest_log(path: Path) -> dict:
    text = path.read_text()
    results = re.findall(r"^xorg-probe result failures=(\d+)", text, re.M)
    failures = re.findall(r"^xorg-probe fail (\S+)(?: errno=(\d+))?", text, re.M)
    oks = re.findall(r"^xorg-probe ok (\S+)", text, re.M)
    guest_ok = bool(results) and not failures and results[-1] == "0"
    errno_notes = [f"{name}: errno={code if code else 'assertion'}"
                   for name, code in failures]
    if not errno_notes:
        errno_notes = ["no failure errno recorded"]
    guest_notes = [f"{len(oks)} probe steps passed"]
    if results:
        guest_notes.append(f"probe result line: failures={results[-1]}")
    if failures:
        guest_notes.append(f"{len(failures)} failed steps: "
                           + ", ".join(name for name, _ in failures))
    if not results:
        guest_ok = False
        guest_notes.append("no xorg-probe result line found in the guest log")
    return {
        "guest-run": (guest_ok, guest_notes),
        "failure-errno": (bool(results), errno_notes),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-only", action="store_true",
                        help="run only the source-check stage")
    parser.add_argument("--cc", help="static C compiler for the probe")
    parser.add_argument("--guest-log", type=Path,
                        help="probe log captured from a target guest run")
    args = parser.parse_args()

    outcomes: dict[str, tuple[bool | None, list[str]]] = {}
    ok, notes = stage_source_check()
    outcomes["source-check"] = (ok, notes)

    if args.source_only:
        outcomes["static-link"] = (None, ["not run (source-only)"])
    else:
        outcomes["static-link"] = stage_static_link(args.cc)

    if args.guest_log:
        outcomes.update(stage_guest_log(args.guest_log))
    else:
        outcomes["guest-run"] = (None, ["not run (no guest log)"])
        outcomes["failure-errno"] = (None, ["not run (no guest log)"])

    failed = False
    for stage in ("source-check", "static-link", "guest-run", "failure-errno"):
        state, notes = outcomes[stage]
        label = {True: "PASS", False: "FAIL", None: "NOT RUN"}[state]
        print(f"xorg-abi {stage}: {label}")
        for note in notes:
            print(f"  {note}")
        if state is False:
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
