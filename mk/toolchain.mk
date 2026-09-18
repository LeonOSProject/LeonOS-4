# Target toolchain selection.
#
# The production compile and link rules use TARGET_* below, never CC or LD
# directly. GNU Make predefines CC=cc with origin "default", so `CC ?= clang`
# would silently do nothing and a rule reading $(CC) would build the host
# architecture into a freestanding kernel image (plan section 6.2).

ifeq ($(origin TOOLCHAIN),command line)
LEONOS_TOOLCHAIN_ORIGIN := command line
else
LEONOS_TOOLCHAIN_ORIGIN := default
endif

$(if $(wildcard $(TOOLCHAIN)),,$(error toolchain description '$(TOOLCHAIN)' not found (set via $(LEONOS_TOOLCHAIN_ORIGIN))))

include $(TOOLCHAIN)

# Only a command-line override may displace the description file. An
# environment-provided CC, LD or AR is deliberately ignored so that whatever
# `make doctor` validated is what actually runs.
leonos_take_override = $(if $(filter command line,$(origin $(2))),$($(2)),$($(1)))

TARGET_CC      := $(call leonos_take_override,TOOLCHAIN_CC,CC)
TARGET_CXX     := $(call leonos_take_override,TOOLCHAIN_CXX,CXX)
TARGET_AR      := $(call leonos_take_override,TOOLCHAIN_AR,AR)
TARGET_LD      := $(call leonos_take_override,TOOLCHAIN_LD,LD)
TARGET_OBJCOPY := $(call leonos_take_override,TOOLCHAIN_OBJCOPY,OBJCOPY)
TARGET_STRIP   := $(call leonos_take_override,TOOLCHAIN_STRIP,STRIP)
TARGET_RUSTC   := $(call leonos_take_override,TOOLCHAIN_RUSTC,RUSTC)

# Which of the two the value came from, for `make V=1` and the signatures.
leonos_override_note = $(if $(filter command line,$(origin $(2))),overridden-from-command-line,$($(1)))

# A profile is a compile policy, so it belongs here rather than being copied
# into C constants or scattered through the rules (plan section 7).
ifeq ($(PROFILE),debug)
	LEONOS_OPTIMIZATION_FLAGS := -O0 -g
	LEONOS_LINK_POLICY_FLAGS :=
else
	LEONOS_OPTIMIZATION_FLAGS := -O3
	LEONOS_LINK_POLICY_FLAGS := --strip-all
endif

# Explicit per-class override variables from the command line remain available
# for experiments; leaving them unset keeps the profile policy in charge.
KERNEL_CFLAGS ?=
KERNEL_AFLAGS ?=
KERNEL_LDFLAGS ?=
