#!/usr/bin/env python3
"""Install official Alpine packages on the host, execute them on NTCLKS.

This runtime-focused fixture complements test_apk_qemu.py's guest transactions.
Package signatures are verified by upstream apk; no binaries are modified.
"""
import argparse
import json
from pathlib import Path
import shutil
import tempfile

from apk_distribution import ROOT, bootstrap, run
from make_live_root import make_live_tree
from make_ext2_root import write_ext2_root
import test_apk_qemu as guest_runner

WORK = ROOT / "build/alpine-runtime"
PACKAGES = ["gcc", "leonos-musl-dev", "make", "nano", "jq", "openssl", "curl"]


def prepare(refresh=False):
    apk = bootstrap()
    managed = ROOT / "build/apk/root"
    signature = (ROOT / "build/apk/normal/manifest.json").read_text()
    root = WORK / "root"
    stamp = WORK / "source.json"
    expected = {"source": signature, "packages": PACKAGES}
    cache = WORK / "cache"
    cache.mkdir(exist_ok=True)
    if refresh or not stamp.is_file() or json.loads(stamp.read_text()) != expected:
        with tempfile.TemporaryDirectory(prefix="prepare-", dir=WORK) as directory:
            stage = Path(directory) / "root"
            shutil.copytree(managed, stage, symlinks=True)
            base = ["unshare", "-Ur", apk, "--root", stage,
                    "--repositories-file", ROOT / "system/rootfs/etc/apk/repositories",
                    "--repository", stage / "usr/share/leonos/apk/repository/packages.adb",
                    "--cache-dir", cache, "--cache-packages", "--timeout", "60"]
            run([*base, "update"])
            gcc = ROOT / "build/apk-qemu/upstream/gcc-15.2.0-r5.apk"
            selection = [str(gcc) if name == "gcc" and gcc.is_file() else name for name in PACKAGES]
            run([*base, "add", "--upgrade", *selection])
            if root.exists(): shutil.rmtree(root)
            shutil.move(stage, root)
            stamp.write_text(json.dumps(expected))
    sysroot = ROOT / "build/musl/sysroot"
    run(["clang", "--target=x86_64-linux-musl", "-fuse-ld=lld", "-nostdlib", "-nostdinc",
         "-isystem", sysroot / "include", "-static", "-O2", "-Wall", "-Wextra", "-DALPINE_RUNTIME_ONLY",
         sysroot / "lib/crt1.o", sysroot / "lib/crti.o", ROOT / "tools/tests/apk_guest_probe.c",
         sysroot / "lib/libc.a", sysroot / "lib/crtn.o", "-o", WORK / "probe.elf"])
    with tempfile.TemporaryDirectory(prefix="image-", dir=WORK) as directory:
        stage = Path(directory) / "root"
        make_live_tree(root, stage)
        tests = stage / "usr/lib/leonos/tests"
        tests.mkdir(parents=True, exist_ok=True)
        shutil.copy2(WORK / "probe.elf", tests / "linux-inventory.elf")
        write_ext2_root(stage, WORK / "root.ext2", minimum_mib=512)
    iso = guest_runner.iso_tools
    iso.GRUB_TEMPLATE = iso.GRUB_TEMPLATE.replace("autospawn=ioctlcloexec autospawn=python315",
        "autospawn=inventory").replace("syscall-trace=/opt/python/", "").replace("set timeout=5", "set timeout=0")
    iso.build_iso(WORK / "root.ext2", WORK / "leonos4-apk.iso", WORK / "grub.cfg", WORK)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh", action="store_true")
    parser.add_argument("--run-only", action="store_true")
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    if not args.run_only: prepare(args.refresh)
    guest_runner.WORK = WORK
    guest_runner.guest(args.timeout)
