#!/usr/bin/env python3
"""Build signed local APKs and populate roots via upstream apk transactions."""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import fcntl
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import time

from apk_ownership import ROOT, inventory, load_policy
from leonos_layout import layout_directories, apply_root_symlinks

ARCHIVE_SHA256 = "c8e2c88c13ba12a12269b79a3543e1190ff8c0ab0beb32b58cadfd5881c619e3"
BINARY_SHA256 = "5118a57ae7c07e13268a754f78aa9c7d39a0bed708bb11c101d78e2a884cee5d"
LICENSE_SHA256 = "b3c87315aae4c9f276c37168f2655dd8bd990544d7a0bbfb929664155c7ab257"
URL = "https://dl-cdn.alpinelinux.org/alpine/v3.24/main/x86_64/apk-tools-static-3.0.8-r0.apk"
LICENSE_URLS = (
    "https://raw.githubusercontent.com/alpinelinux/apk-tools/v3.0.8/LICENSE",
    "https://gitlab.alpinelinux.org/alpine/apk-tools/-/raw/v3.0.8/LICENSE",
)
REPOSITORY = "usr/share/leonos/apk/repository"
EXTERNAL = {"EFI", "grub", "leonos", "loader.elf", "install"}
_USER_NAMESPACE_AVAILABLE = None


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def _user_namespace_available():
    global _USER_NAMESPACE_AVAILABLE
    if _USER_NAMESPACE_AVAILABLE is None:
        try:
            run(["unshare", "-Ur", "true"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except (OSError, subprocess.CalledProcessError):
            _USER_NAMESPACE_AVAILABLE = False
        else:
            _USER_NAMESPACE_AVAILABLE = True
    return _USER_NAMESPACE_AVAILABLE


def _run_apk(apk, arguments, *, usermode=False):
    """Run apk with root-like metadata support on restricted CI runners."""
    if _user_namespace_available():
        run(["unshare", "-Ur", apk, *arguments])
        return
    if shutil.which("fakeroot") is None:
        raise RuntimeError("apk packaging requires unshare user namespaces or fakeroot")
    command = ["fakeroot", apk]
    if usermode:
        command += ["--usermode", "--force-no-chroot"]
    run(command + list(arguments))


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download_verified(urls, destination, expected_sha256):
    destination = Path(destination)
    temporary = destination.with_name(destination.name + ".download")
    try:
        for url in urls:
            try:
                run(["curl", "--fail", "--location", "--retry", "3",
                     "--output", temporary, url])
            except subprocess.CalledProcessError:
                continue
            if digest(temporary) == expected_sha256:
                temporary.replace(destination)
                return
        raise RuntimeError(f"unable to download verified file: {destination.name}")
    finally:
        temporary.unlink(missing_ok=True)


def bootstrap():
    directory = ROOT / "buildsystem/deps/apk-tools"
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / ".lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return _bootstrap_locked(directory)


def _bootstrap_locked(directory):
    archive = directory / "apk-tools-static-3.0.8-r0.apk"
    if not archive.exists():
        reference = ROOT / "build/apk-preparation-reference" / archive.name
        if reference.is_file() and digest(reference) == ARCHIVE_SHA256:
            shutil.copyfile(reference, archive)
        else:
            download_verified((URL,), archive, ARCHIVE_SHA256)
    if digest(archive) != ARCHIVE_SHA256:
        raise ValueError("apk-tools archive checksum mismatch")
    binary = directory / "apk.static"
    signature = directory / "apk.static.sig"
    with tarfile.open(archive, "r:gz", ignore_zeros=True) as packed:
        for name, target in (("sbin/apk.static", binary),
                             ("sbin/apk.static.SIGN.RSA.sha256.alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub", signature)):
            member = packed.getmember(name)
            if not member.isfile():
                raise ValueError(f"unexpected archive member {name}")
            with packed.extractfile(member) as src, tempfile.NamedTemporaryFile(dir=directory, delete=False) as dst:
                temporary = Path(dst.name)
                shutil.copyfileobj(src, dst)
            temporary.chmod(0o755 if target == binary else 0o644)
            temporary.replace(target)
    if digest(binary) != BINARY_SHA256:
        raise ValueError("apk-tools executable checksum mismatch")
    run(["openssl", "dgst", "-sha256", "-verify",
         ROOT / "system/rootfs/etc/apk/keys/alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub",
         "-signature", signature, binary], stdout=subprocess.DEVNULL)
    binary.chmod(0o755)
    license_file = directory / "LICENSE"
    if not license_file.exists():
        download_verified(LICENSE_URLS, license_file, LICENSE_SHA256)
    if digest(license_file) != LICENSE_SHA256:
        raise ValueError("apk-tools license checksum mismatch")
    return binary.resolve()


def signing_key(path=None):
    path = Path(path or os.environ.get("LEONOS_APK_SIGNING_KEY",
                str(Path.home() / ".local/share/leonos/apk-signing/key.pem"))).absolute()
    if path.is_symlink():
        raise ValueError("signing key must not be a symlink")
    if not path.exists():
        path.parent.mkdir(parents=True, mode=0o700, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".key-", delete=False) as file:
            temporary = Path(file.name)
        try:
            run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
                 "-out", temporary], stderr=subprocess.DEVNULL)
            temporary.chmod(0o600)
            os.link(temporary, path)  # Never replace another concurrent build's key.
        except FileExistsError:
            pass
        finally:
            temporary.unlink(missing_ok=True)
    if path.stat().st_mode & 0o077:
        raise ValueError("APK private key must have mode 0600")
    return path


def make_package(apk, key, payload, output, name, version, depends, provides=(), scripts=(), triggers=()):
    payload.mkdir(parents=True, exist_ok=True)
    args = ["mkpkg", "--files", payload, "--output", output,
            "--info", f"name:{name}", "--info", f"version:{version}",
            "--info", "arch:x86_64", "--info", f"origin:{name}",
            "--info", f"description:LeonOS build payload {name}",
            "--info", "license:LicenseRef-See-Bundled-Notices"]
    if key:
        args += ["--sign-key", key]
    if depends:
        args += ["--info", "depends:" + " ".join(sorted(depends))]
    if provides:
        args += ["--info", "provides:" + " ".join(sorted(provides))]
    for kind, script in scripts:
        args += ["--script", f"{kind}:{script}"]
    for trigger in triggers:
        args += ["--trigger", trigger]
    _run_apk(apk, args)
    return output


def copy_entry(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_symlink():
        destination.symlink_to(os.readlink(source))
    else:
        shutil.copy2(source, destination)


def package_name(group):
    return group if group.startswith("leonos-") or group in ("nano", "ca-certificates-bundle") else "leonos-" + group


def musl_development_payload(tree, destination):
    sysroot = ROOT / "build/musl/sysroot"
    if (tree / "lib/ld-musl-x86_64.so.1").read_bytes() != (sysroot / "lib/libc.so").read_bytes():
        raise ValueError("musl development sysroot does not match the packaged runtime")
    # Repackaged musl headers are not an external libxcrypt installation.
    crypt_header = tree / "usr/include/crypt.h"
    external_crypt = crypt_header.is_file() and crypt_header.read_bytes() != (sysroot / "include/crypt.h").read_bytes()
    shutil.copytree(sysroot / "include", destination / "usr/include", symlinks=True,
                    ignore=shutil.ignore_patterns("crypt.h") if external_crypt else None)
    libdir = destination / "usr/lib"
    libdir.mkdir(parents=True)
    for filename in ("crt1.o", "Scrt1.o", "rcrt1.o", "crti.o", "crtn.o", "libc.a",
                     "libcrypt.a", "libdl.a", "libm.a", "libpthread.a", "libresolv.a",
                     "librt.a", "libutil.a", "libxnet.a", "libssp_nonshared.a"):
        if external_crypt and filename == "libcrypt.a":
            continue
        shutil.copy2(sysroot / "lib" / filename, libdir / filename)
    (libdir / "libc.so").symlink_to("../../lib/ld-musl-x86_64.so.1")
    notices = destination / "usr/share/licenses/leonos-musl-dev"
    notices.mkdir(parents=True)
    shutil.copy2(ROOT / "third_party/musl/COPYRIGHT", notices / "COPYRIGHT")
    shutil.copy2(ROOT / "userland/musl-dev/stack_chk_fail_local.c", notices / "stack_chk_fail_local.c")


def build_distribution(source, output, work, apk, key):
    from openrc_packages import packages as openrc_packages
    upstream, upstream_paths, upstream_manifest = openrc_packages(apk)
    source, output, work = (Path(p).absolute() for p in (source, output, work))
    if output == source or output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError("managed output must be separate from source")
    if work.is_relative_to(source) or work.is_relative_to(output):
        raise ValueError("package work must be separate from source/output")
    work.mkdir(parents=True, exist_ok=True)
    policy, rules = load_policy(ROOT / "configs/apk-ownership.json", ROOT / "configs/components.toml")
    with tempfile.TemporaryDirectory(prefix="payload-", dir=work) as directory:
        scratch = Path(directory)
        tree = scratch / "tree"
        shutil.copytree(source, tree, symlinks=True,
                        ignore=lambda parent, names: EXTERNAL & set(names) if Path(parent) == source else ())
        for name in ("lib/apk/db", "var/cache/apk", REPOSITORY):
            path = tree / name
            if path.is_symlink():
                raise ValueError(f"invalid package state symlink: {name}")
            shutil.rmtree(path, ignore_errors=True)
        (tree / "etc/apk/world").unlink(missing_ok=True)
        (tree / "var/log/apk.log").unlink(missing_ok=True)
        # Upstream package files retain their original signatures and ownership,
        # including when repackaging the installer runtime. Local producers may
        # not overwrite them. Configuration is in LeonOS-specific scripts.
        for name in upstream_paths:
            path = tree / name
            if path.is_file() or path.is_symlink(): path.unlink()
        for name in ("run", "tmp"):
            path = tree / name
            if path.is_symlink(): raise ValueError(f"invalid runtime symlink: {name}")
            shutil.rmtree(path, ignore_errors=True)
        layout_directories(tree)
        apply_root_symlinks(tree)
        if not (tree / "bin/busybox").is_file() or not (tree / "bin/sh").is_file():
            raise ValueError("APK distribution requires the BusyBox /bin/sh interpreter for package scripts")
        shutil.copy2(apk, tree / "sbin/apk")
        # Optional BusyBox applets must yield their paths to real binutils.
        # Their fallback links are maintained by package scripts, as on Alpine.
        for applet in ("ar", "strings"):
            link = tree / "usr/bin" / applet
            if link.is_symlink() and os.readlink(link).endswith("/busybox"):
                link.unlink()
        # Retire the old ash-as-bash alias when repackaging an existing root.
        # GNU Bash from Alpine must own /bin/bash, including its script semantics.
        bash = tree / "bin/bash"
        if bash.is_symlink() and bash.resolve() == (tree / "bin/busybox").resolve():
            bash.unlink()
        notices = tree / "usr/share/licenses/apk-tools"
        notices.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(apk.parent / "LICENSE", notices / "LICENSE")
        (notices / "SOURCE.json").write_text(json.dumps({
            "name": "apk-tools-static", "version": "3.0.8-r0", "binary_url": URL,
            "archive_sha256": ARCHIVE_SHA256, "binary_sha256": BINARY_SHA256,
            "source": "https://gitlab.alpinelinux.org/alpine/apk-tools/-/tree/v3.0.8",
            "recipe": "https://gitlab.alpinelinux.org/alpine/aports/-/blob/3.24-stable/main/apk-tools/APKBUILD",
            "license": "GPL-2.0-only", "modified": False,
        }, indent=2) + "\n")
        updater = tree / "usr/lib/leonos/leonos-apk-update"
        updater.parent.mkdir(parents=True, exist_ok=True)
        updater.unlink(missing_ok=True)
        shutil.copyfile(ROOT / "userland/storage/leonos-apk-update", updater)
        updater.chmod(0o755)
        policy_file = tree / "usr/share/leonos/apk-ownership.json"
        policy_file.parent.mkdir(parents=True, exist_ok=True)
        policy_file.unlink(missing_ok=True)
        installed_policy = dict(policy, apk_registration="installed-by-upstream-apk",
                                current_distributor="leonos-apk")
        policy_file.write_text(json.dumps(installed_policy, indent=2) + "\n")
        public = scratch / "public.pem"
        run(["openssl", "pkey", "-in", key, "-pubout", "-out", public], stderr=subprocess.DEVNULL)
        public_name = "leonos-" + digest(public)[:16] + ".rsa.pub"
        (tree / "etc/apk/keys" / public_name).unlink(missing_ok=True)
        shutil.copyfile(public, tree / "etc/apk/keys" / public_name)
        repositories = tree / "etc/apk/repositories"
        repositories.unlink(missing_ok=True)
        repositories.write_text("ndx /" + REPOSITORY + "/packages.adb\n" +
            (ROOT / "system/rootfs/etc/apk/repositories").read_text())
        protected = tree / "etc/apk/protected_paths.d/leonos.list"
        protected.unlink(missing_ok=True)
        shutil.copyfile(ROOT / "system/rootfs/etc/apk/protected_paths.d/leonos.list", protected)
        # This is a real filename for the actual musl loader, not a fake version provider.
        loader = tree / "lib/ld-musl-x86_64.so.1"
        if loader.is_file():
            alias = tree / "lib/libc.musl-x86_64.so.1"
            if not alias.exists() and not alias.is_symlink():
                alias.symlink_to("ld-musl-x86_64.so.1")
            rules["lib/libc.musl-x86_64.so.1"] = "musl"
        rules["sbin/apk"] = "leonos-apk-tools"
        rules["usr/lib/leonos/leonos-apk-update"] = "leonos-apk-tools"
        rules["usr/share/licenses/apk-tools"] = "leonos-apk-tools"
        source_record = tree / "usr/share/licenses/leonos-openrc/SOURCE.json"
        source_record.parent.mkdir(parents=True, exist_ok=True)
        source_record.write_text(json.dumps(upstream_manifest, indent=2) + "\n")
        development = scratch / "musl-dev"
        development_entries = []
        if loader.is_file():
            musl_development_payload(tree, development)
            development_entries = inventory(development, {"usr": "leonos-musl-dev"})
        # Installer roots are packaged again. Generated SDK files must retain
        # their own package, including libc.so's symlink to the runtime loader.
        development_paths = {entry["path"] for entry in development_entries}
        entries = [entry for entry in inventory(tree, rules) if entry["path"] not in development_paths]
        entries = sorted(entries + development_entries, key=lambda entry: entry["path"])
        groups = defaultdict(list)
        for entry in entries:
            groups[package_name(entry["group"])].append(entry)
        provides, needed = defaultdict(set), defaultdict(set)
        providers = {}
        for name, files in groups.items():
            payload_root = development if name == "leonos-musl-dev" else tree
            for entry in files:
                path = payload_root / entry["path"].lstrip("/")
                if entry["path"] == "/bin/sh" and path.is_file() and path.stat().st_mode & 0o111:
                    provides[name].add("/bin/sh")
                if entry["type"] != "file":
                    continue
                with path.open("rb") as stream:
                    if stream.read(4) != b"\x7fELF":
                        continue
                dynamic = run(["readelf", "-dW", path], capture_output=True, text=True).stdout
                needed[name].update(re.findall(r"\(NEEDED\).*\[(.*?)\]", dynamic))
                sonames = re.findall(r"\(SONAME\).*\[(.*?)\]", dynamic)
                if re.fullmatch(r".+\.so(?:\.[0-9]+)*", path.name):
                    sonames.append(path.name)
                for soname in sonames:
                    if soname in providers and providers[soname] != name:
                        raise ValueError(f"multiple ELF providers for {soname}")
                    providers[soname] = name
                    provides[name].add("so:" + soname + "=0")
        if loader.is_file():
            providers["libc.musl-x86_64.so.1"] = "leonos-musl"
            provides["leonos-musl"].add("so:libc.musl-x86_64.so.1=1")
        # BusyBox's actual ifup/ifdown implementation is the ifupdown provider.
        applets = set(subprocess.check_output([tree / "bin/busybox", "--list"], text=True).splitlines())
        if not {"init", "ifup", "ifdown", "udhcpc", "ntpd"} <= applets:
            raise ValueError("BusyBox is missing required init/network applets")
        provides["leonos-busybox"].add("ifupdown-any")
        dependencies = defaultdict(set)
        dependencies["leonos-apk-tools"].add("leonos-busybox")
        if "ca-certificates-bundle" in groups:
            dependencies["leonos-trust"].add("ca-certificates-bundle")
        for name, requirements in needed.items():
            for requirement in requirements:
                owner = providers.get(requirement)
                if owner is None:
                    raise ValueError(f"unresolved real ELF dependency: {name}: {requirement}")
                if owner != name:
                    dependencies[name].add(owner)
        if "leonos-fastfetch" in groups:
            dependencies["leonos-fastfetch"].add("!fastfetch")
        # Base is a real package of all distribution files not assigned to another
        # producer. No records are synthesized in apk's installed database.
        version = f"0.{time.time_ns()}-r0"
        if "leonos-musl-dev" in groups:
            dependencies["leonos-musl-dev"].add("leonos-musl=" + version)
            provides["leonos-musl-dev"].update(("musl-dev", "libc-dev"))
        repository = work / "repository"
        if repository.exists():
            shutil.rmtree(repository)
        repository.mkdir()
        packages = []
        for archive in upstream:
            destination = repository / archive.name
            shutil.copy2(archive, destination)
            packages.append(destination)
        for name, files in sorted(groups.items()):
            payload_root = development if name == "leonos-musl-dev" else tree
            payload = scratch / name
            if name == "leonos-base":
                for parent, dirs, _ in os.walk(tree, followlinks=False):
                    for basename in dirs:
                        original = Path(parent) / basename
                        relative = original.relative_to(tree)
                        if original.is_symlink() or relative.parts[0] in {"dev", "proc", "sys", "run"}:
                            continue
                        destination = payload / relative
                        destination.mkdir(parents=True, exist_ok=True)
                        destination.chmod(original.stat().st_mode & 0o7777)
            for entry in files:
                relative = entry["path"].lstrip("/")
                copy_entry(payload_root / relative, payload / relative)
                for parent in (payload / relative).parents:
                    if parent == payload:
                        break
                    original = payload_root / parent.relative_to(payload)
                    if original.is_dir():
                        parent.chmod(original.stat().st_mode & 0o7777)
            scripts, triggers = [], []
            if name == "leonos-busybox":
                script = ROOT / "userland/storage/busybox-binutils-links"
                scripts = [(kind, script) for kind in ("post-install", "post-upgrade", "trigger")]
                triggers = ["/usr/bin"]
            packages.append(make_package(apk, key, payload, repository / f"{name}-{version}.apk",
                                         name, version, dependencies[name], provides[name], scripts, triggers))
        run([apk, "mkndx", "--keys-dir", tree / "etc/apk/keys", "--sign-key", key,
             "--output", repository / "packages.adb", *packages])
        managed = scratch / "managed"
        layout_directories(managed)
        shutil.copytree(ROOT / "system/rootfs/etc/apk/keys", managed / "etc/apk/keys", dirs_exist_ok=True)
        shutil.copyfile(public, managed / "etc/apk/keys" / public_name)
        _run_apk(apk, ["--root", managed, "--arch", "x86_64", "--initdb",
                       "--repositories-file", "/dev/null", "--repository", repository / "packages.adb",
                       "add", *sorted(groups), *[entry["name"] + "=" + entry["version"] for entry in upstream_manifest["packages"]]], usermode=True)
        apply_root_symlinks(managed)
        shutil.copytree(repository, managed / REPOSITORY)
        for name in EXTERNAL - {"install"}:
            path = source / name
            if path.is_dir():
                shutil.copytree(path, managed / name, symlinks=True)
            elif path.exists():
                copy_entry(path, managed / name)
        manifest = {"version": version, "packages": sorted(groups) + [entry["name"] for entry in upstream_manifest["packages"]], "upstream": upstream_manifest, "optional_packages": [],
                    "signing_public_key": public_name, "files": entries,
                    "database": "created-by-upstream-apk", "source": URL,
                    "bootstrap_sha256": BINARY_SHA256}
        (work / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        if output.exists():
            shutil.rmtree(output)
        shutil.copytree(managed, output, symlinks=True)
        print(f"Signed APK root: {len(groups)} packages, {len(entries)} owned files -> {output}")
    return output


def repackage_tree(stage, work):
    """Re-register image policy overrides from their actual final payload."""
    stage, work = Path(stage).absolute(), Path(work).absolute()
    with tempfile.TemporaryDirectory(prefix=".apk-root-", dir=stage.parent) as directory:
        replacement = Path(directory) / "root"
        build_distribution(stage, replacement, work, bootstrap(), signing_key())
        # The installer media's nested installation payload is a separate root.
        if (stage / "install").exists():
            (stage / "install").rename(replacement / "install")
        shutil.rmtree(stage)
        replacement.rename(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    args = parser.parse_args()
    build_distribution(args.source, args.output, args.work, bootstrap(), signing_key())


if __name__ == "__main__":
    main()
