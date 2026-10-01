#!/usr/bin/env python3
"""Build the Xorg desktop backend image and verify its session in real QEMU.

The script keeps its build in the isolated output tree out/xorg-qemu and all
verification artifacts under --output. It never downloads anything: missing
cache entries are reported with their lock ids and the `make fetch` command.
The guest evidence is collected from the serial console, QMP screenshots, the
Xorg server log, the session wrapper log and the in-guest device probe log.
"""
import argparse
import json
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_O = ROOT / "out/xorg-qemu"
SEMANTIC_LINES = {
    "Xorg server started": "xterm client connected to the Xorg server",
    "xterm client started": "the xterm X11 client launched",
    "/bin/login prompt reached": "xterm runs /bin/login for real sign-in",
    "native windowd/sessiond not started": "the native desktop stack stayed out",
    "Xorg server exited with status": "the Xorg session ended with a status",
    "tty1 restored to text login": "tty1 fell back to the text login",
}


def log(message: str) -> None:
    print(f"xorg-qemu: {message}", flush=True)


def run(command: list[str], *, capture: bool = False) -> subprocess.CompletedProcess:
    log("run: " + " ".join(str(part) for part in command))
    return subprocess.run(command, cwd=ROOT, check=True, text=True,
                          capture_output=capture)


class Qmp:
    def __init__(self, sock_path: Path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        for _ in range(200):
            try:
                self.sock.connect(str(sock_path))
                break
            except OSError:
                time.sleep(0.05)
        else:
            raise RuntimeError(f"QMP socket never appeared: {sock_path}")
        self._recv()

    def _recv(self) -> None:
        self.sock.settimeout(0.2)
        while True:
            try:
                data = self.sock.recv(4096)
            except OSError:
                break
            if not data or len(data) < 4096:
                break

    def execute(self, name: str, arguments: dict | None = None, delay: float = 0.1) -> None:
        message = {"execute": name}
        if arguments:
            message["arguments"] = arguments
        self.sock.sendall((json.dumps(message) + "\r\n").encode())
        time.sleep(delay)
        self._recv()

    def hmp(self, command_line: str, delay: float = 0.1) -> None:
        self.execute("human-monitor-command", {"command-line": command_line}, delay)

    def send_keys(self, keys: tuple[str, ...], delay: float = 0.08) -> None:
        for key in keys:
            self.hmp(f"sendkey {key}", delay)

    def screendump(self, path: Path) -> None:
        self.hmp(f"screendump {path}", 0.4)


SPECIAL_KEYS = {
    " ": "spc", ".": "dot", "/": "slash", "-": "minus", "_": "shift-minus",
    ">": "shift-dot", "<": "shift-comma", "=": "equal", ":": "shift-semicolon",
}


def text_keys(text: str) -> tuple[str, ...]:
    keys: list[str] = []
    for character in text:
        if "A" <= character <= "Z":
            keys.append(f"shift-{character.lower()}")
        else:
            keys.append(SPECIAL_KEYS.get(character, character))
    return tuple(keys)


def ppm_stats(path: Path) -> tuple[float, float, int]:
    """Return (mean luminance, bright-pixel fraction, max luminance)."""
    data = path.read_bytes()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if not match:
        raise RuntimeError(f"unexpected screendump format: {path}")
    width, height, _maximum = (int(value) for value in match.groups())
    pixels = data[match.end():match.end() + width * height * 3]
    luminance_sum = 0
    bright = 0
    peak = 0
    samples = 0
    for index in range(0, len(pixels), 3 * 29):
        value = (pixels[index] * 299 + pixels[index + 1] * 587 + pixels[index + 2] * 114) // 1000
        luminance_sum += value
        if value > 128:
            bright += 1
        if value > peak:
            peak = value
        samples += 1
    return luminance_sum / samples, bright / samples, peak


def read_config() -> dict:
    values = {}
    config = BUILD_O / "config/.config"
    for line in config.read_text().splitlines():
        match = re.match(r'^(CONFIG_[A-Z0-9_]+)=(.*)$', line)
        if match:
            values[match.group(1)] = match.group(2).strip('"')
    return values


def prepare_config() -> None:
    run(["make", "-s", f"O={BUILD_O.relative_to(ROOT)}", "defconfig"])
    config = BUILD_O / "config/.config"
    text = config.read_text()
    text = text.replace("CONFIG_DESKTOP_BACKEND_RELIEFOS=y",
                        "# CONFIG_DESKTOP_BACKEND_RELIEFOS is not set")
    text += "CONFIG_DESKTOP_BACKEND_XORG=y\n"
    config.write_text(text)
    run(["make", "-s", f"O={BUILD_O.relative_to(ROOT)}", "olddefconfig"])
    result = config.read_text()
    if result.count("CONFIG_DESKTOP_BACKEND_XORG=y") != 1 or \
            "CONFIG_DESKTOP_BACKEND_RELIEFOS=y" in result:
        raise RuntimeError("desktop backend selection is not exclusively Xorg")


def verify_cache(cache: Path) -> None:
    command = ["make", "-s", f"O={BUILD_O.relative_to(ROOT)}",
               f"RELIEFOS_CACHE={cache}", "reliefos-verify-cache"]
    log("run: " + " ".join(command))
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(
            "xorg-qemu: cache verification failed; the listed lock entries are "
            "missing from the cache. Nothing was downloaded - run "
            f"'make fetch' (cache: {cache}) and retry.")


def build_image(cache: Path, output: Path, jobs: int) -> None:
    command = ["make", "-s", f"-j{jobs}", f"O={BUILD_O.relative_to(ROOT)}",
               f"RELIEFOS_CACHE={cache}", "image-vmdk", "sdk"]
    log("run: " + " ".join(command))
    build_log = output / "build.log"
    with build_log.open("w") as handle:
        result = subprocess.run(command, cwd=ROOT, text=True, stdout=handle,
                                stderr=subprocess.STDOUT)
    if result.returncode != 0:
        tail = build_log.read_text().splitlines()[-30:]
        sys.stderr.write("\n".join(tail) + "\n")
        raise SystemExit(f"xorg-qemu: build failed; full log: {build_log}")


def compile_probe(output: Path) -> Path:
    compiler = BUILD_O / "sdk/reliefos-musl-sdk/bin/reliefos-musl-cc"
    probe = output / "xorg-device-probe"
    run([str(compiler), "-D_GNU_SOURCE", "-static", "-Iinclude",
         "-Ikernel/reliefnt/include/uapi", "tools/tests/xorg_device_probe.c",
         "-o", str(probe)])
    return probe


def root_partition(disk: Path, output: Path) -> Path:
    layout = json.loads(subprocess.run(["sfdisk", "--json", str(disk)],
                                       check=True, capture_output=True,
                                       text=True).stdout)
    table = layout["partitiontable"]
    root = table["partitions"][1]
    offset = root["start"] * table["sectorsize"]
    length = root["size"] * table["sectorsize"]
    slice_path = output / "root.ext4"
    with disk.open("rb") as source, slice_path.open("wb") as target:
        source.seek(offset)
        remaining = length
        while remaining:
            chunk = source.read(min(16 * 1024 * 1024, remaining))
            if not chunk:
                raise RuntimeError("truncated root partition")
            target.write(chunk)
            remaining -= len(chunk)
    return slice_path


def write_back(disk: Path, slice_path: Path, output: Path) -> None:
    layout = json.loads(subprocess.run(["sfdisk", "--json", str(disk)],
                                       check=True, capture_output=True,
                                       text=True).stdout)
    table = layout["partitiontable"]
    root = table["partitions"][1]
    offset = root["start"] * table["sectorsize"]
    with slice_path.open("rb") as source, disk.open("r+b") as target:
        target.seek(offset)
        shutil.copyfileobj(source, target, 16 * 1024 * 1024)


def guest_dump(slice_path: Path, guest_path: str, destination: Path) -> bool:
    result = subprocess.run(["debugfs", "-R", f"dump {guest_path} {destination}",
                             str(slice_path)], capture_output=True, text=True)
    if result.returncode != 0 or not destination.exists():
        log(f"guest log missing: {guest_path} ({result.stderr.strip()})")
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/xorg-qemu",
                        help="verification artifact directory")
    parser.add_argument("--cache", type=Path, default=ROOT / "cache/downloads",
                        help="verified download cache (never written to)")
    parser.add_argument("--qemu", default="qemu-system-x86_64",
                        help="QEMU binary")
    parser.add_argument("--boot-timeout", type=int, default=240,
                        help="seconds to wait for the Xorg session")
    parser.add_argument("--skip-build", action="store_true",
                        help="reuse an existing out/xorg-qemu image")
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()

    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.mkdir(parents=True, exist_ok=True)
    cache = args.cache if args.cache.is_absolute() else ROOT / args.cache

    if not args.skip_build:
        prepare_config()
        verify_cache(cache)
        build_image(cache, output, args.jobs)
    else:
        log("reusing the existing out/xorg-qemu build")
    probe = compile_probe(output)

    disk = BUILD_O / "images/reliefos.raw"
    if not disk.is_file():
        raise SystemExit(f"xorg-qemu: disk image is missing: {disk}")
    work_disk = output / "disk.raw"
    log(f"preparing test disk {work_disk}")
    shutil.copyfile(disk, work_disk)

    slice_path = root_partition(work_disk, output)
    run(["debugfs", "-w", "-R", f"write {probe} /root/xorg-device-probe",
         str(slice_path)])
    run(["debugfs", "-w", "-R", "set_inode_field /root/xorg-device-probe mode 0100755",
         str(slice_path)])
    listing = subprocess.run(["debugfs", "-R", "stat /root/xorg-device-probe",
                              str(slice_path)], capture_output=True, text=True)
    if "Inode:" not in listing.stdout:
        raise SystemExit("xorg-qemu: the device probe did not land in the guest root")
    write_back(work_disk, slice_path, output)

    config = read_config()
    firmware = config.get("CONFIG_QEMU_OVMF_PATH", "")
    if not firmware or not Path(firmware).is_file():
        for candidate in ("/usr/share/edk2/x64/OVMF.4m.fd", "/usr/share/ovmf/OVMF.fd",
                          "/usr/share/OVMF/OVMF_CODE_4M.fd", "/usr/share/qemu/OVMF.fd"):
            if Path(candidate).is_file():
                firmware = candidate
                break
    if not firmware or not Path(firmware).is_file():
        raise SystemExit("xorg-qemu: no OVMF firmware found for the UEFI boot")

    memory = config.get("CONFIG_QEMU_MEMORY_MB", "4096")
    cpus = config.get("CONFIG_QEMU_SMP_CPUS", "4")
    width = config.get("CONFIG_QEMU_DISPLAY_WIDTH", "1920")
    height = config.get("CONFIG_QEMU_DISPLAY_HEIGHT", "1080")
    accel = ["-enable-kvm", "-cpu", "host"] if config.get("CONFIG_QEMU_ENABLE_KVM") == "y" \
        else ["-cpu", "max"]

    qmp_socket = output / "qmp.sock"
    qmp_socket.unlink(missing_ok=True)
    serial_log = output / "serial.log"
    command = [args.qemu, "-machine", "q35", *accel, "-m", memory, "-smp", cpus,
               "-bios", firmware, "-display", "none",
               "-serial", f"file:{serial_log}",
               "-device", f"VGA,xres={width},yres={height}",
               "-drive", f"file={work_disk},if=none,id=disk0,format=raw",
               "-device", "ich9-ahci,id=ahci",
               "-device", "ide-hd,drive=disk0,bus=ahci.0",
               "-qmp", f"unix:{qmp_socket},server=on,wait=off",
               "-no-reboot"]
    log("boot: " + " ".join(command))
    machine = subprocess.Popen(command, cwd=ROOT)
    checks: dict[str, tuple[bool, str]] = {}
    try:
        qmp = Qmp(qmp_socket)
        qmp.execute("qmp_capabilities")

        # UEFI, GRUB and the userland handoff need about half a minute; the
        # Xterm window then paints the first bright frame on the VGA surface.
        x_frame = None
        deadline = time.time() + args.boot_timeout
        time.sleep(40)
        frame_index = 0
        while time.time() < deadline:
            frame_index += 1
            frame = output / f"frame-{frame_index:03d}.ppm"
            qmp.screendump(frame)
            mean, bright, peak = ppm_stats(frame)
            log(f"frame {frame.name}: mean={mean:.1f} bright={bright:.3f} peak={peak}")
            if bright > 0.05:
                x_frame = frame
                break
            time.sleep(5)
        if x_frame is None:
            checks["X11 frame appeared"] = (
                False, f"no bright frame within {args.boot_timeout}s; "
                       f"last frame kept in {output}")
        else:
            checks["X11 frame appeared"] = (True, str(x_frame))
            # The login prompt owns the xterm; public test image credentials
            # apply. Authentication (PAM) settles for about ten seconds before
            # the shell prompt appears, so wait for it before typing commands.
            qmp.send_keys(text_keys("root") + ("ret",))
            time.sleep(2.0)
            qmp.send_keys(text_keys("root") + ("ret",))
            time.sleep(15.0)
            qmp.send_keys(text_keys("/root/xorg-device-probe >/root/p.log") + ("ret",))
            time.sleep(6.0)
            # Leaving the login shell closes xterm, xinit and Xorg in order and
            # console-session returns tty1 to the text login.
            qmp.send_keys(text_keys("exit") + ("ret",))
            time.sleep(10.0)
            text_frame = output / "frame-after-exit.ppm"
            qmp.screendump(text_frame)
            mean_after, bright_after, peak_after = ppm_stats(text_frame)
            # A restored text console shows glyph content on a dark
            # background; the bright X11 xterm frame must be gone.
            checks["tty1 text frame after Xorg exit"] = (
                peak_after > 80 and bright_after < 0.03,
                f"{text_frame.name} mean={mean_after:.1f} bright={bright_after:.3f} peak={peak_after}")

        qmp.execute("system_powerdown", delay=0.5)
        for _ in range(60):
            if machine.poll() is not None:
                break
            time.sleep(0.5)
        else:
            qmp.execute("quit", delay=0.2)
            machine.wait(timeout=10)
    finally:
        if machine.poll() is None:
            machine.kill()
            machine.wait(timeout=10)

    serial = output / "serial.log"
    if serial.exists():
        log(f"serial log: {serial} ({serial.stat().st_size} bytes)")

    slice_path = root_partition(work_disk, output)
    logs = {}
    for guest, name in (("/var/log/xorg-session.log", "xorg-session.log"),
                        ("/var/log/Xorg.0.log", "xorg-server.log"),
                        ("/var/log/desktop.log", "desktop.log"),
                        ("/root/p.log", "xorg-probe.log")):
        logs[name] = output / name
        guest_dump(slice_path, guest, logs[name])

    session_text = logs["xorg-session.log"].read_text() if logs["xorg-session.log"].exists() else ""
    desktop_text = logs["desktop.log"].read_text() if logs["desktop.log"].exists() else ""
    for marker, meaning in SEMANTIC_LINES.items():
        checks[marker] = (marker in session_text, meaning)
    checks["native desktop log stayed silent"] = (
        "graphical-session" not in desktop_text and "windowd" not in desktop_text,
        "the native desktop session never logged anything")
    server_log = logs["xorg-server.log"]
    checks["Xorg server log"] = (
        server_log.exists() and "fbdev" in server_log.read_text(errors="replace"),
        str(server_log))

    probe_log = logs["xorg-probe.log"]
    if probe_log.exists():
        abi = subprocess.run([sys.executable, str(ROOT / "tools/test_xorg_abi.py"),
                              "--guest-log", str(probe_log)], cwd=ROOT,
                             text=True, capture_output=True)
        sys.stdout.write(abi.stdout)
        checks["guest device probe"] = (abi.returncode == 0, str(probe_log))
    else:
        checks["guest device probe"] = (False, f"{probe_log} is missing")

    print()
    failed = 0
    for name, (ok, detail) in checks.items():
        label = "PASS" if ok else "FAIL"
        print(f"xorg-qemu {label} - {name}: {detail}")
        failed += 0 if ok else 1
    print(f"xorg-qemu: artifacts in {output}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
