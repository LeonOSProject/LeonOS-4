#!/bin/sh
# List the public build surface. Must run with no compiler installed and must
# not configure, download or build anything.
set -u

cat <<'HELP'
LeonOS 4 build system (GNU Make + C host tools)

  make                 same as `make help`; never downloads or builds
  make help            this message
  make doctor          check host tools, target toolchain and third-party bits
  make fetch           the only network stage: fetch and verify locked sources

  make defconfig       start $(O)/config/.config from configs/default.conf
  make olddefconfig    resolve new symbols non-interactively, keep user choices
  make menuconfig      edit the configuration of the selected output directory

  make tools           build this project's C host tools (HOSTCC only)
  make kernel          freestanding kernel image and debug symbols
  make userland        ring-3 programs and libraries
  make runtime         shared runtime objects
  make sdk             developer SDK (relocatable, no Python wrappers)
  make rootfs          staged root filesystem
  make apk-repo        signed local package repository
  make image-vmdk      bootable disk image
  make iso             live ISO
  make installer       installer ISO
  make all             kernel userland runtime sdk apk-repo image-vmdk iso installer

  make run             boot the VMDK in QEMU          (no root required)
  make run-iso         boot the live ISO in QEMU
  make run-installer   boot the installer ISO in QEMU

  make test-tools      C host tool tests, plain and under ASan/UBSan
  make test-build      shell contract tests for the build surface
  make test            test-tools plus test-build
  make test-smoke      QEMU boot tests (long; explicit)
  make test-legacy     pre-existing regression tests, some need Python

  make clean           remove built products in $(O), keep configuration
  make distclean       also remove configuration in $(O); keeps the download cache

Variables
  ARCH=x86_64          only x86_64 is supported today; anything else is an error
  PROFILE=debug|release  compile policy; anything else is an error
  O=PATH               output directory, default out/<arch>/<profile>
  V=1                  echo the real command, its working directory and overrides
  CPUS=n               guest virtual CPU count for the run targets
  MEMORY=4G            guest memory for the run targets
  TOOLCHAIN=FILE       toolchain description, default configs/toolchains/llvm-x86_64.mk
  CC/CXX/AR/LD/OBJCOPY/STRIP=  explicit target tool override (command line only)
  HOSTCC=              host compiler for the C helpers; independent of CC
  SOURCE_DATE_EPOCH=N  pin generated timestamps for reproducible output
  BUILD_ID=N           pin the numeric build id instead of deriving it

Environment values of ARCH, PROFILE, O and the target tools are ignored so an
unrelated shell setting cannot silently change what gets built.

Examples
  make doctor
  make fetch
  make defconfig PROFILE=debug
  make kernel PROFILE=debug -j8
  make installer PROFILE=release -j8
  make run CPUS=2 MEMORY=4G
  make kernel V=1
  make test-build

Diagnosing
  make --trace, make -n and make -p are the supported inspection tools.
  Note that `make -n` is not a side-effect-free sandbox: Make may rebuild
  generated files it includes and may run recursive rules. Plain help, doctor
  and clean never regenerate configuration or start a production build.

Third-party sources
  Git submodules under third_party/ must be initialised before any image build:
      git submodule update --init --recursive
HELP
