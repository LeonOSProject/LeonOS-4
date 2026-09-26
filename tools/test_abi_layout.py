#!/usr/bin/env python3
"""Freeze UAPI wire layouts and constant values; fail on any ABI drift.

The golden file records, for every struct/union defined in the exported
UAPI surface: sizeof, align and every member offset, plus the numeric
value of the load-bearing constants (ioctl numbers, magics, versions).
Run with --update after an INTENTIONAL ABI change; an accidental change
must fail this test.

Layout data comes from clang's record-layout dump of a probe TU that
instantiates every discovered wire struct, so new structs are picked up
automatically (and require a golden update to pass).
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "tools/tests/abi_layout_golden.json"

# Headers that hold user/kernel wire contracts (pre- and post-split).
WIRE_HEADERS = [
    "uapi/leonos/auth_user.h", "uapi/leonos/fs_abi.h", "uapi/leonos/net_control.h",
    "uapi/leonos/rootfs.h", "uapi/leonos/syscall_abi.h",
    "leonos/audio.h", "leonos/auth.h", "leonos/boot_handoff.h", "leonos/device.h",
    "leonos/driver.h", "leonos/elf_abi.h", "leonos/gpu.h", "leonos/inputm.h",
    "leonos/kernel_debug.h", "leonos/net.h", "leonos/pty.h", "leonos/signal.h",
    "leonos/startup.h", "leonos/system.h",
    # split targets, once they exist
    "uapi/leonos/audio_abi.h", "uapi/leonos/auth_abi.h", "uapi/leonos/device_abi.h",
    "uapi/leonos/driver_abi.h", "uapi/leonos/gpu_abi.h", "uapi/leonos/inputm_abi.h",
    "uapi/leonos/kernel_debug_abi.h", "uapi/leonos/net_abi.h",
    "uapi/leonos/startup_abi.h", "uapi/leonos/system_abi.h",
    "uapi/leonos/pty_abi.h", "uapi/leonos/signal_abi.h",
]

# Numeric constants whose value is ABI (ioctl/magic/version/enum encodings).
CONSTANTS = [
    "LEONOS_SYS_NICE",
    "LEONOS_BOOT_HANDOFF_VERSION",
    "LEONOS_DRIVER_MODULE_MAGIC", "LEONOS_DRIVER_ABI_VERSION",
    "LEONOS_DRIVER_CONTROL_IOCTL",
    "LEONOS_VT_GETGENERATION", "LEONOS_EVIOCSVT",
    "LEONOS_NET_CONTROL_IOCTL",
    "LEONOS_KERNEL_DEBUG_CONTROL_GET_STATE", "LEONOS_KERNEL_DEBUG_CONTROL_SET_ENABLED",
    "LEONOS_KERNEL_DEBUG_CONTROL_ARM_NEXT_BOOT", "LEONOS_KERNEL_DEBUG_CONTROL_CLEAR",
    "LEONOS_IOCTL_GPU_INFO", "LEONOS_IOCTL_GPU_CREATE", "LEONOS_IOCTL_GPU_DESTROY",
    "LEONOS_IOCTL_GPU_RENDER", "LEONOS_IOCTL_GPU_DIAGNOSTICS",
    "LEONOS_AUTH_ROLE_NONE", "LEONOS_AUTH_ROLE_USER", "LEONOS_AUTH_ROLE_ADMIN",
    "LEONOS_AUTH_USER_DISABLED", "LEONOS_AUTH_UPDATE_ROLE", "LEONOS_AUTH_UPDATE_FLAGS",
    "LEONOS_PERF_MAX_CPUS",
    "LEONOS_TASK_AFFINITY_GET", "LEONOS_TASK_AFFINITY_SET",
    "LEONOS_PTY_NCCS", "LEONOS_PTY_PATH_LEN",
    "LEONOS_INPUTM_MAX_PROVIDERS", "LEONOS_INPUTM_MAX_CANDIDATES",
    "LEONOS_FS_TYPE_FILE", "LEONOS_FS_TYPE_DIR",
]

INCLUDES = ["-I", str(ROOT / "include/uapi"), "-I", str(ROOT / "include"),
            "-I", str(ROOT / "userland/runtime/include"), "-I", str(ROOT / "include/uapi")]


def wire_header_paths():
    """Existing wire headers; optional split targets are skipped until present."""
    paths = []
    for relative in WIRE_HEADERS:
        path = ROOT / "include" / relative
        if path.is_file():
            paths.append(path)
    return paths


def discover_structs(paths):
    """Every struct/union *definition* in the wire headers (skip forward decls).

    Maps name -> "struct"/"union" kind so the probe uses the right keyword.
    """
    found = {}
    definition = re.compile(r"^\s*struct\s+(leonos_\w+)\s*\{", re.M)
    union_definition = re.compile(r"^\s*union\s+(leonos_\w+)\s*\{", re.M)
    for path in paths:
        text = path.read_text()
        for match in definition.finditer(text):
            found.setdefault(match.group(1), "struct")
        for match in union_definition.finditer(text):
            found.setdefault(match.group(1), "union")
    return found


def probe_source(structs):
    lines = ['#include <leonos/%s>' % p.name for p in
             sorted({p for p in wire_header_paths()})]
    # Include by wire path so uapi/leonos files resolve as <leonos/...> too.
    # A sizeof assertion forces clang to complete each record's layout here;
    # -fdump-record-layouts only reports records whose layout was computed.
    for name in sorted(structs):
        kind = structs[name]
        lines.append('_Static_assert(sizeof(%s %s) > 0, "layout %s");'
                     % (kind, name, name))
    lines.append("#include <stdio.h>")
    lines.append("int main(void) {")
    for const in CONSTANTS:
        lines.append('  printf("%%s=%%lld\\n", "%s", (long long)(%s));' % (const, const))
    lines.append("  return 0; }")
    return "\n".join(lines) + "\n"


def parse_record_layouts(dump, wanted):
    """clang -fdump-record-layouts text -> {record: {size, align, members}}."""
    records = {}
    current = None
    start = re.compile(r"^\s*\d+ \| (?:struct|union) (leonos_\w+)\s*$")
    member = re.compile(r"^\s*(\d+) \| (.+)$")
    finish = re.compile(r"^\s*\| \[sizeof=(\d+), align=(\d+)\]")
    for line in dump.splitlines():
        match = start.match(line)
        if match:
            current = match.group(1)
            if current in wanted:
                records[current] = {"members": []}
            else:
                current = None
            continue
        if current is None:
            continue
        match = finish.match(line)
        if match:
            records[current]["size"] = int(match.group(1))
            records[current]["align"] = int(match.group(2))
            current = None
            continue
        match = member.match(line)
        if match:
            records[current]["members"].append([int(match.group(1)),
                                                match.group(2).strip()])
    return records


def collect(structs):
    with tempfile.TemporaryDirectory(prefix="leonos-abi-") as directory:
        temp = Path(directory)
        source = temp / "probe.c"
        source.write_text(probe_source(structs))
        consts = {}
        if CONSTANTS:
            binary = temp / "probe"
            subprocess.run(["clang", *INCLUDES, str(source), "-o", str(binary)],
                           check=True, capture_output=True)
            output = subprocess.run([str(binary)], check=True, capture_output=True,
                                    text=True).stdout
            for line in output.splitlines():
                key, _, value = line.partition("=")
                consts[key] = int(value)
        dump = subprocess.run(
            ["clang", "-fsyntax-only", "-Xclang", "-fdump-record-layouts",
             *INCLUDES, str(source)],
            check=True, capture_output=True, text=True).stdout
        # Anonymous-member names embed the absolute header path ("unnamed at
        # /abs/include/..."); normalize so the goldens are location-independent.
        dump = dump.replace(str(ROOT) + "/", "")
        layout = parse_record_layouts(dump, set(structs))
        missing = sorted(set(structs) - set(layout))
        if missing:
            raise SystemExit(f"layout dump missed structs: {missing}")
        return {"consts": consts, "layout": layout}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true",
                        help="rewrite the golden file after an intentional change")
    args = parser.parse_args()
    paths = wire_header_paths()
    structs = discover_structs(paths)
    current = collect(structs)
    if args.update:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_text(json.dumps(current, indent=1, sort_keys=True) + "\n")
        print(f"updated {GOLDEN.relative_to(ROOT)}: "
              f"{len(current['layout'])} records, {len(current['consts'])} constants")
        return
    if not GOLDEN.is_file():
        raise SystemExit(f"missing golden {GOLDEN}; run with --update first")
    golden = json.loads(GOLDEN.read_text())
    failures = []
    for record in sorted(set(golden["layout"]) | set(current["layout"])):
        want, have = golden["layout"].get(record), current["layout"].get(record)
        if want is None:
            failures.append(f"{record}: new record (run --update after review)")
        elif have is None:
            failures.append(f"{record}: disappeared")
        elif want != have:
            for key in ("size", "align"):
                if want.get(key) != have.get(key):
                    failures.append(f"{record}: {key} {want.get(key)} -> {have.get(key)}")
            want_members = {tuple(m) for m in want["members"]}
            have_members = {tuple(m) for m in have["members"]}
            for offset, decl in sorted(want_members - have_members):
                failures.append(f"{record}: member lost/moved: {offset} {decl}")
            for offset, decl in sorted(have_members - want_members):
                failures.append(f"{record}: member added/moved: {offset} {decl}")
    for const in sorted(set(golden["consts"]) | set(current["consts"])):
        want, have = golden["consts"].get(const), current["consts"].get(const)
        if want != have:
            failures.append(f"constant {const}: {want} -> {have}")
    if failures:
        print("ABI DRIFT detected (this is what the test is for):")
        for line in failures:
            print("  " + line)
        raise SystemExit(1)
    print(f"PASS ABI layout: {len(current['layout'])} records, "
          f"{len(current['consts'])} constants unchanged")


if __name__ == "__main__":
    main()
