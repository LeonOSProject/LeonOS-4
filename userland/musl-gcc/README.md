# Native Static Musl Toolchain

This external ABI test fixture uses the unchanged x86-64 binary distribution
from [Dyne musl 2.2.0](https://github.com/dyne/musl/releases/tag/2.2.0).
The archive SHA256 is
`31420e4f978e7ccbcc597ca5e18c2dcbe640ea6985ed6325112d8a171a72ece3`.
It contains GCC 15.1.0 (C/C++), binutils, musl headers and static libraries,
libgcc, libstdc++, and the additional sysroot shipped by Dyne. Its host
executables require x86-64-v2. Original contents live under `/opt/dyne`.

The isolated GCC probe adds `/usr/bin` aliases for `gcc`, `cc`, `musl-gcc`, `g++`, `c++`,
`musl-g++`, `cpp`, `as`, `ld`, `ar`, `ranlib`, `nm`, `objcopy`, `objdump`,
`readelf`, `strip`, the remaining supplied tools, and their target-prefixed
names. They locate the installed compiler and sysroot; no compiler source or
binary patches are applied. Compiler flags are forwarded unchanged, so request
static output explicitly, for example `musl-gcc -static hello.c -o hello`.
This standalone Linux toolchain does not replace the LeonOS GUI SDK and does
not link mimalloc into generated programs automatically.

GCC and binutils are no longer production components. Normal image and SDK
builds do not build, download or install this fixture. Future system installs
will use apk. To reproduce the independent regression fixture explicitly:

```sh
python3 tools/package_musl_gcc.py --archive buildsystem/deps/musl-gcc/dyne-gcc-musl-x86_64.tar.xz --out build/musl-gcc/root
python3 build.py run musl-sdk esp
python3 build/musl/sdk/bin/leonos-musl-cc -O2 -static tools/tests/gcc_guest_probe.c -o build/gcc-probe.elf
python3 tools/prepare_gcc_probe.py --runner build/gcc-probe.elf --toolchain build/musl-gcc/root --out build/gcc-probe
```

If the cache is missing, the packaging script downloads using the host's normal
network and optional proxy environment. An explicit `--source /path/to/archive.tar.xz` supplies an offline
archive, with the same checksum requirement. The probe uses its own staging
tree and never adds GCC back to the normal image.

The distribution's existing notices and documentation are preserved. Upstream
source and build recipes are available at https://github.com/dyne/musl/tree/2.2.0
(including its pinned GCC/binutils/musl build inputs). GCC and binutils are GPL
software; GCC runtime libraries carry the GCC Runtime Library Exception where
specified upstream. Musl uses the MIT license. Additional bundled libraries
retain their own licenses. `/usr/share/licenses/musl-gcc/package.json` records every
upstream file's SHA256 for comparison with the original archive.

The case-sensitive Linux headers require ext2 in the live/installer root as
well as on the installed system. The boot module keeps the legacy filename
`/install/root.fat` for boot configuration compatibility, but new media contain
ext2 and the kernel recognizes the filesystem magic. EFI boot remains FAT.
