# LeonOS 4 build entry point.
#
# GNU Make owns the dependency graph, the parallel schedule and the incremental
# decisions. C helpers under tools/host/ perform data transforms only; short
# scripts under tools/build/ drive third-party builds. Nothing here shells out to
# a second scheduler, and no production path runs Python, Meson or Ninja.
#
# Contract: docs/superpowers/plans/2026-09-19-make-c-build-rewrite.md

# --- GNU Make version ------------------------------------------------------
# Grouped targets ('&:') and the $(file) function both need 4.3.
leonos_make_min := $(shell printf '4.3\n$(MAKE_VERSION)\n' | LC_ALL=C sort -V | head -n1)
ifeq ($(leonos_make_min),4.3)
else
$(error GNU Make >= 4.3 is required, this is $(MAKE_VERSION))
endif

LEONOS_SRC := $(patsubst %/,%,$(dir $(realpath $(firstword $(MAKEFILE_LIST)))))

# --- user-facing variables --------------------------------------------------
# ARCH, PROFILE and O may come from the command line or from these defaults
# only. An inherited environment value is ignored on purpose: an unrelated shell
# setting must not silently change what gets built (plan section 6.2).
ifeq ($(origin ARCH),undefined)
ARCH := x86_64
endif
ifeq ($(origin PROFILE),undefined)
PROFILE := release
endif
ifeq ($(origin O),undefined)
O := $(LEONOS_SRC)/out/$(ARCH)/$(PROFILE)
endif

V ?= 0
CPUS ?= $(shell nproc 2>/dev/null || echo 1)
MEMORY ?=
SOURCE_DATE_EPOCH ?=
BUILD_ID ?=
TOOLCHAIN ?= $(LEONOS_SRC)/configs/toolchains/llvm-x86_64.mk

O := $(patsubst %/,%,$(O))

# --- input validation -------------------------------------------------------
# The supported character set is enumerated rather than promising arbitrary
# paths: Make word splitting, shell quoting and the generated manifests all break
# on the rejected set, so failing here is far cheaper than failing mid-build.
LEONOS_ALLOWED_CHARS := a b c d e f g h i j k l m n o p q r s t u v w x y z \
	A B C D E F G H I J K L M N O P Q R S T U V W X Y Z \
	0 1 2 3 4 5 6 7 8 9 . _ / -

# $(call strip_allowed,text,chars): keep only characters outside the allow-list.
strip_allowed = $(if $(2),$(call strip_allowed,$(subst $(firstword $(2)),,$(1)),$(wordlist 2,9999,$(2))),$(1))

ifeq ($(O),)
$(error O= must not be empty; it names this build's output directory)
endif
ifneq ($(words $(O)),1)
$(error O='$(O)' is unsupported: whitespace and newlines are not accepted in an output path)
endif
leonos_O_residual := $(call strip_allowed,$(O),$(LEONOS_ALLOWED_CHARS))
ifneq ($(leonos_O_residual),)
$(error O='$(O)' is unsupported: offending characters are [$(leonos_O_residual)]; accepted are A-Z a-z 0-9 . _ / -)
endif
# Compare absolute paths: `O=.` and `O=<src>` are the same refusal. O has already
# been restricted to a safe character set above, so quoting here cannot be escaped.
leonos_O_absolute := $(shell realpath -m -- '$(O)' 2>/dev/null || printf '%s' '$(O)')
ifeq ($(leonos_O_absolute),/)
$(error refusing / as the output directory)
endif
ifeq ($(leonos_O_absolute),$(LEONOS_SRC))
$(error refusing the source root as the output directory (O='$(O)'))
endif
ifneq ($(filter $(ARCH),x86_64),)
else
$(error unsupported ARCH '$(ARCH)'; this build supports ARCH=x86_64)
endif
ifneq ($(filter $(PROFILE),debug release),)
else
$(error unsupported PROFILE '$(PROFILE)'; use PROFILE=debug or PROFILE=release)
endif

# --- output layout ----------------------------------------------------------
O_HOST      := $(O)/host
O_OBJ       := $(O)/obj
O_GENERATED := $(O)/generated
O_INCLUDE   := $(O)/include
O_CONFIG    := $(O)/config
O_SYSROOT   := $(O)/sysroot
O_STAGE     := $(O)/stage
O_PACKAGES  := $(O)/packages
O_IMAGES    := $(O)/images
O_LOGS      := $(O)/logs
O_META      := $(O)/meta

# Shared download cache: deliberately outside O because it is profile
# independent and `distclean` must not throw it away.
LEONOS_CACHE := $(LEONOS_SRC)/cache/downloads

# Written when an output tree is created and re-checked before anything is
# deleted, so `clean` can never operate on a directory it does not own.
LEONOS_O_MARKER := $(O)/.leonos-out

# --- fragment includes ------------------------------------------------------
include $(LEONOS_SRC)/mk/host.mk
include $(LEONOS_SRC)/mk/toolchain.mk
include $(LEONOS_SRC)/mk/config.mk
include $(LEONOS_SRC)/mk/kernel.mk
include $(LEONOS_SRC)/mk/tests.mk

# --- public goals -----------------------------------------------------------
.DEFAULT_GOAL := help

.PHONY: help doctor fetch defconfig olddefconfig menuconfig tools \
	kernel userland runtime sdk rootfs apk-repo image-vmdk iso installer all \
	run run-iso run-installer test test-tools test-build test-smoke test-legacy \
	clean distclean

help:
	@V='$(V)' O='$(O)' ARCH='$(ARCH)' PROFILE='$(PROFILE)' CPUS='$(CPUS)' \
	TOOLCHAIN='$(TOOLCHAIN)' SRC='$(LEONOS_SRC)' sh $(LEONOS_SRC)/scripts/help.sh

doctor:
	@SRC='$(LEONOS_SRC)' O='$(O)' ARCH='$(ARCH)' PROFILE='$(PROFILE)' \
	TOOLCHAIN='$(TOOLCHAIN)' HOSTCC='$(HOSTCC)' \
	TARGET_CC='$(TARGET_CC)' TARGET_LD='$(TARGET_LD)' TARGET_AR='$(TARGET_AR)' \
	TARGET_OBJCOPY='$(TARGET_OBJCOPY)' TARGET_STRIP='$(TARGET_STRIP)' \
	TARGET_RUSTC='$(TARGET_RUSTC)' TARGET_TRIPLE_KERNEL='$(TRIPLE_KERNEL)' \
	TARGET_TRIPLE_USER='$(TRIPLE_USER)' \
	sh $(LEONOS_SRC)/scripts/doctor.sh

fetch:
	@sh $(LEONOS_SRC)/scripts/not-migrated.sh fetch "P2/P3 (dependency lock file and offline cache)"

defconfig olddefconfig menuconfig: $(LEONOS_O_MARKER)
	@$(MAKE) --no-print-directory $(LEONOS_CONFIG_FILE) LEONOS_KCONFIG_MODE=$@

tools: $(LEONOS_HOST_TOOLS)

kernel: $(LEONOS_KERNEL_SYS) $(LEONOS_KERNEL_DEBUG)

test: test-tools test-build

clean:
	@O='$(O)' SRC='$(LEONOS_SRC)' KEEP_CONFIG=1 sh $(LEONOS_SRC)/scripts/clean.sh

distclean:
	@O='$(O)' SRC='$(LEONOS_SRC)' KEEP_CONFIG=0 sh $(LEONOS_SRC)/scripts/clean.sh

# Goals that land in later phases fail loudly. A stub that exits 0 would look
# green while shipping nothing (plan section 8).
userland runtime sdk rootfs apk-repo image-vmdk iso installer run run-iso \
run-installer test-smoke test-legacy:
	@sh $(LEONOS_SRC)/scripts/not-migrated.sh $@ "a later phase of the Make+C rebuild"

all: kernel
	@sh $(LEONOS_SRC)/scripts/not-migrated.sh all "a later phase (userland, sdk, apk-repo, images)"
