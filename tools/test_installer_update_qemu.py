#!/usr/bin/env python3
"""Update an isolated copy of an installed test disk through the actual GUI."""
import argparse
from pathlib import Path
import subprocess
import re
import json

from test_installer_accounts_qemu import boot, next_page
from test_installer_mode_switch_qemu import wait_page

ROOT = Path(__file__).resolve().parents[1]


def installed_state(disk, directory):
    table = json.loads(subprocess.check_output(["sfdisk", "--json", str(disk)], text=True))["partitiontable"]
    partition = next(part for part in table["partitions"]
                     if part["type"].lower() == "0fc63daf-8483-4772-8e79-3d69d8477de4")
    image = directory / "target-root.ext2"
    subprocess.run(["dd", f"if={disk}", f"of={image}", "bs=1M", "iflag=skip_bytes,count_bytes",
                    f"skip={partition['start'] * table['sectorsize']}",
                    f"count={partition['size'] * table['sectorsize']}", "conv=sparse", "status=none"], check=True)
    result = {}
    for name, path in (("world", "/etc/apk/world"), ("installed", "/lib/apk/db/installed")):
        value = subprocess.check_output(["debugfs", "-R", f"cat {path}", str(image)],
                                        stderr=subprocess.DEVNULL, text=True)
        assert value, f"Missing {path}"
        result[name] = value
    image.unlink()
    return result


def package_versions(database):
    versions = {}
    for entry in database.split("\n\n"):
        fields = dict(line.split(":", 1) for line in entry.splitlines()
                      if line.startswith(("P:", "V:")))
        if "P" in fields:
            versions[fields["P"]] = fields["V"]
    return versions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk", type=Path, required=True, help="Read-only input; only its copy is updated")
    parser.add_argument("--iso", type=Path, default=ROOT / "build/images/leonos4-installer.iso")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=2400)
    parser.add_argument("--preserve-package", action="append", default=[])
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    disk = output / "scratch.raw"
    if disk.exists():
        parser.error("output scratch.raw already exists; use a new output directory")
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", args.disk.resolve(), disk], check=True)
    before = installed_state(disk, output) if args.preserve_package else None
    if before:
        versions = package_versions(before["installed"])
        assert all(name in versions for name in args.preserve_package), "Fixture lacks required packages"
    with boot(output / "update", disk, args.iso.resolve()) as (probe, serial, process):
        wait_page(probe, process, "Select Language")
        probe.frame("language")
        for _ in range(4):
            next_page(probe)
        y = 64 if probe.height > 640 else 44
        probe.click(350, y + 192)
        next_page(probe)
        next_page(probe)
        wait_page(probe, process, "Confirm Update")
        probe.click(350, y + 212)
        probe.text("UPDATE")
        probe.frame("confirmation")
        next_page(probe)
        try:
            wait_page(probe, process, "was updated on the selected disk", args.timeout)
            probe.frame("updated")
        finally:
            log = serial.read_text(errors="replace")
            match = re.search(r"installer root ready ramdisk=(0x[0-9a-f]+).*bytes=(\d+)", log)
            if match:
                memory = output / "installer-root.ext2"
                probe.command("stop")
                probe.command("pmemsave", {"val": int(match[1], 16), "size": int(match[2]),
                                          "filename": str(memory)})
                subprocess.run(["debugfs", "-R", f"dump /var/log/installer.log {output / 'installer.log'}",
                                str(memory)], check=True)
        log = (output / "installer.log").read_text()
        assert "APK update wait_status=0" in log and "update completed successfully" in log
    if before:
        after = installed_state(disk, output)
        updated = package_versions(after["installed"])
        assert before["world"] == after["world"], "Update changed world"
        for name in args.preserve_package:
            assert versions[name] == updated.get(name), f"Update changed {name}"
        assert any(name.startswith("leonos-") and updated.get(name) != version
                   for name, version in versions.items()), "No LeonOS package was upgraded"
        print("PASS preserved world and packages: " + ", ".join(args.preserve_package))
    print(f"PASS GUI update: confirmation, package transaction and completion; {serial}")


if __name__ == "__main__":
    main()
