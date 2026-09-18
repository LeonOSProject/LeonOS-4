#!/usr/bin/env python3
"""Exercise issue #28 on an exclusive disk copy (requires Pillow and Tesseract)."""
import argparse
from pathlib import Path
import subprocess
import time

from PIL import ImageChops
from test_installer_accounts_qemu import boot, next_page

ROOT = Path(__file__).resolve().parents[1]


def page_text(probe, name):
    frame = probe.frame(name)
    if frame.width < 640 or frame.height < 480:
        return ""
    crop = probe.output / "content.png"
    frame.crop((240, 50, min(frame.width, 1250), 320)).resize((2020, 540)).save(crop)
    return subprocess.check_output(["tesseract", str(crop), "stdout", "--psm", "6"],
                                   stderr=subprocess.DEVNULL, text=True)


def wait_page(probe, process, text, timeout=90):
    deadline = time.monotonic() + timeout
    previous = ""
    while time.monotonic() < deadline:
        actual = page_text(probe, "progress")
        if actual != previous:
            print(actual, flush=True)
            previous = actual
        if text in actual:
            return
        if "Failed" in actual or process.poll() is not None:
            raise AssertionError(f"Installer failed while waiting for {text}: {actual}")
        time.sleep(2)
    raise AssertionError(f"Timed out waiting for {text}: {previous}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk", type=Path, required=True)
    parser.add_argument("--iso", type=Path, default=ROOT / "build/images/leonos4-installer.iso")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    disk = output / "scratch.raw"
    if disk.exists():
        parser.error("scratch.raw already exists; use a new output directory")
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", args.disk.resolve(), disk], check=True)
    if disk.stat().st_size < 4 * 1024**3:
        subprocess.run(["qemu-img", "resize", "-f", "raw", disk, "4G"], check=True)
    failures = []
    with boot(output, disk, args.iso.resolve()) as (probe, serial, process):
        wait_page(probe, process, "Select Language")
        probe.frame("language")
        for _ in range(4):
            next_page(probe)
        y = 64 if probe.height > 640 else 44
        probe.click(350, y + 192)
        next_page(probe)
        next_page(probe)
        wait_page(probe, process, "Confirm Update")
        before = probe.frame("update-confirm-empty")
        # The confirmation field must be focused on entry without a click.
        assert "Type UPDATE" in page_text(probe, "update-confirm-page")
        probe.text("UPDATE")
        after = probe.frame("update-confirm-typed")
        button = (probe.width - 206, probe.height - 44,
                  probe.width - 122, probe.height - 20)
        if ImageChops.difference(before.crop(button), after.crop(button)).getbbox() is None:
            failures.append("Typing UPDATE did not enable the update button")
        for _ in range(2):
            probe.click(probe.width - 258, probe.height - 33)
            time.sleep(.8)
        probe.click(350, y + 96)
        next_page(probe)
        next_page(probe)
        for index, value in enumerate(("alice", "password", "password", "rootpass", "rootpass")):
            probe.click(350, y + 96 + index * 54)
            probe.text(value)
        next_page(probe)
        probe.text("INSTALL")
        probe.frame("fresh-confirm-typed")
        next_page(probe)
        try:
            wait_page(probe, process, "Installation Complete", timeout=2400)
        except AssertionError as error:
            failures.append(str(error))
        if failures:
            raise AssertionError("; ".join(failures))
        print(f"PASS UPDATE input and update-to-fresh installation; {serial}", flush=True)


if __name__ == "__main__":
    main()
