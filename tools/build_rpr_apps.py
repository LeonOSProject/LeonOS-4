#!/usr/bin/env python3
"""Build the optional official LeonOS application APKs for RPR."""
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

from apk_distribution import bootstrap, make_package, signing_key

ROOT = Path(__file__).resolve().parents[1]
VERSION_RE = re.compile(r'#define LEONOS_KERNEL_VERSION "([0-9]+\.[0-9]+\.[0-9]+)-([0-9]+)"')


def parse_version(header: Path) -> tuple[str, str]:
    match = VERSION_RE.search(header.read_text(encoding="utf-8"))
    if not match:
        raise RuntimeError(f"missing kernel version in {header}")
    image_version = match.group(1)
    build = match.group(2)
    return f"{image_version}-r{build}", f"{image_version}-{build}"


def write_manifest(path: Path, *, app_id: str, name: str, version: str,
                   category: str, executable: str, icon: str, entry: int,
                   commands: str, input_method: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "[app]\n"
        f"id={app_id}\n"
        f"name={name}\n"
        f"version={version}\n"
        f"category={category}\n"
        f"exec={executable}\n"
        f"icon={icon}\n"
        f"entry={entry}\n"
        "terminal=0\n"
        "system=0\n"
        "hidden=0\n"
        "open_with=0\n"
        f"commands={commands}\n"
        "extensions=\n"
        + ("input_method=1\n" if input_method else "")
        + "\n",
        encoding="utf-8",
        newline="\n",
    )


def symlink(payload: Path, relative: str, target: str) -> None:
    path = payload / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.symlink_to(target)


def package_helloworld(payload: Path, app_version: str, app_elf: Path,
                       icon: Path) -> None:
    root = payload / "usr/lib/leonos/apps/helloworld"
    root.mkdir(parents=True, exist_ok=True)
    shutil.copy2(app_elf, root / "helloworld.elf")
    shutil.copy2(icon, root / "helloworld.bmp")
    write_manifest(root / "manifest.ini", app_id="helloworld", name="Hello World",
                   version=app_version, category="Developer applications",
                   executable="helloworld.elf", icon="helloworld.bmp", entry=1,
                   commands="helloworld")
    symlink(payload, "usr/bin/helloworld", "../lib/leonos/apps/helloworld/helloworld.elf")


def package_doom(payload: Path, app_version: str, launcher: Path, doom: Path,
                 wad: Path, icon: Path, engine_license: Path, notice: Path) -> None:
    root = payload / "usr/lib/leonos/apps/doom"
    root.mkdir(parents=True, exist_ok=True)
    for source, name in ((launcher, "doomlauncher.elf"), (doom, "doom.elf"),
                         (wad, "freedoom1.wad"), (icon, "doom.bmp"),
                         (engine_license, "DOOMGENERIC-LICENSE"),
                         (notice, "FREEDOOM-COPYING.txt")):
        shutil.copy2(source, root / name)
    write_manifest(root / "manifest.ini", app_id="doom", name="DOOM",
                   version=app_version, category="Games", executable="doomlauncher.elf",
                   icon="doom.bmp", entry=1, commands="doom,doomlauncher")
    symlink(payload, "usr/bin/doom", "../lib/leonos/apps/doom/doomlauncher.elf")
    symlink(payload, "usr/bin/doomlauncher", "../lib/leonos/apps/doom/doomlauncher.elf")


def package_oschinpt(payload: Path, app_version: str, app_elf: Path, dictionary: Path,
                     index: Path, settings: Path, license_file: Path,
                     attribution: Path, post_install: Path,
                     post_deinstall: Path) -> tuple[tuple[str, Path], ...]:
    root = payload / "usr/lib/leonos/apps/oschinpt"
    root.mkdir(parents=True, exist_ok=True)
    for source, name in ((app_elf, "oschinpt.elf"), (dictionary, "pinyin_simp.dict.yaml"),
                         (index, "oscp.idx"), (settings, "settings.ini"),
                         (license_file, "LICENSE"), (attribution, "ATTRIBUTION.txt")):
        shutil.copy2(source, root / name)
    write_manifest(root / "manifest.ini", app_id="oschinpt", name="LeonOS 4 Chinese Input",
                   version=app_version, category="Input methods", executable="oschinpt.elf",
                   icon="", entry=0, commands="oschinpt", input_method=True)
    symlink(payload, "usr/bin/oschinpt", "../lib/leonos/apps/oschinpt/oschinpt.elf")
    scripts = (("post-install", post_install), ("post-upgrade", post_install),
               ("post-deinstall", post_deinstall))
    return scripts


def build(output: Path, build_info: Path, app_elfs: dict[str, Path], icon_dir: Path,
          oschinpt_index: Path) -> None:
    apk = bootstrap()
    key = signing_key()
    package_version, app_version = parse_version(build_info)
    if output.exists():
        if output.is_symlink() or not output.is_dir():
            raise RuntimeError(f"refusing to replace non-directory output: {output}")
        shutil.rmtree(output)
    output.mkdir(parents=True)
    work = output / ".work"
    work.mkdir()
    common_depends = ("leonos-apps", "leonos-musl")
    packages: list[Path] = []

    payload = work / "helloworld"
    package_helloworld(payload, app_version, app_elfs["helloworld"],
                       icon_dir / "helloworld.bmp")
    packages.append(make_package(apk, key, payload, output / "leonos-helloworld.apk",
                                 "leonos-helloworld", package_version, common_depends))

    payload = work / "doom"
    package_doom(payload, app_version, app_elfs["doomlauncher"], app_elfs["doom"],
                 ROOT / "third_party/doomgeneric/freedoom1.wad", icon_dir / "doom.bmp",
                 ROOT / "third_party/doomgeneric/LICENSE",
                 ROOT / "third_party/doomgeneric/FREEDOOM-COPYING.txt")
    packages.append(make_package(apk, key, payload, output / "leonos-doom.apk",
                                 "leonos-doom", package_version, common_depends))

    payload = work / "oschinpt"
    scripts = package_oschinpt(
        payload, app_version, app_elfs["oschinpt"],
        ROOT / "third_party/rime-pinyin-simp/pinyin_simp.dict.yaml", oschinpt_index,
        ROOT / "userland/apps/oschinpt/settings.ini",
        ROOT / "third_party/rime-pinyin-simp/LICENSE",
        ROOT / "third_party/rime-pinyin-simp/ATTRIBUTION.txt",
        ROOT / "tools/oschinpt-apk-post-install",
        ROOT / "tools/oschinpt-apk-post-deinstall",
    )
    packages.append(make_package(apk, key, payload, output / "leonos-oschinpt.apk",
                                 "leonos-oschinpt", package_version, common_depends,
                                 scripts=scripts))
    (output / "packages.list").write_text("\n".join(path.name for path in packages) + "\n",
                                           encoding="ascii")
    shutil.rmtree(work)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build-info", required=True, type=Path)
    parser.add_argument("--icon-dir", required=True, type=Path)
    parser.add_argument("--oschinpt-index", required=True, type=Path)
    parser.add_argument("--helloworld", required=True, type=Path)
    parser.add_argument("--doom", required=True, type=Path)
    parser.add_argument("--doomlauncher", required=True, type=Path)
    parser.add_argument("--oschinpt", required=True, type=Path)
    args = parser.parse_args()
    build(args.output, args.build_info, {
        "helloworld": args.helloworld,
        "doom": args.doom,
        "doomlauncher": args.doomlauncher,
        "oschinpt": args.oschinpt,
    }, args.icon_dir, args.oschinpt_index)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
