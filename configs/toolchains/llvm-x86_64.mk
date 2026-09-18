# LeonOS 4 x86_64 target toolchain description.
#
# Reviewed data, not logic: mk/toolchain.mk interprets it and `make doctor`
# verifies it. Switching toolchains means pointing TOOLCHAIN= at another file in
# this directory, never at an undeclared environment variable.

TOOLCHAIN_NAME := llvm-x86_64

# The kernel, the boot loader and the middlelayer link at fixed addresses with
# ld.lld; userland targets x86_64-linux-musl against the project sysroot.
TRIPLE_KERNEL := x86_64-unknown-none
TRIPLE_USER := x86_64-linux-musl

TOOLCHAIN_CC := clang
TOOLCHAIN_CXX := clang++
TOOLCHAIN_AR := llvm-ar
TOOLCHAIN_LD := ld.lld
TOOLCHAIN_OBJCOPY := llvm-objcopy
TOOLCHAIN_STRIP := llvm-strip
TOOLCHAIN_RUSTC := rustc

# Checked by `make doctor`: the middlelayer is Rust plus C, and choosing C for
# the host helper tools did not remove that dependency.
TOOLCHAIN_REQUIRED := $(TOOLCHAIN_CC) $(TOOLCHAIN_CXX) $(TOOLCHAIN_AR) \
	$(TOOLCHAIN_LD) $(TOOLCHAIN_OBJCOPY) $(TOOLCHAIN_STRIP) $(TOOLCHAIN_RUSTC)
