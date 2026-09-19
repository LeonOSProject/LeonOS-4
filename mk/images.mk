# Host tools own the disk formats; Make owns each image dependency.
GRUB_EFI_DIR ?= /usr/lib/grub/x86_64-efi
LEONOS_EXT2_TIME := $(LEONOS_HOST_BIN)/leonos-ext2-time
$(LEONOS_EXT2_TIME): $(O_HOST)/obj/tools/host/images/leonos-ext2-time.c.o
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -lext2fs -lcom_err -o $@
ESP_STAGE := $(O_STAGE)/esp
ESP_STAMP := $(O_STAGE)/esp.complete
IMAGE_DISPLAY := $(O_GENERATED)/config/display.conf
$(IMAGE_DISPLAY): $(LEONOS_CONFIG_FILE) $(LEONOS_EMIT)
	$(Q)mkdir -p $(@D)
	$(Q)awk 'BEGIN{theme="metro";mode="fill"} /^CONFIG_VMDK_DEFAULT_THEME_WIN95=y$$/{theme="win95"} /^CONFIG_VMDK_WALLPAPER_STRETCH=y$$/{mode="stretch"} /^CONFIG_VMDK_WALLPAPER_CENTER=y$$/{mode="center"} END{print "theme="theme;print "wallpaper.mode="mode}' $< | $(LEONOS_EMIT) --input - --output $@
LEONOS_SIG_images := epoch=$(SOURCE_DATE_EPOCH)|grub=$(GRUB_EFI_DIR)|grub-identity=$(shell grub-mkstandalone --version 2>/dev/null)|mke2fs=$(shell mke2fs -V 2>&1 | head -n1)|mtools=$(shell mcopy -V 2>&1 | head -n1)|disk-size=$(CONFIG_IMAGE_SIZE_MIB)|installer-size=$(CONFIG_INSTALLER_ROOT_SIZE_MIB)|qemu-img=$(shell qemu-img --version 2>/dev/null | head -n1)|xorriso=$(shell xorriso -version 2>/dev/null | head -n1)|sfdisk=$(shell sfdisk --version 2>/dev/null)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,images)))
$(ESP_STAMP): $(LOADER_ELF) $(O_GENERATED)/system/kernel.sys $(MIDDLELAYER_SYS) $(GRUB_FONT) $(IMAGE_DISPLAY) $(LEONOS_SRC)/tools/build/efi-stage.sh $(LEONOS_SRC)/boot/grub/embedded.cfg $(LEONOS_SRC)/boot/grub/grub.cfg $(LEONOS_SRC)/boot/grub/theme/theme.txt $(GRUB_EFI_DIR)/modinfo.sh $(O_META)/images.sig
	$(Q)sh $(LEONOS_SRC)/tools/build/efi-stage.sh $(LEONOS_SRC) $(GRUB_EFI_DIR) $(LOADER_ELF) $(O_GENERATED)/system/kernel.sys $(MIDDLELAYER_SYS) $(GRUB_FONT) $(IMAGE_DISPLAY) $(ESP_STAGE) $(SOURCE_DATE_EPOCH)
	$(Q)touch $@
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh write $(ESP_STAGE) $(O_META)/esp.files
.PHONY: esp
esp: $(ESP_STAMP)
tools: $(LEONOS_EXT2_TIME)

ROOT_EXT2 := $(O_IMAGES)/root.ext2
DISK_ROOT_EXT2 := $(O_IMAGES)/disk-root.ext2
LIVE_ROOT_STAGE := $(O_STAGE)/live-root
DISK_ROOT_STAGE := $(O_STAGE)/disk-root
define LEONOS_STANDALONE_STAGE
$(O_STAGE)/$(1)-root.complete: $(APK_MANAGED_ROOT)/.apk-complete $(LEONOS_SRC)/tools/build/standalone-root.sh $(wildcard $(LEONOS_SRC)/system/test-accounts/*) $(LEONOS_CONFIG_FILE)
	$$(Q)sh $$(LEONOS_SRC)/tools/build/standalone-root.sh $$(LEONOS_SRC) $$(APK_MANAGED_ROOT) $$(O_STAGE)/$(1)-root $(1) $(if $(filter disk,$(1)),$(if $(filter y,$(CONFIG_VMDK_DEFAULT_LANGUAGE_ZH)),zh,en),en)
	$$(Q)touch $$@
	$$(Q)sh $$(LEONOS_SRC)/tools/build/stage-inventory.sh write $$(O_STAGE)/$(1)-root $$(O_META)/$(1)-root.files
endef
$(eval $(call LEONOS_STANDALONE_STAGE,live))
$(eval $(call LEONOS_STANDALONE_STAGE,disk))
INSTALLER_ROOT := $(O_STAGE)/installer
INSTALLER_STAGE_STAMP := $(O_STAGE)/installer.complete
INSTALLER_EXT2 := $(O_IMAGES)/installer-root.ext2
DISK_RAW := $(O_IMAGES)/leonos4.raw
DISK_VMDK := $(O_IMAGES)/leonos4.vmdk
LIVE_ISO := $(O_IMAGES)/leonos4-live.iso
INSTALLER_ISO := $(O_IMAGES)/leonos4-installer.iso
$(LIVE_ISO) $(INSTALLER_ISO): $(BUILD_LOG_INPUTS)
$(INSTALLER_STAGE_STAMP): $(USERLAND_DIR)/installer.elf
$(ROOT_EXT2): $(O_STAGE)/live-root.complete $(LEONOS_EXT2_TIME) $(LEONOS_SRC)/tools/build/images.sh $(O_META)/images.sig
	$(Q)LEONOS_EXT2_TIME=$(LEONOS_EXT2_TIME) sh $(LEONOS_SRC)/tools/build/images.sh ext2 $(LIVE_ROOT_STAGE) $@ $(SOURCE_DATE_EPOCH) 5c13543b-732c-4f81-8652-621124484420
$(DISK_ROOT_EXT2): $(O_STAGE)/disk-root.complete $(LEONOS_EXT2_TIME) $(LEONOS_SRC)/tools/build/images.sh $(O_META)/images.sig
	$(Q)LEONOS_EXT2_TIME=$(LEONOS_EXT2_TIME) sh $(LEONOS_SRC)/tools/build/images.sh ext2 $(DISK_ROOT_STAGE) $@ $(SOURCE_DATE_EPOCH) 5c13543b-732c-4f81-8652-621124484420
LEONOS_DEDUP_TOOL := $(LEONOS_HOST_BIN)/leonos-dedup
$(LEONOS_DEDUP_TOOL): $(O_HOST)/obj/tools/host/manifest/leonos-dedup.c.o | $(LEONOS_HOST_BIN)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
tools: $(LEONOS_DEDUP_TOOL)
$(INSTALLER_STAGE_STAMP): $(LEONOS_DEDUP_TOOL) $(APK_STAGE_INPUTS)
$(INSTALLER_STAGE_STAMP): $(ROOTFS_RAW_STAMP) $(ESP_STAMP) $(RUNTIME_INSTALLER_SO) $(O)/userland-installer-policy/desktop.elf $(O)/userland-installer-policy/settings.elf $(O)/userland-installer/gptinit.elf $(APK_MANAGED_ROOT)/.apk-complete $(LEONOS_SRC)/tools/build/installer-stage.sh $(APK_STAGE_SCRIPT) $(LEONOS_SRC)/docs/ADVANCED_INSTALL.txt $(O_META)/images.sig
	$(Q)mkdir -p $(O_LOGS)
	$(Q)APK_TOOL=$(abspath $(UPSTREAM_APK)) APK_UPSTREAM=$(abspath $(UPSTREAM_APK_ROOT)) APK_OWN_TOOL=$(abspath $(LEONOS_APK_OWN)) APK_KEY='$(APK_SIGNING_KEY)' APK_VERSION='$(APK_BUILD_VERSION)' sh $(LEONOS_SRC)/tools/build/run-logged.sh $(O_LOGS)/installer-stage.log sh $(LEONOS_SRC)/tools/build/installer-stage.sh $(LEONOS_SRC) $(abspath $(O)) $(abspath $(ROOTFS_RAW)) $(abspath $(ESP_STAGE)) $(abspath $(INSTALLER_ROOT)) $(SOURCE_DATE_EPOCH)
	$(Q)touch $@
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh write $(INSTALLER_ROOT) $(O_META)/installer.files
$(INSTALLER_EXT2): $(INSTALLER_STAGE_STAMP) $(LEONOS_EXT2_TIME) $(LEONOS_SRC)/tools/build/images.sh $(O_META)/images.sig
	$(Q)IMAGE_MIN_MIB=$(or $(CONFIG_INSTALLER_ROOT_SIZE_MIB),400) LEONOS_EXT2_TIME=$(LEONOS_EXT2_TIME) sh $(LEONOS_SRC)/tools/build/images.sh ext2 $(INSTALLER_ROOT) $@ $(SOURCE_DATE_EPOCH) 9e1f6d46-3f41-42d2-9917-9a21a2134401
$(DISK_RAW) $(DISK_VMDK) &: $(DISK_ROOT_EXT2) $(ESP_STAMP) $(LEONOS_SRC)/tools/build/disk.sh $(O_META)/images.sig
	$(Q)IMAGE_MIN_MIB=$(or $(CONFIG_IMAGE_SIZE_MIB),1024) sh $(LEONOS_SRC)/tools/build/disk.sh $(ESP_STAGE) $(DISK_ROOT_EXT2) $(DISK_RAW) $(DISK_VMDK) $(SOURCE_DATE_EPOCH)
$(LIVE_ISO): $(ROOT_EXT2) $(ESP_STAMP) $(LEONOS_SRC)/tools/build/iso.sh $(LEONOS_SRC)/boot/grub/live.cfg $(LEONOS_SRC)/boot/grub/installer_embedded.cfg $(O_META)/images.sig
	$(Q)sh $(LEONOS_SRC)/tools/build/iso.sh $(LEONOS_SRC) $(GRUB_EFI_DIR) $(ESP_STAGE) $(ROOT_EXT2) $(LEONOS_SRC)/boot/grub/live.cfg $@ $(SOURCE_DATE_EPOCH) LEONOS4LIVE
$(INSTALLER_ISO): $(INSTALLER_EXT2) $(ESP_STAMP) $(LEONOS_SRC)/tools/build/iso.sh $(LEONOS_SRC)/boot/grub/installer.cfg $(LEONOS_SRC)/boot/grub/installer_embedded.cfg $(O_META)/images.sig
	$(Q)sh $(LEONOS_SRC)/tools/build/iso.sh $(LEONOS_SRC) $(GRUB_EFI_DIR) $(ESP_STAGE) $(INSTALLER_EXT2) $(LEONOS_SRC)/boot/grub/installer.cfg $@ $(SOURCE_DATE_EPOCH) LEONOS4INST
.PHONY: image-vmdk iso installer
image-vmdk: $(DISK_RAW) $(DISK_VMDK)
iso: $(LIVE_ISO)
installer: $(INSTALLER_ISO)

define LEONOS_IMAGE_PRESENCE
$(O_META)/$(1)-present.sig: FORCE
	$$(Q)sh $$(LEONOS_SRC)/tools/build/stage-inventory.sh check $(2) $$(O_META)/$(1).files $$@
$(3): $(O_META)/$(1)-present.sig $(LEONOS_SRC)/tools/build/stage-inventory.sh
endef
$(eval $(call LEONOS_IMAGE_PRESENCE,esp,$(ESP_STAGE),$(ESP_STAMP)))
$(eval $(call LEONOS_IMAGE_PRESENCE,live-root,$(LIVE_ROOT_STAGE),$(O_STAGE)/live-root.complete))
$(eval $(call LEONOS_IMAGE_PRESENCE,disk-root,$(DISK_ROOT_STAGE),$(O_STAGE)/disk-root.complete))
$(eval $(call LEONOS_IMAGE_PRESENCE,installer,$(INSTALLER_ROOT),$(INSTALLER_STAGE_STAMP)))
