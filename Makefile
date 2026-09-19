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
CPUS ?=
MEMORY ?=
SOURCE_DATE_EPOCH ?= $(shell git -C $(LEONOS_SRC) show -s --format=%ct HEAD)
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
# Where upstream build directories live. Not in the plan's list of output
# subdirectories, but an out-of-tree upstream build needs somewhere to write,
# and `obj/` is this project's own objects.
O_THIRD_PARTY := $(O)/third-party
# Staged root filesystem fragment for the upstream authentication libraries
# (Linux-PAM, libxcrypt, ...). Same status as third-party: a layout extension,
# because these packages install into their own DESTDIR rather than a flat
# object tree.
O_AUTH := $(O)/auth

# Shared download cache: deliberately outside O because it is profile
# independent and `distclean` must not throw it away.
LEONOS_CACHE := $(LEONOS_SRC)/cache/downloads

# Written when an output tree is created and re-checked before anything is
# deleted, so `clean` can never operate on a directory it does not own.
LEONOS_O_MARKER := $(O)/.leonos-out

# --- same-output-directory mutual exclusion ---------------------------------
# Two top-level makes sharing one O would race on objects, generated headers and
# signature files, so one of them has to be refused outright (plan section 6.3).
# Different O directories are independent and may build concurrently.
#
# The owner is the make process that acquired the lock, and the token names the
# output directory it owns: LEONOS_BUILD_OWNER is "<pid>.<start ticks>:<absolute O>".
# A nested make for that same directory inherits it, so recursive build
# invocations do not refuse their own outer build. A nested
# make for a different directory acquires its own lock, so a test suite that
# spawns builds still gets real exclusion for the trees it creates.
#
# Three cases skip acquisition: dry runs and `make -q` (they promise no output,
# and refusing them would make `make -n` depend on unrelated builds), and goals
# that write nothing at all -- a bare `make` and `make help` must keep working
# while a build is running elsewhere, and must not create the output tree.
leonos_read_only_goals := help doctor
leonos_lock_not_needed :=
ifeq ($(MAKECMDGOALS),)
leonos_lock_not_needed := 1
else ifeq ($(words $(filter $(leonos_read_only_goals),$(MAKECMDGOALS))),$(words $(MAKECMDGOALS)))
leonos_lock_not_needed := 1
endif
leonos_lock_dir := $(O)/.build-lock
leonos_lock_skip := \
	$(if $(findstring n,$(firstword -$(MAKEFLAGS))),1)\
	$(if $(findstring q,$(firstword -$(MAKEFLAGS))),1)\
	$(leonos_lock_not_needed)
ifeq ($(strip $(leonos_lock_skip)),)
LEONOS_BUILD_OWNER := $(shell sh $(LEONOS_SRC)/scripts/build-lock.sh acquire \
	$(leonos_lock_dir) $(leonos_O_absolute))
ifeq ($(LEONOS_BUILD_OWNER),)
$(error refusing to build: '$(O)' is already being built by another make)
endif
export LEONOS_BUILD_OWNER
endif

# --- fragment includes ------------------------------------------------------
include $(LEONOS_SRC)/mk/host.mk
include $(LEONOS_SRC)/mk/toolchain.mk
include $(LEONOS_SRC)/mk/config.mk
include $(LEONOS_SRC)/mk/kernel.mk
include $(LEONOS_SRC)/mk/boot.mk
include $(LEONOS_SRC)/mk/third-party.mk
include $(LEONOS_SRC)/mk/pam.mk
include $(LEONOS_SRC)/mk/runtime.mk
include $(LEONOS_SRC)/mk/upstream.mk
include $(LEONOS_SRC)/mk/userland.mk
include $(LEONOS_SRC)/mk/resources.mk
include $(LEONOS_SRC)/mk/sdk.mk
include $(LEONOS_SRC)/mk/rootfs.mk
include $(LEONOS_SRC)/mk/apk.mk
include $(LEONOS_SRC)/mk/images.mk
include $(LEONOS_SRC)/mk/rpr.mk
include $(LEONOS_SRC)/mk/run.mk
include $(LEONOS_SRC)/mk/tests.mk

# --- public goals -----------------------------------------------------------
.DEFAULT_GOAL := help
# Source inventories are inputs, never implicit host executable targets.
.SUFFIXES:

.PHONY: help doctor fetch defconfig olddefconfig menuconfig tools \
	kernel userland runtime sdk rootfs apk-repo image-vmdk iso installer all \
	run run-iso run-installer test test-tools test-build test-long test-smoke \
	test-legacy clean distclean

help:
	@V='$(V)' O='$(O)' ARCH='$(ARCH)' PROFILE='$(PROFILE)' CPUS='$(CPUS)' \
	TOOLCHAIN='$(TOOLCHAIN)' SRC='$(LEONOS_SRC)' CACHE='$(LEONOS_CACHE)' \
	LOCK='$(LEONOS_LOCK)' sh $(LEONOS_SRC)/scripts/help.sh

doctor:
	@SRC='$(LEONOS_SRC)' O='$(O)' ARCH='$(ARCH)' PROFILE='$(PROFILE)' \
	TOOLCHAIN='$(TOOLCHAIN)' HOSTCC='$(HOSTCC)' \
	TARGET_CC='$(TARGET_CC)' TARGET_LD='$(TARGET_LD)' TARGET_AR='$(TARGET_AR)' \
	TARGET_OBJCOPY='$(TARGET_OBJCOPY)' TARGET_STRIP='$(TARGET_STRIP)' \
	TARGET_RUSTC='$(TARGET_RUSTC)' TARGET_TRIPLE_KERNEL='$(TRIPLE_KERNEL)' \
	TARGET_TRIPLE_USER='$(TRIPLE_USER)' \
	DEPS='$(LEONOS_DEPS_TOOL)' LOCK='$(LEONOS_LOCK)' CACHE='$(LEONOS_CACHE)' \
	sh $(LEONOS_SRC)/scripts/doctor.sh

defconfig olddefconfig menuconfig: $(LEONOS_O_MARKER) $(KCONFIG_CONF) $(KCONFIG_MCONF) | $(O_CONFIG)
	$(Q)sh $(LEONOS_SRC)/tools/build/kconfig-frontends.sh run \
		--conf $(abspath $(KCONFIG_CONF)) --mconf $(abspath $(KCONFIG_MCONF)) \
		--kconfig $(KCONFIG_ROOT) --config $(abspath $(LEONOS_CONFIG_FILE)) \
		--seed $(KCONFIG_SEED) --mode $@

tools: $(LEONOS_HOST_TOOLS)

kernel: $(LEONOS_KERNEL_SYS) $(LEONOS_KERNEL_DEBUG)

test: test-tools test-build

clean:
	@O='$(O)' SRC='$(LEONOS_SRC)' KEEP_CONFIG=1 sh $(LEONOS_SRC)/scripts/clean.sh

distclean:
	@O='$(O)' SRC='$(LEONOS_SRC)' KEEP_CONFIG=0 sh $(LEONOS_SRC)/scripts/clean.sh

test-smoke: image-vmdk iso installer
	@QEMU='$(QEMU)' sh $(LEONOS_SRC)/scripts/test-smoke.sh $(O_IMAGES) $(O_LOGS) '$(QEMU_FIRMWARE)'

test-legacy:
	@sh $(LEONOS_SRC)/scripts/test-legacy.sh $(LEONOS_SRC)

rootfs: apk-repo

all: kernel userland runtime sdk apk-repo image-vmdk iso installer

.PHONY: image-iso release config-sync build-info test-all
image-iso: iso
release: all rpr-pages
config-sync: $(AUTOCONF_H) $(AUTOCONF_INSTALLER_H) $(RUSTCFG_ARGS) $(LEONOS_COMPONENT_MK)
build-info: $(BUILD_INFO_HEADER)
test-all: test test-long test-legacy test-smoke
