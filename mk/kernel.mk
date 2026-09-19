# Kernel, boot-loader-side freestanding objects and the linker stage.
#
# Object paths keep the component and the source-relative path so two util.c
# files cannot collide (plan section 6.1).

KERNEL_LD_SCRIPT := $(LEONOS_SRC)/kernel/ntclks/arch/x86_64/linker.ld
KERNEL_SOURCE_DIRS := kernel/ntclks kernel/ostui drivers/bootstrap
# drivers/bootstrap/storage/*.c are textually included by the storage.c facade
# and must not also become independent objects.
KERNEL_SOURCE_EXCLUDE := drivers/bootstrap/storage

KERNEL_SOURCES_LIST := $(O_OBJ)/kernel/sources.list
KERNEL_SOURCES_MK := $(O_OBJ)/kernel/sources.mk
KERNEL_UNSTRIPPED := $(O_GENERATED)/system/kernel.unstripped
LEONOS_KERNEL_SYS := $(O_GENERATED)/system/kernel.sys
LEONOS_KERNEL_DEBUG := $(O_GENERATED)/system/kernel.debug
BOOT_LOGO_HEADER := $(O_INCLUDE)/generated/boot_logo.h
BUILD_INFO_HEADER := $(O_INCLUDE)/generated/build_info.h
LEONOS_SOURCE_ID := $(shell git -C $(LEONOS_SRC) rev-parse --short HEAD 2>/dev/null || echo unknown)
LEONOS_SIG_version := source=$(LEONOS_SOURCE_ID)|epoch=$(SOURCE_DATE_EPOCH)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,version)))

# --- flags ------------------------------------------------------------------
# Include order puts the output tree first: include/generated/ still holds
# committed copies of autoconf.h and build_info.h, and a stale tracked header
# must not shadow what this build just produced. P4 removes those copies.
KERNEL_INCLUDES := -I$(O_INCLUDE) -I$(LEONOS_SRC)/kernel/ntclks/include \
	-I$(LEONOS_SRC)/include/uapi -I$(LEONOS_SRC)/include

KERNEL_CC_BASE := $(TARGET_CC) -target $(TRIPLE_KERNEL) \
	$(LEONOS_OPTIMIZATION_FLAGS) -std=c11 -ffreestanding -fno-stack-protector \
	-fno-pic -fno-pie -mno-red-zone -mgeneral-regs-only -mcmodel=kernel \
	-Wall -Wextra $(KERNEL_INCLUDES) -include $(AUTOCONF_H)

KERNEL_AS_BASE := $(TARGET_CC) -target $(TRIPLE_KERNEL) \
	$(LEONOS_OPTIMIZATION_FLAGS) -ffreestanding -mno-red-zone \
	-mgeneral-regs-only $(KERNEL_INCLUDES)

KERNEL_LD_BASE := $(TARGET_LD) -nostdlib -z max-page-size=0x1000 \
	-T $(KERNEL_LD_SCRIPT)

# The signature records the resolved tool paths as well as the argv, so moving
# to a different clang on PATH rebuilds instead of mixing objects.
leonos_absolute = $(shell command -v $(1) 2>/dev/null || printf '%s' '$(1)')

LEONOS_SIG_kernel-cc := argv=$(KERNEL_CC_BASE) $(KERNEL_CFLAGS)|path=$(call leonos_absolute,$(TARGET_CC))|identity=$(shell $(TARGET_CC) --version 2>&1 | head -n1)
LEONOS_SIG_kernel-as := argv=$(KERNEL_AS_BASE) $(KERNEL_AFLAGS)|path=$(call leonos_absolute,$(TARGET_CC))|identity=$(shell $(TARGET_CC) --version 2>&1 | head -n1)
LEONOS_SIG_kernel-link := argv=$(KERNEL_LD_BASE) $(KERNEL_LDFLAGS)|path=$(call leonos_absolute,$(TARGET_LD))|identity=$(shell $(TARGET_LD) --version 2>&1 | head -n1)

$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,kernel-cc)))
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,kernel-as)))
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,kernel-link)))

# --- source manifest --------------------------------------------------------
# The directory is the dependency: creating or removing a file changes its
# mtime, which regenerates the list, which is a prerequisite of the link. A
# deleted source therefore relinks even though every surviving object is current
# (plan section 6.1).
# leonos-emit is what keeps the manifest mtime stable, so the rule must wait for
# it: a fresh output directory builds host tools on demand.
$(KERNEL_SOURCES_LIST): FORCE $(LEONOS_EMIT)
	$(Q)mkdir -p $(dir $@)
	$(Q)find $(KERNEL_SOURCE_DIRS) -path '$(KERNEL_SOURCE_EXCLUDE)/*' -prune -o \
	    -type f \( -name '*.c' -o -name '*.S' \) -print \
	  | LC_ALL=C sort > $@.tmp
	$(Q)$(LEONOS_EMIT) --input $@.tmp --output $@
	$(Q)rm -f $@.tmp

# The generated fragment splits the manifest by action so the two signature
# classes stay independent.
$(KERNEL_SOURCES_MK): $(KERNEL_SOURCES_LIST)
	$(Q)cc_list=''; as_list=''; \
	while read -r source; do \
	    case "$$source" in \
	        *.c) cc_list="$$cc_list $(O_OBJ)/kernel/$$source.o" ;; \
	        *)   as_list="$$as_list $(O_OBJ)/kernel/$$source.o" ;; \
	    esac; \
	done < $<; \
	printf 'LEONOS_KERNEL_CC_OBJS :=%s\nLEONOS_KERNEL_AS_OBJS :=%s\n' \
	    "$$cc_list" "$$as_list" > $@.tmp && mv $@.tmp $@

$(KERNEL_SOURCES_MK): | $(O_OBJ)

ifeq ($(LEONOS_PASSIVE),1)
# help, clean and distclean must not materialise or regenerate a make fragment
# they do not need (plan section 4).
else
ifneq ($(LEONOS_INSPECT),)
$(eval $(file <$(KERNEL_SOURCES_MK)))
else
-include $(KERNEL_SOURCES_MK)
endif
endif
-include $(shell find $(O_OBJ)/kernel -name '*.o.d' 2>/dev/null)

LEONOS_KERNEL_OBJS := $(LEONOS_KERNEL_CC_OBJS) $(LEONOS_KERNEL_AS_OBJS)

# --- compilation ------------------------------------------------------------
# One generated header per object, not one per build: the storage facade and the
# two special dependencies below keep the graph honest without making every
# object wait for every asset.
$(O_OBJ)/kernel/%.c.o: $(LEONOS_SRC)/%.c $(AUTOCONF_H) $(O_META)/kernel-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' CC $<
	$(Q)$(KERNEL_CC_BASE) $(KERNEL_CFLAGS) -MMD -MF $@.d -c $< -o $@

$(O_OBJ)/kernel/%.S.o: $(LEONOS_SRC)/%.S $(O_META)/kernel-as.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' AS $<
	$(Q)$(KERNEL_AS_BASE) $(KERNEL_AFLAGS) -MMD -MF $@.d -c $< -o $@

# Extra prerequisites for the objects that consume generated headers. Depfiles
# take over from the second build onward; these make the first build correct.
$(O_OBJ)/kernel/drivers/bootstrap/boot_splash.c.o: $(BOOT_LOGO_HEADER)
$(O_OBJ)/kernel/kernel/ntclks/version.c.o: $(BUILD_INFO_HEADER)

# The storage facade textually includes its private modules.
$(O_OBJ)/kernel/drivers/bootstrap/storage.c.o: \
	$(wildcard $(LEONOS_SRC)/drivers/bootstrap/storage/*.c) \
	$(LEONOS_SRC)/drivers/bootstrap/storage/storage_internal.h

# --- generated inputs -------------------------------------------------------
$(BOOT_LOGO_HEADER): $(LEONOS_SRC)/logo.png $(LEONOS_BOOT_LOGO_TOOL) | $(O_INCLUDE)/generated
	$(Q)printf '  %-8s %s\n' GEN $@
	$(Q)$(LEONOS_BOOT_LOGO_TOOL) --input $< --output $@ --size 192

$(BUILD_INFO_HEADER): $(LEONOS_SRC)/configs/build-version $(LEONOS_VERSION_TOOL) $(O_META)/version.sig \
	| $(O_INCLUDE)/generated
	$(Q)printf '  %-8s %s\n' GEN $@
	$(Q)$(LEONOS_VERSION_TOOL) --version-file $< \
        --source-id '$(LEONOS_SOURCE_ID)' \
	    --epoch '$(or $(SOURCE_DATE_EPOCH),$(shell git -C $(LEONOS_SRC) show -s --format=%ct HEAD 2>/dev/null || echo 0))' \
	    --output $@

# --- link and images --------------------------------------------------------
# Grouped target: kernel.unstripped is the only output of the link, but the two
# objcopy products are derived from it independently, so each gets its own rule
# and neither can be mistaken for a complete-image stamp.
$(KERNEL_UNSTRIPPED): $(LEONOS_KERNEL_OBJS) $(KERNEL_LD_SCRIPT) \
	$(KERNEL_SOURCES_LIST) $(O_META)/kernel-link.sig | $(O_GENERATED)/system
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' LD $@
	$(Q)$(KERNEL_LD_BASE) $(KERNEL_LDFLAGS) -o $@ \
	    $(LEONOS_KERNEL_CC_OBJS) $(LEONOS_KERNEL_AS_OBJS)

$(LEONOS_KERNEL_SYS): $(KERNEL_UNSTRIPPED) $(O_META)/kernel-objcopy.sig | $(O_GENERATED)/system
	$(Q)printf '  %-8s %s\n' IMAGE $@
	$(Q)$(TARGET_OBJCOPY) --strip-debug $< $@.tmp
	$(Q)mv $@.tmp $@

$(LEONOS_KERNEL_DEBUG): $(KERNEL_UNSTRIPPED) $(O_META)/kernel-objcopy.sig | $(O_GENERATED)/system
	$(Q)printf '  %-8s %s\n' IMAGE $@
	$(Q)$(TARGET_OBJCOPY) --only-keep-debug $< $@.tmp
	$(Q)mv $@.tmp $@

LEONOS_SIG_kernel-objcopy := argv=$(TARGET_OBJCOPY)|path=$(call leonos_absolute,$(TARGET_OBJCOPY))|identity=$(shell $(TARGET_OBJCOPY) --version 2>&1 | head -n1)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,kernel-objcopy)))

# Whole-object-tree invalidation guard: `.DELETE_ON_ERROR` covers interrupted
# recipes, and the object files are individually tracked by the depfiles above.
.DELETE_ON_ERROR:
