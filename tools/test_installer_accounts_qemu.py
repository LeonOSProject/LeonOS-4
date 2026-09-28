#!/usr/bin/env python3
"""Install onto an exclusive scratch disk, then verify actual login credentials."""
import argparse
from contextlib import contextmanager
from pathlib import Path
import subprocess
import re
import json
import time

from test_installer_window_qemu import Probe as WindowProbe, wait_log
from qmp_terminal_smoke import text_keys
from run_gcc_probe_qemu import qmp_quit

ROOT = Path(__file__).resolve().parents[1]
USER_PASSWORD = "U!" + "u" * 30
ROOT_PASSWORD = "r"


class Probe(WindowProbe):
    def click(self, x, y):
        super().click(x, y)
        # QMP acknowledges enqueueing, not the guest focus change.
        time.sleep(0.5)

    def key(self, key):
        super().key(key)
        # Allow guest key release processing before repeating the same key.
        time.sleep(0.15)

    def text(self, text):
        punctuation = {";": "semicolon", "(": "shift-9", ")": "shift-0",
                       ">": "shift-dot", "$": "shift-4", "?": "shift-slash",
                       "'": "apostrophe", '"': "shift-apostrophe", "!": "shift-1"}
        for character in text:
            self.key(punctuation.get(character, text_keys(character)[0]))
        # The keyboard and USB pointer travel through separate guest queues.
        time.sleep(1)


@contextmanager
def boot(output, disk, iso=None, *, smp=2):
    output.mkdir(parents=True, exist_ok=True)
    serial, qmp = output / "serial.log", output / "qmp.sock"
    command = ["qemu-system-x86_64", "-enable-kvm", "-cpu", "host", "-machine", "q35",
               "-m", "4096", "-smp", str(smp), "-bios", "/usr/share/edk2/x64/OVMF.4m.fd",
               "-display", "none", "-serial", f"file:{serial}",
               "-device", "VGA,xres=1280,yres=720", "-device", "qemu-xhci", "-device", "usb-tablet",
               "-netdev", "user,id=net0", "-device", "e1000,netdev=net0",
               "-drive", f"file={disk},format=raw,if=ide",
               "-qmp", f"unix:{qmp},server=on,wait=off", "-no-reboot", "-no-shutdown"]
    # An installed disk has its own EFI loader. Give the optical device an
    # explicit firmware priority so update tests actually boot the installer.
    command += (["-drive", f"file={iso},format=raw,media=cdrom,if=none,id=installer-cd",
                 "-device", "ide-cd,drive=installer-cd,bootindex=1", "-boot", "d"]
                if iso else ["-boot", "c"])
    with (output / "qemu.log").open("w") as errors:
        process = subprocess.Popen(command, stdout=errors, stderr=errors, cwd=ROOT)
        probe = None
        try:
            deadline = time.monotonic() + 15
            while not qmp.exists() and time.monotonic() < deadline:
                if process.poll() is not None: raise RuntimeError("QEMU failed to start")
                time.sleep(.1)
            probe = Probe(qmp, output)
            probe.disk = disk
            yield probe, serial, process
        finally:
            try:
                if probe is not None:
                    try: probe.frame("last-frame")
                    finally: probe.close()
            finally:
                if process.poll() is None: qmp_quit(qmp, process)
                qmp.unlink(missing_ok=True)


def next_page(probe):
    probe.click(probe.width - 165, probe.height - 33)
    time.sleep(.8)


def installer_log(probe, serial):
    """Read the actual redirected guest stderr after the finish page appears."""
    match = re.search(r"installer root ready ramdisk=(0x[0-9a-f]+).*bytes=(\d+)",
                      serial.read_text(errors="replace"))
    if not match:
        raise AssertionError("Missing installer RAM disk location in serial log")
    memory = probe.output / "installer-root.ext2"
    log_path = probe.output / "installer.log"
    probe.command("stop")
    try:
        probe.command("pmemsave", {"val": int(match[1], 16), "size": int(match[2]),
                                  "filename": str(memory)})
    finally:
        probe.command("cont")
    subprocess.run(["debugfs", "-R", f"dump /var/log/desktop.log {log_path}",
                    str(memory)], check=True)
    return log_path.read_text()


def wait_install(probe, serial, process):
    deadline = time.monotonic() + 2400
    while time.monotonic() < deadline:
        log = serial.read_text(errors="replace")
        if "[installer.elf] installation completed successfully" in log:
            probe.frame("installed")
            return
        if any("[installer.elf]" in line and " ret=-" in line for line in log.splitlines()):
            raise AssertionError("Installer failed; see serial.log and last-frame.png")
        if process.poll() is not None: raise RuntimeError("QEMU exited during installation")
        frame = probe.frame("progress")
        # The sidebar finish row is language independent. It only triggers log
        # capture; success still requires the real installer's completion record.
        finish_y = 104 + 9 * (34 if probe.height >= 600 else 26)
        if probe.width > 520 and frame.getpixel((14, finish_y)) == (229, 229, 229):
            guest_log = installer_log(probe, serial)
            assert "[installer.elf] installation completed successfully" in guest_log, guest_log
            probe.frame("installed")
            return
        time.sleep(5)
    raise AssertionError("Installation timed out")


def wait_desktop(probe, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        frame = probe.frame("desktop")
        if frame.getpixel((probe.width // 2, probe.height - 10)) != (0, 120, 212):
            return frame
        time.sleep(.5)
    raise AssertionError("Login did not transition from the ReliefOS login screen to the desktop")


def install_gui(probe, serial, process):
    wait_log(serial, "path=/usr/lib/reliefos/apps/installer/installer.elf", process)
    time.sleep(4)
    probe.frame("language")
    for _ in range(6): next_page(probe)
    y = 64 if probe.height > 640 else 44
    for index, text in enumerate(("alice", USER_PASSWORD, USER_PASSWORD, ROOT_PASSWORD, ROOT_PASSWORD)):
        probe.click(350, y + 96 + index * 54)
        probe.text(text)
    probe.frame("accounts")
    next_page(probe)
    probe.frame("confirmation")
    probe.click(350, y + 212)
    probe.text("INSTALL")
    probe.frame("confirmation-ready")
    next_page(probe)
    wait_install(probe, serial, process)


def install_tty(probe, serial, process):
    time.sleep(3)
    probe.key("down")
    probe.key("ret")
    answers = (("Mode [install/update]: ", "install"),
               ("Select disk number (r to refresh, q to quit): ", "0"),
               ("Username: ", "alice"), ("Password: ", USER_PASSWORD),
               ("Confirm password: ", USER_PASSWORD), ("root password: ", ROOT_PASSWORD),
               ("Confirm root password: ", ROOT_PASSWORD),
               ("Type INSTALL to confirm erasing this disk: ", "INSTALL"))
    for prompt, answer in answers:
        wait_log(serial, prompt, process)
        probe.text(answer)
        probe.key("ret")
    wait_install(probe, serial, process)


def login_tty(probe, serial, process, root):
    # All normal boots now start the graphical session on tty1. Exercise a
    # real independent getty on tty2 instead of guessing a GRUB menu timing.
    from test_vt_qemu import wait_text, ocr
    wait_log(serial, "path=/usr/lib/reliefos/apps/login/login.elf", process)
    wait_text(probe, process, "graphical-login", "alice", timeout=90)
    probe.key("ctrl-alt-f2")
    wait_text(probe, process, "tty-login", "login:", timeout=90)
    probe.text("root" if root else "alice")
    probe.key("ret")
    wait_text(probe, process, "tty-password", "Password:")
    probe.text(ROOT_PASSWORD if root else USER_PASSWORD)
    probe.key("ret")
    wait_text(probe, process, "tty-shell", "built-in shell", timeout=60)
    if root:
        command = ("cat /proc/self/status; echo root-ok > /root/installer-root-check; "
                   "cat /root/installer-root-check; echo group-only > /tmp/root-group-only; "
                   "chmod 040 /tmp/root-group-only; echo ROOT_SESSION_DONE")
    else:
        command = (
            "cat /proc/self/status; echo HOME=$HOME; echo user-ok > $HOME/installer-user-check; "
            "cat $HOME/installer-user-check; "
            "if (echo bad > /root/installer-denied) 2>/dev/null; then echo ACCESS_FAILED; else echo ROOT_DENIED; fi; "
            "if (echo bad > /etc/passwd) 2>/dev/null; then echo ACCESS_FAILED; else echo PASSWD_DENIED; fi; "
            "if cat /tmp/root-group-only 2>/dev/null; then echo ACCESS_FAILED; else echo ROOT_GROUP_DENIED; fi; "
            "test -e /usr/bin/python3; echo PYTHON_STATUS=$?; "
            "test -e /usr/bin/gcc; echo GCC_STATUS=$?; "
            "test -e /usr/bin/tcc; echo TCC_STATUS=$?; echo USER_SESSION_DONE")
    evidence_path = "/root/installer-root-evidence" if root else "/home/alice/installer-user-evidence"
    marker = "ROOT_SESSION_DONE" if root else "USER_SESSION_DONE"
    probe.text("( " + command + " ) > " + evidence_path + " 2>&1; sync")
    probe.key("ret")
    # Read back only after the shell has returned and flushed its writes. The
    # ordinary user need not gain access to the root-owned serial device.
    time.sleep(3)
    probe.text("cat " + evidence_path)
    probe.key("ret")
    wait_text(probe, process, "session-evidence", marker, timeout=60)
    table = json.loads(subprocess.check_output(["sfdisk", "--json", str(probe.disk)], text=True))["partitiontable"]
    partition = next(part for part in table["partitions"]
                     if part["type"].lower() == "0fc63daf-8483-4772-8e79-3d69d8477de4")
    image = probe.output / "evidence-root.ext2"
    probe.command("stop")
    try:
        subprocess.run(["dd", f"if={probe.disk}", f"of={image}", "bs=4M", "iflag=skip_bytes,count_bytes",
                        f"skip={partition['start'] * table['sectorsize']}",
                        f"count={partition['size'] * table['sectorsize']}", "conv=sparse", "status=none"], check=True)
    finally:
        probe.command("cont")
    log = subprocess.check_output(["debugfs", "-R", f"cat {evidence_path}", str(image)],
                                  stderr=subprocess.DEVNULL, text=True).replace("\r", "")
    (probe.output / "session-evidence.txt").write_text(log)
    assert marker in log.splitlines(), log
    expected = ["0" if root else "1000"] * 4
    for field in ("Uid:", "Gid:"):
        identities = [line.split()[1:] for line in log.splitlines() if line.startswith(field)]
        assert identities and all(identity == expected for identity in identities), (field, identities)
    if not root:
        for line in ("ROOT_DENIED", "PASSWD_DENIED", "ROOT_GROUP_DENIED", "HOME=/home/alice", "user-ok"):
            assert "\n" + line + "\n" in log, line
        assert "\nACCESS_FAILED\n" not in log
        for tool in ("PYTHON", "GCC", "TCC"):
            assert f"\n{tool}_STATUS=1\n" in log
    else:
        assert "\nroot-ok\n" in log
    probe.frame("root-session" if root else "user-session")


def login_desktop(probe, serial, process):
    wait_log(serial, "path=/usr/lib/reliefos/apps/login/login.elf", process)
    time.sleep(5)
    probe.frame("login")
    probe.key("down")
    probe.text(USER_PASSWORD)
    probe.key("ret")
    wait_desktop(probe)
    time.sleep(5)
    probe.key("meta_l")
    time.sleep(1)
    probe.text("terminal")
    probe.key("ret")
    wait_log(serial, "path=/usr/lib/reliefos/apps/terminal/terminal.elf", process)
    probe.text("fastfetch --format json --structure OS:Kernel")
    probe.key("ret")
    time.sleep(3)
    probe.frame("terminal-fastfetch")
    time.sleep(2)
    probe.text("cat /proc/self/status; echo HOME=$HOME")
    probe.key("ret")
    time.sleep(3)
    probe.frame("terminal-identity")
    log = serial.read_text(errors="replace")
    assert "name=terminal.elf code=127" not in log, "Terminal shell failed to execute"
    assert "name=terminal.elf code=126" not in log, "Terminal shell failed to drop credentials"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, default=ROOT / "build/images/reliefos-installer.iso")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--installer", choices=("gui", "tty"), default="gui")
    parser.add_argument("--login-only", action="store_true", help="Boot this test's existing scratch disk without reinstalling")
    parser.add_argument("--desktop", action="store_true", help="Also log into the installed desktop and capture Terminal identity")
    parser.add_argument("--desktop-only", action="store_true", help="Only boot the existing test disk into the desktop")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    disk = output / "scratch.raw"
    if args.desktop_only:
        if not disk.is_file(): parser.error("--desktop-only requires an existing scratch disk")
        with boot(output / "desktop", disk) as session:
            login_desktop(*session)
        return
    if args.login_only:
        if not disk.is_file(): parser.error("--login-only requires an existing scratch disk")
    else:
        with disk.open("xb") as file: file.truncate(8 * 1024**3)
        with boot(output / "install", disk, args.iso.resolve()) as session:
            (install_gui if args.installer == "gui" else install_tty)(*session)
    with boot(output / "root", disk) as session:
        login_tty(*session, True)
    with boot(output / "user", disk) as session:
        login_tty(*session, False)
    if args.desktop:
        with boot(output / "desktop", disk) as session:
            login_desktop(*session)
    print(f"PASS: installer, ordinary/root credentials: {output}")


if __name__ == "__main__":
    main()
