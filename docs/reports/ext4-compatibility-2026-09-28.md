# ReliefOS ext4 compatibility report

Date: 2026-09-29
Branch: `xiaobai/ext4-development`
Kernel commit: `c6835eeb156ac4a76d523c13c26c6f3643ebabf0`
Outer repository commit before task 15: `0d2adb6fb13b4ea13f7fd05d22667b23d9e41be6`

The implementation targets the agreed basic ext4 scope. It reads and writes
normal Linux ext4 images using 1 KiB, 2 KiB, and 4 KiB blocks, extents, 64-bit
groups, flex_bg, metadata checksums, dir_index, xattrs, ordered metadata
journaling, fallocate/punch-hole/zero-range, timestamps, statfs/statx data,
and Linux-compatible inode and directory records. The generated root image uses
the explicit profile `extents,dir_index,metadata_csum,64bit,flex_bg,has_journal,
large_file,huge_file,extra_isize`.

## Verification

| Gate | Command or fixture | Result |
| --- | --- | --- |
| Feature schema | `python3 tools/tests/ext4_feature_matrix.py --check-schema` | PASS, 24 rows checked against `linux/fs/ext4/ext4.h` |
| Linux to ReliefOS | `ext4_interop_test.py --direction linux-to-reliefos --mode fixture` | PASS |
| ReliefOS to Linux | `ext4_interop_test.py --direction reliefos-to-linux --mode fixture` | PASS; e2fsck/debugfs oracle |
| Native read/write | `test_ext4_read.py --case all`, `test_ext4_write.py` | PASS for 1K/2K/4K images |
| Directory and VFS | `test_ext4_dir.py --count 10000`, `test_ext4_vfs.py --filesystem ext4` | PASS; Linux HTREE and e2fsck checks |
| Metadata/xattrs | `ext4_metadata_api_test.py --case all`, `test_storage_metadata.py` | PASS; inline and external user xattrs |
| Crash replay | `ext4_crash_replay_test.py --mode host-fixture --iterations 32` | PASS, 384 profile/block replay combinations, zero failures |
| Build | `make -j4 kernel`, `make -j4 userland`, `make -j4 image-vmdk` | PASS |
| Root image | `out/x86_64/release/images/disk-root.ext4` + `e2fsck -f -n` | PASS, all five e2fsck passes |
| Runtime tools | `test_storage_upstream_runtime.py --filesystem ext4` | PASS, 4 tests |
| Performance | `ext4-performance-2026-09-28.json` | PASS; all ext2-relative gates true |

The performance fixture measured 458.113 MiB/s sequential read, 274791.105
KiB/s random 4 KiB read, and 283.729 small-file creates per second for ext4.
Relative to the same ext2 fixture, the ratios were 0.9911, 1.3349, and 1.1188.
The performance gate requires at least 0.80 sequential, 0.70 random, and no
more than 2x the ext2 small-file time.

The QEMU upstream guest booted the generated image with
`boot complete: ... fs=ext4` and logged `ext4 mount ok`; the storage, tmpfs,
block, formatter, fsck, and boot-payload checks passed. The guest harness still
reports one diagnostic failure, `mount listing`, because `/bin/mount` cannot
read `/proc/mounts` and exits with `I/O error`. This does not affect the ext4
root mount or the other guest checks, but it remains an open runtime integration
item and is intentionally not marked as a pass.

## Scope boundary

The basic interoperability target does not claim Linux's advanced ext4
features. The driver rejects `bigalloc`, `inline_data`, `casefold`, encryption,
verity, quota/project quota, fast_commit, MMP, and unsupported incompatible
feature bits without modifying the volume. DAX and related advanced VFS APIs
return `-EOPNOTSUPP`. Direct-I/O, iomap, atomic write, snapshots, online
resize, external journals, multi-device RAID, and the Linux FIEMAP ioctl are
outside this delivery. Native kernel metadata APIs are available; a complete
Linux `getxattr`/`setxattr` syscall surface is not part of this milestone.
