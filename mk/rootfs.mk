# Rootfs composition is driven by source and installed-product inventories.
LEONOS_STAGE_TOOL := $(LEONOS_HOST_BIN)/leonos-stage
LEONOS_LAYOUT_TOOL := $(LEONOS_HOST_BIN)/leonos-layout
$(LEONOS_STAGE_TOOL): $(O_HOST)/obj/tools/host/manifest/leonos-stage.c.o $(LEONOS_HOST_COMMON_OBJS) $(O_HOST)/obj/tools/host/manifest/json.c.o
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
$(LEONOS_LAYOUT_TOOL): $(O_HOST)/obj/tools/host/manifest/leonos-layout.c.o
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
ROOTFS_RAW := $(O)/rootfs/raw
ROOTFS_RAW_STAMP := $(O)/rootfs/raw.complete
ROOTFS_MANIFEST := $(O)/rootfs/manifest.json
APK_RAW_ROOT := $(ROOTFS_RAW)
APK_RAW_STAMP := $(ROOTFS_RAW_STAMP)
ROOTFS_SOURCE_DIRS := $(LEONOS_SRC)/system $(LEONOS_SRC)/resources/build-art $(LEONOS_SRC)/userland/storage $(LEONOS_SRC)/userland/apps
ROOTFS_SOURCE_DIRS += $(LEONOS_SRC)/userland/fastfetch $(LEONOS_SRC)/userland/lua $(LEONOS_SRC)/logo.png $(LEONOS_SRC)/test/test.mp3 $(LEONOS_SRC)/docs/APK_PREPARATION.md $(LEONOS_SRC)/configs/apk-ownership.json $(LEONOS_SRC)/third_party/stardustui/docs/zh-cn/example
ROOTFS_SOURCE_DIRS += $(wildcard $(addprefix $(LEONOS_SRC)/third_party/,busybox/LICENSE file/COPYING cmd/LICENSE less/LICENSE sl/LICENSE pl_editor/LICENSE vim/LICENSE portablegl/LICENSE))
ROOTFS_SOURCE_DIRS += $(LEONOS_SRC)/tools/build/rpr-config.sh
$(O_META)/rootfs-sources.sig: FORCE $(LEONOS_SRC)/tools/build/tree-signature.sh
	$(Q)sh $(LEONOS_SRC)/tools/build/tree-signature.sh $@ $(ROOTFS_SOURCE_DIRS)
ROOTFS_UPSTREAM_PRODUCTS = $(foreach package,$(UPSTREAM_PACKAGES) ncurses vim,$(UPSTREAM_ROOT)/$(package)/root/.complete $(upstream_$(package)_products))
ROOTFS_APP_PRODUCTS = $(addprefix $(USERLAND_DIR)/,$(addsuffix .elf,$(LEONOS_COMPONENT_APPS))) $(USERLAND_DIR)/dynlinkerror.elf $(BUSYBOX_ELF) $(BUSYBOX_LINKS) $(LUA_SO) $(MAGIC_SO) $(SQLITE_SO) $(FILE_MAGIC) $(PORTABLEGL_SO) $(USERLAND_DIR)/cmd.elf $(USERLAND_DIR)/less.elf $(USERLAND_DIR)/sl.elf
$(ROOTFS_RAW_STAMP) $(ROOTFS_MANIFEST): $(NLS_MO) $(NLS_MUSL_MO) $(LEONOS_SRC)/configs/nls/LINGUAS
LEONOS_SIG_rootfs := epoch=$(SOURCE_DATE_EPOCH)|sources=$(O_META)/rootfs-sources.sig|components=$(LEONOS_COMPONENTS_ENABLED)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,rootfs)))
$(ROOTFS_RAW_STAMP) $(ROOTFS_MANIFEST) &: $(LEONOS_SRC)/tools/build/rootfs-stage.sh $(LEONOS_STAGE_TOOL) $(LEONOS_LAYOUT_TOOL) $(O_META)/rootfs-sources.sig $(ROOTFS_UPSTREAM_PRODUCTS) $(ROOTFS_APP_PRODUCTS) $(RUNTIME_SO) $(KERNELDEBUG_SYS) $(DRIVER_OUTPUTS) $(COMPONENT_METADATA) $(UI_METRO_FONT) $(UI_WIN95_FONT) $(LEONOS_CONFIG_FILE) $(O_META)/rootfs.sig
	$(Q)sh $(LEONOS_SRC)/tools/build/rootfs-stage.sh $(LEONOS_SRC) $(abspath $(O)) $(abspath $(LEONOS_CONFIG_FILE)) $(abspath $(COMPONENT_METADATA)) $(abspath $(LEONOS_STAGE_TOOL)) $(abspath $(LEONOS_LAYOUT_TOOL)) $(abspath $(ROOTFS_RAW)) $(abspath $(ROOTFS_MANIFEST)) $(SOURCE_DATE_EPOCH)
	$(Q)touch $(ROOTFS_RAW_STAMP)
.PHONY: rootfs-raw
rootfs-raw: $(ROOTFS_RAW_STAMP) $(ROOTFS_MANIFEST)
tools: $(LEONOS_STAGE_TOOL) $(LEONOS_LAYOUT_TOOL)
$(O_META)/rootfs-present.sig: FORCE $(LEONOS_STAGE_TOOL)
	$(Q)mkdir -p $(@D)
	$(Q)if ! $(LEONOS_STAGE_TOOL) --check $(ROOTFS_MANIFEST) $(ROOTFS_RAW); then touch $@; elif test ! -f $@; then touch $@; fi
$(ROOTFS_RAW_STAMP) $(ROOTFS_MANIFEST): $(O_META)/rootfs-present.sig
