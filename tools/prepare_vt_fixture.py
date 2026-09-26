#!/usr/bin/env python3
"""Create an exclusive installed-image copy containing the real VT ABI probe."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def prepare(source, output, text_only=False):
    output.mkdir(parents=True, exist_ok=False)
    probe = output / 'vt-probe'
    compiler = ROOT / 'out/x86_64/release/sdk/leonos-musl-sdk/bin/leonos-musl-cc'
    subprocess.run([str(compiler), '-D_GNU_SOURCE', '-static', '-Iinclude', '-Ikernel/ntclks/include/uapi',
                    'tools/tests/vt_guest_test.c', '-o', str(probe)], cwd=ROOT, check=True)
    disk = output / 'disk.raw'
    subprocess.run(['cp', '--reflink=auto', '--sparse=always', str(source), str(disk)], check=True)
    layout = json.loads(subprocess.check_output(['sfdisk', '--json', str(disk)]))['partitiontable']
    root = layout['partitions'][1]
    offset, length = root['start'] * layout['sectorsize'], root['size'] * layout['sectorsize']
    fs = output / 'root.ext2'
    with disk.open('rb') as src, fs.open('wb') as dst:
        src.seek(offset)
        remaining = length
        while remaining:
            chunk = src.read(min(16 * 1024**2, remaining))
            if not chunk: raise RuntimeError('Truncated root partition')
            dst.write(chunk)
            remaining -= len(chunk)
    commands = [f'write {probe.resolve()} /tmp/vt-probe', 'set_inode_field /tmp/vt-probe mode 0100755']
    if text_only: commands.append('rm /etc/leonos/desktop-session')
    for command in commands:
        subprocess.run(['debugfs', '-w', '-R', command, str(fs)], check=True)
    with fs.open('rb') as src, disk.open('r+b') as dst:
        dst.seek(offset)
        shutil.copyfileobj(src, dst, 16 * 1024**2)
    return disk

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, default=ROOT / 'out/x86_64/release/images/leonos4.raw')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--text-only', action='store_true')
    args = parser.parse_args()
    print(prepare(args.image, args.output, args.text_only))
