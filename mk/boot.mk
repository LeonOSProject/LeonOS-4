# Complete freestanding boot chain. No pre-existing build/ artifacts are inputs.
LOADER_ELF := $(O_GENERATED)/boot/loader.elf
LOADER_INTEGRITY := $(O_INCLUDE)/generated/loader_integrity.h
LOADER_SOURCES := $(patsubst $(LEONOS_SRC)/%,%,$(shell find $(LEONOS_SRC)/boot/loader -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort))
LOADER_OBJECTS := $(addprefix $(O_OBJ)/loader/,$(addsuffix .o,$(LOADER_SOURCES)))
LOADER_CFLAGS := -target $(TRIPLE_KERNEL) $(LEONOS_OPTIMIZATION_FLAGS) -std=c11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mgeneral-regs-only -Wall -Wextra -I$(O_INCLUDE) -I$(LEONOS_SRC)/include/uapi -I$(LEONOS_SRC)/include -include $(AUTOCONF_H)
LEONOS_SIG_boot := cc=$(TARGET_CC)|flags=$(LOADER_CFLAGS)|ld=$(TARGET_LD)|sources=$(LOADER_SOURCES)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,boot)))
$(LOADER_INTEGRITY): $(LEONOS_KERNEL_SYS) $(LEONOS_EMIT) $(LEONOS_SRC)/tools/build/loader-integrity.sh
	$(Q)mkdir -p $(@D)
	$(Q)sh $(LEONOS_SRC)/tools/build/loader-integrity.sh $(LEONOS_KERNEL_SYS) $@ $(LEONOS_EMIT)
$(O_OBJ)/loader/%.c.o: $(LEONOS_SRC)/%.c $(LOADER_INTEGRITY) $(AUTOCONF_H) $(O_META)/boot.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(TARGET_CC) $(LOADER_CFLAGS) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(O_OBJ)/loader/%.S.o: $(LEONOS_SRC)/%.S $(O_META)/boot.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(TARGET_CC) -target $(TRIPLE_KERNEL) -ffreestanding -mno-red-zone -mgeneral-regs-only -I$(O_INCLUDE) -I$(LEONOS_SRC)/include -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(LOADER_ELF): $(LOADER_OBJECTS) $(LEONOS_SRC)/boot/loader/linker.ld $(O_META)/boot.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(TARGET_LD) -nostdlib -z max-page-size=0x1000 -T $(LEONOS_SRC)/boot/loader/linker.ld -o $@.tmp $(LOADER_OBJECTS)
	$(Q)mv $@.tmp $@
DRIVER_NAMES := mouse serial e1000 ac97 es1371
DRIVER_OUTPUTS := $(addprefix $(O_GENERATED)/drivers/,$(addsuffix .drv,$(DRIVER_NAMES)))
define driver_rules
DRIVER_$(1)_SOURCES := $$(patsubst $$(LEONOS_SRC)/%,%,$$(shell find $$(LEONOS_SRC)/drivers/$(1) -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort))
DRIVER_$(1)_OBJECTS = $$(addprefix $$(O_OBJ)/drivers/,$$(addsuffix .o,$$(DRIVER_$(1)_SOURCES)))
$$(O_GENERATED)/drivers/$(1).drv: $$(DRIVER_$(1)_OBJECTS) $$(O_META)/kernel-link.sig
	$$(Q)mkdir -p $$(@D)
	$$(Q)$$(TARGET_LD) -r -o $$@.tmp $$(DRIVER_$(1)_OBJECTS)
	$$(Q)mv $$@.tmp $$@
endef
$(foreach driver,$(DRIVER_NAMES),$(eval $(call driver_rules,$(driver))))
$(O_OBJ)/drivers/%.c.o: $(LEONOS_SRC)/%.c $(AUTOCONF_H) $(O_META)/kernel-cc.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(KERNEL_CC_BASE) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(O_OBJ)/drivers/%.S.o: $(LEONOS_SRC)/%.S $(O_META)/kernel-as.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(KERNEL_AS_BASE) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
KERNELDEBUG_SYS := $(O_GENERATED)/system/kerneldebug.sys
$(KERNELDEBUG_SYS): $(LEONOS_SRC)/kernel/kerneldebug/kerneldebug.c $(AUTOCONF_H) $(O_META)/kernel-cc.sig
	$(Q)mkdir -p $(@D)
	$(Q)$(KERNEL_CC_BASE) -c $< -o $@.o.tmp
	$(Q)$(TARGET_OBJCOPY) --remove-section .llvm_addrsig --remove-section .comment --remove-section .note.GNU-stack --rename-section .note.leonos.kerneldebug=.note.leonos.kerneldebug,alloc,load,readonly,data,contents $@.o.tmp $@.tmp
	$(Q)mv $@.tmp $@
	$(Q)rm $@.o.tmp
.PHONY: loader drivers boot
loader: $(LOADER_ELF)
drivers: $(DRIVER_OUTPUTS) $(KERNELDEBUG_SYS)
boot: kernel loader drivers
-include $(LOADER_OBJECTS:%=%.d) $(foreach driver,$(DRIVER_NAMES),$(DRIVER_$(driver)_OBJECTS:%=%.d))
