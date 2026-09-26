#!/usr/bin/env python3
"""Inventory the M1 service role marks (ntclks separation, phase 5.7).

The M1 authority decision (docs/superpowers/ntclks-separation/
09-m1-authority-design-review.md) rides on reserved role gids recorded in the
image inode. The mark must live in exactly one place per layer, and the layers
must agree - a drifted constant silently drops the SERVICE grant, a spread
mark hands it to the wrong binary. This test pins the inventory:

1. kernel defines: LEONOS_GID_WINDOW_SERVER/LEONOS_GID_SERVICE in
   kernel/ntclks/kernel/ntclks/user/userland.c carry the pinned values;
2. staging plan: tools/build/rootfs-stage.sh assigns gid 60001 to desktop.elf
   and 60002 to windowd.elf/imd.elf and nothing else;
3. image build: tools/build/images.sh re-applies those gids after its blanket
   chown, for the runtime apps and the embedded /install/root payload alike
   (the in-guest installer copies uid/gid through to the installed system);
4. guest groups: system/rootfs/etc/group and system/test-accounts/group
   declare the reserved, memberless groups at the same numbers;
5. built manifest (optional, --manifest): the set of entries whose gid != 0 is
   exactly the three service images - no fourth file may carry a role mark.

Checks 1-4 run against the checked-in sources; check 5 needs a built tree
(`make O=<dir> all`, manifest at <dir>/rootfs/manifest.json) and is reported
as skipped when no manifest is available.
"""
from pathlib import Path
import argparse
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
KERNEL_USERLAND = (ROOT / "kernel/ntclks/kernel/ntclks/user/userland.c")
ROOTFS_STAGE = ROOT / "tools/build/rootfs-stage.sh"
IMAGES = ROOT / "tools/build/images.sh"
GROUP_FILES = [ROOT / "system/rootfs/etc/group", ROOT / "system/test-accounts/group"]

WINDOW_SERVER_GID = 60001
SERVICE_GID = 60002

# The complete mark inventory: path -> role gid. Nothing else may be marked.
ROLE_MARKS = {
    "/usr/lib/leonos/apps/desktop/desktop.elf": WINDOW_SERVER_GID,
    "/usr/lib/leonos/apps/windowd/windowd.elf": SERVICE_GID,
    "/usr/lib/leonos/apps/imd/imd.elf": SERVICE_GID,
}

GROUP_NAMES = {
    "leonos-window-server": WINDOW_SERVER_GID,
    "leonos-service": SERVICE_GID,
}


def check_kernel_defines(failures):
    text = KERNEL_USERLAND.read_text()
    for name, value in (("LEONOS_GID_WINDOW_SERVER", WINDOW_SERVER_GID),
                        ("LEONOS_GID_SERVICE", SERVICE_GID)):
        match = re.search(rf"^#define {name} (\d+)u$", text, re.M)
        if not match:
            failures.append(f"KERNEL: {name} define missing in {KERNEL_USERLAND}")
        elif int(match.group(1)) != value:
            failures.append(f"KERNEL: {name}={match.group(1)}, expected {value}")


def check_staging_plan(failures):
    text = ROOTFS_STAGE.read_text()
    expected = (r"case \$app in desktop\) gid=60001 ;; windowd\|imd\) gid=60002 ;; esac")
    if not re.search(expected, text):
        failures.append("STAGE: rootfs-stage.sh role gid mapping "
                        "(desktop=60001, windowd|imd=60002) missing or changed")
    # The gid column must reach the app ELF plan line and nothing else there.
    lines = [line for line in text.splitlines()
             if "gid=60001" in line or "gid=60002" in line]
    if len(lines) != 1:
        failures.append(f"STAGE: expected exactly one role gid assignment in "
                        f"rootfs-stage.sh, found {len(lines)}")


def check_images_rechown(failures):
    text = IMAGES.read_text()
    for path, gid in ROLE_MARKS.items():
        if f"{path}:{gid}" not in text:
            failures.append(f"IMAGES: {path}:{gid} missing from images.sh "
                            "re-chown list")
    if "/install/root" not in text:
        failures.append("IMAGES: /install/root payload re-chown missing "
                        "(installed system would lose its role marks)")


def check_group_files(failures):
    for path in GROUP_FILES:
        names = {}
        for line in path.read_text().splitlines():
            if not line or line.startswith("#"):
                continue
            name, password, gid, members = (line.split(":") + [""] * 4)[:4]
            if name in GROUP_NAMES:
                names[name] = (password, int(gid), members)
        for name, gid in GROUP_NAMES.items():
            if name not in names:
                failures.append(f"GROUP: {name} missing from {path}")
                continue
            password, actual, members = names[name]
            if actual != gid:
                failures.append(f"GROUP: {path} {name} gid={actual}, expected {gid}")
            if members:
                failures.append(f"GROUP: {path} {name} is not memberless "
                                f"(members={members!r}); non-root must not chgrp in")


def check_manifest(manifest, failures):
    document = json.loads(manifest.read_text())
    marked = {}
    for entry in document.get("entries", []):
        gid = entry.get("gid", 0)
        if gid:
            marked[entry["path"]] = gid
    if marked != ROLE_MARKS:
        extras = sorted(set(marked) - set(ROLE_MARKS))
        missing = sorted(set(ROLE_MARKS) - set(marked))
        wrong = sorted(path for path in set(marked) & set(ROLE_MARKS)
                       if marked[path] != ROLE_MARKS[path])
        for path in extras:
            failures.append(f"MANIFEST: unapproved role mark on {path} "
                            f"(gid={marked[path]}) - mark spread")
        for path in missing:
            failures.append(f"MANIFEST: {path} carries no role mark")
        for path in wrong:
            failures.append(f"MANIFEST: {path} gid={marked[path]}, "
                            f"expected {ROLE_MARKS[path]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        help="built rootfs manifest.json (e.g. <O>/rootfs/manifest.json); "
                             "skips the built-mark inventory when omitted")
    args = parser.parse_args()

    failures = []
    check_kernel_defines(failures)
    check_staging_plan(failures)
    check_images_rechown(failures)
    check_group_files(failures)
    if args.manifest:
        check_manifest(args.manifest, failures)
    else:
        print("note - built manifest not provided; source inventory only "
              "(pass --manifest <O>/rootfs/manifest.json after a build)")

    if failures:
        print("SERVICE MARKER inventory violations:")
        for line in failures:
            print(f"  {line}")
        raise SystemExit(1)
    scope = "source + built manifest" if args.manifest else "source"
    print(f"PASS service marker: {len(ROLE_MARKS)} role marks pinned "
          f"({scope}): " + ", ".join(sorted(ROLE_MARKS)))


if __name__ == "__main__":
    main()
