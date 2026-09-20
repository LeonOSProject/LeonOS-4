#!/usr/bin/env python3
"""Pack the normal desktop payload in a bootable ext2 ramdisk."""
import argparse
from pathlib import Path
import tempfile

from leonos_layout import layout_directories, apply_root_symlinks

from make_image import make_root_tree
from make_ext2_root import write_ext2_root
from make_installer_root import share_identical_payload_files


def make_live_tree(tree: Path, stage: Path) -> None:
    """Use exactly the same applications and data as the installed system."""
    make_root_tree(tree, stage, "en")
    layout_directories(stage)
    apply_root_symlinks(stage)
    share_identical_payload_files(stage)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tree", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="leonos-live-tree-", dir=args.out.parent) as directory:
        stage = Path(directory)
        make_live_tree(args.tree, stage)
        write_ext2_root(stage, args.out)


if __name__ == "__main__":
    main()
