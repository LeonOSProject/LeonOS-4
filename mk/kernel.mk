# Kernel products: adapter over the standalone ntclks kernel checkout.
#
# Phase 3 of the kernel/userland separation: this repository no longer compiles
# any kernel, driver or boot-loader source. The standalone checkout named by
# NTCLKS_DIR owns those sources and builds kernel.sys, kernel.debug,
# kerneldebug.sys, loader.elf and the five .drv files; this fragment drives that
# build and publishes the results to the legacy locations the rest of the parent
# build (rootfs, images, rpr) consumes. The parent consumes only the sub-build's
# `install` output and its exported headers (mk/headers.mk).
#
# The sub-build is asked on every invocation. A stamp keyed on a git SHA would
# miss local edits in the checkout, so incrementality is the sub-make's own
# decision (its signatures and depfiles); when it has nothing to do the ask is
# cheap. Publishing goes through leonos-emit, so an unchanged product never
# moves mtime and never rebuilds its consumers.

# The kernel checkout. Since phase 5 the default path is the kernel/ntclks
# git submodule (github.com/LeonOSProject/NTCLKS); when it is not initialized
# the adapter fails at recipe time with instructions instead of at parse time
# (see the rule below).
NTCLKS_DIR ?= $(LEONOS_SRC)/kernel/ntclks
# Sub-build output directory: the checkout writes everything under O.
NTCLKS_O ?= $(O)/ntclks
# DESTDIR for the sub-make's `install`; the published products are copied out
# of here.
NTCLKS_DEST := $(O)/kernel-install

# Legacy product locations (kept stable for mk/rootfs.mk, mk/images.mk,
# mk/rpr.mk and mk/boot.mk).
LEONOS_KERNEL_SYS := $(O_GENERATED)/system/kernel.sys
LEONOS_KERNEL_DEBUG := $(O_GENERATED)/system/kernel.debug
NTCLKS_LOADER_ELF := $(O_GENERATED)/boot/loader.elf
NTCLKS_KERNELDEBUG_SYS := $(O_GENERATED)/system/kerneldebug.sys
NTCLKS_DRIVER_NAMES := mouse serial e1000 ac97 es1371
NTCLKS_DRIVER_OUTPUTS := $(addprefix $(O_GENERATED)/drivers/,$(addsuffix .drv,$(NTCLKS_DRIVER_NAMES)))

# "legacy path below generated/:installed file name" per product; the sub-make's
# install writes the nine products flat under $(NTCLKS_DEST).
NTCLKS_PUBLISH_PAIRS := system/kernel.sys:kernel.sys system/kernel.debug:kernel.debug \
	system/kerneldebug.sys:kerneldebug.sys boot/loader.elf:loader.elf \
	drivers/mouse.drv:mouse.drv drivers/serial.drv:serial.drv \
	drivers/e1000.drv:e1000.drv drivers/ac97.drv:ac97.drv \
	drivers/es1371.drv:es1371.drv

NTCLKS_PUBLISHED := $(LEONOS_KERNEL_SYS) $(LEONOS_KERNEL_DEBUG) \
	$(NTCLKS_KERNELDEBUG_SYS) $(NTCLKS_LOADER_ELF) $(NTCLKS_DRIVER_OUTPUTS)

# An explicit command-line tool override is part of the caller's intent and is
# passed to the sub-make as well (the checkout accepts the same CC/CXX/AR/
# RANLIB/LD/OBJCOPY/STRIP overrides). Everything else stays the checkout's own
# toolchain choice; only ARCH, PROFILE and SOURCE_DATE_EPOCH are pinned here.
NTCLKS_TOOL_PASSTHRU := $(strip $(foreach tool,CC CXX AR RANLIB LD OBJCOPY STRIP, \
	$(if $(filter command line,$(origin $(tool))),$(tool)=$(strip $($(tool))))))

# --- the delegation and publish rule -----------------------------------------
# Grouped targets: one recipe builds and installs the whole kernel product set
# in the sub-build, then publishes every product to its legacy path. FORCE, not
# a stamp: the checkout may carry local edits no recorded identity would notice.
#
# The first recipe line must stay a single command after its guard so the shell
# `exec`s the sub-make: it then is a direct child of this make and dies with it
# on an interrupt, instead of surviving as an orphan that would hold the
# sub-build's output lock. The guard keeps `make -n` a promise of no output:
# recipe lines that contain $(MAKE) run even under -n.
#
# Order-only on the exported-header manifest: mk/headers.mk drives the same
# sub-build output directory for headers_install, and the checkout's build lock
# refuses two concurrent makes on one O, so the (cheap) header delegation runs
# first and the product delegation never overlaps it. Nothing else waits: the
# userland build is gated on the header export, not on kernel.sys.
$(NTCLKS_PUBLISHED) &: FORCE $(LEONOS_EMIT) | $(O)/kernel-export/manifest.txt
	+$(Q)case "$${MAKEFLAGS%% *}" in *n*) exit 0 ;; esac; \
	if [ ! -f '$(NTCLKS_DIR)/Makefile' ]; then \
	    printf '%s\n' \
	        'ntclks adapter: kernel checkout not found: $(NTCLKS_DIR)/Makefile' \
	        '' \
	        'The kernel products are built by the ntclks kernel checkout (the' \
	        'kernel/ntclks git submodule since phase 5). Initialize it with' \
	        '`git submodule update --init --recursive` and run' \
	        '`make -C kernel/ntclks fetch`, or point NTCLKS_DIR at an existing' \
	        'checkout, e.g. NTCLKS_DIR=/path/to/ntclks or NTCLKS_DIR=.' >&2; \
	    exit 1; \
	fi; \
	exec $(MAKE) -C '$(NTCLKS_DIR)' O='$(NTCLKS_O)' ARCH='$(ARCH)' \
	    PROFILE='$(PROFILE)' SOURCE_DATE_EPOCH='$(or $(SOURCE_DATE_EPOCH),0)' \
	    $(NTCLKS_TOOL_PASSTHRU) all install DESTDIR='$(NTCLKS_DEST)'
	$(Q)set -eu; \
	test -f $(NTCLKS_DEST)/manifest.txt || { \
	    echo "ntclks adapter: $(NTCLKS_DEST)/manifest.txt missing after install" >&2; \
	    exit 1; }; \
	for pair in $(NTCLKS_PUBLISH_PAIRS); do \
	    rel=$${pair%%:*}; name=$${pair#*:}; \
	    test -f $(NTCLKS_DEST)/$$name || { \
	        echo "ntclks adapter: installed product $$name missing from $(NTCLKS_DEST)" >&2; \
	        exit 1; }; \
	    mkdir -p $(O_GENERATED)/$${rel%%/*}; \
	    $(LEONOS_EMIT) --input $(NTCLKS_DEST)/$$name --output $(O_GENERATED)/$$rel; \
	done

# --- the parent-owned version header -----------------------------------------
# build_info.h is not a kernel product: the parent generates it for its own
# version consumers (mk/rpr.mk's app packages, mk/site.mk, `make build-info`).
# The kernel checkout generates its own copy inside its O.
BUILD_INFO_HEADER := $(O_INCLUDE)/generated/build_info.h
LEONOS_SOURCE_ID := $(shell git -C $(LEONOS_SRC) rev-parse --short HEAD 2>/dev/null || echo unknown)
LEONOS_SIG_version := source=$(LEONOS_SOURCE_ID)|epoch=$(SOURCE_DATE_EPOCH)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,version)))

$(BUILD_INFO_HEADER): $(LEONOS_SRC)/configs/build-version $(LEONOS_VERSION_TOOL) $(O_META)/version.sig \
	| $(O_INCLUDE)/generated
	$(call LEONOS_LOG,GEN,$@)
	$(Q)$(LEONOS_VERSION_TOOL) --version-file $< \
        --source-id '$(LEONOS_SOURCE_ID)' \
	    --epoch '$(or $(SOURCE_DATE_EPOCH),$(shell git -C $(LEONOS_SRC) show -s --format=%ct HEAD 2>/dev/null || echo 0))' \
	    --output $@
