#!/usr/bin/env python3
"""Check the production kernel release and unmodified e2fsprogs in a scratch guest."""
import argparse
import json
import re
import subprocess
import time

from pathlib import Path
from test_installer_accounts_qemu import boot
from test_lsblk_filesystems_qemu import ROOT, frame_until, partition_file, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--release", required=True, help="expected uname release, including suffix")
    parser.add_argument("--smp", type=int, choices=(1, 2), default=2)
    args = parser.parse_args()
    assert re.fullmatch(r"[A-Za-z0-9._+\-]{1,31}", args.release), "invalid release"
    output = ROOT / f"build/kernel-version-qemu/{args.release}/{args.smp}cpu"
    output.mkdir(parents=True, exist_ok=True)
    disk, root = output / "scratch.raw", output / "root.ext4"
    run("cp", "--reflink=auto", args.image.resolve(), disk)
    partitions = json.loads(subprocess.check_output(["sfdisk", "--json", str(disk)]))["partitiontable"]["partitions"]
    assert len(partitions) == 2
    partition_file(disk, partitions[1], root)
    script = output / "verify.sh"
    script.write_text(f"""#!/bin/sh
set -eu
exec >/root/kernel-version-verify.log 2>&1
trap 'status=$?; sync; printf "[kernel-version] DONE status=%s\\n" "$status" >/dev/serial0' EXIT
expected='{args.release}'
uname -a
test "$(uname -r)" = "$expected"
test "$(cat /proc/sys/kernel/osrelease)" = "$expected"
case "$(cat /proc/version)" in
    "ReliefNT version $expected ("*) ;;
    *) exit 1 ;;
esac
truncate -s 32M /root/kernel-version-fs.ext4
mkfs.ext4 -F -q /root/kernel-version-fs.ext4
sync
test "$(blkid -p -s TYPE -o value /root/kernel-version-fs.ext4)" = ext4
fsck.ext4 -fn /root/kernel-version-fs.ext4
rm /root/kernel-version-fs.ext4
printf 'PASS kernel release %s: uname, procfs, mkfs.ext4, blkid and fsck.ext4\\n' "$expected"
""")
    run("debugfs", "-w", "-R", f"write {script} /root/kernel-version-verify.sh", root, capture_output=True)
    run("dd", f"if={root}", f"of={disk}", "bs=512", f"seek={partitions[1]['start']}",
        "conv=notrunc", "status=none")
    with boot(output, disk, smp=args.smp) as (probe, serial, process):
        frame_until(probe, process, "login", lambda f: f.width == 1920 and f.getpixel((705, 365)) == (229, 229, 229))
        probe.click(800, 482)
        probe.click(900, 604)
        probe.text("root")
        probe.click(1140, 678)
        frame_until(probe, process, "desktop", lambda f: f.getpixel((705, 365)) != (229, 229, 229) and
                    f.getpixel((20, 1060)) != (0, 120, 212))
        probe.click(45, 1064)
        probe.click(225, 748)
        time.sleep(2)
        probe.text("sh /root/kernel-version-verify.sh")
        probe.key("ret")
        deadline = time.monotonic() + 240
        while "[kernel-version] DONE" not in serial.read_text(errors="replace"):
            assert process.poll() is None and time.monotonic() < deadline, "kernel version guest timed out"
            time.sleep(.5)
        probe.text("uname -a; cat /proc/version; lsblk -f")
        probe.key("ret")
        time.sleep(5)
        probe.frame("kernel-version")
    partition_file(disk, partitions[1], root)
    log = subprocess.check_output(["debugfs", "-R", "cat /root/kernel-version-verify.log", str(root)],
                                  stderr=subprocess.DEVNULL).decode()
    (output / "verify.log").write_text(log)
    assert f"PASS kernel release {args.release}:" in log, log
    print(log, end="")


if __name__ == "__main__":
    main()
