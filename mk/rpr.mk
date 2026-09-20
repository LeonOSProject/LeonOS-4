LEONOS_OSCHINPT_INDEX := $(LEONOS_HOST_BIN)/leonos-oschinpt-index
$(LEONOS_OSCHINPT_INDEX): $(O_HOST)/obj/tools/host/assets/leonos-oschinpt-index.c.o $(LEONOS_HOST_COMMON_OBJS)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
OSCHINPT_INDEX := $(O_GENERATED)/rpr/pinyin_simp.idx
$(OSCHINPT_INDEX): $(LEONOS_SRC)/third_party/rime-pinyin-simp/pinyin_simp.dict.yaml $(LEONOS_OSCHINPT_INDEX)
	$(Q)mkdir -p $(@D)
	$(Q)$(LEONOS_OSCHINPT_INDEX) $< $@
RPR_APPS := $(O)/rpr-apps/repository
RPR_APP_PACKAGES := $(RPR_APPS)/.complete $(RPR_APPS)/packages.list
RPR_INPUTS := $(wildcard $(LEONOS_SRC)/third_party/rime-pinyin-simp/* $(LEONOS_SRC)/tools/oschinpt-apk-* $(LEONOS_SRC)/userland/apps/oschinpt/settings.ini) $(LEONOS_SRC)/third_party/doomgeneric/freedoom1.wad $(LEONOS_SRC)/third_party/doomgeneric/LICENSE $(LEONOS_SRC)/third_party/doomgeneric/FREEDOOM-COPYING.txt $(LEONOS_SRC)/resources/build-art/app-icons/helloworld.bmp $(LEONOS_SRC)/resources/build-art/app-icons/doom.bmp
$(RPR_APP_PACKAGES) &: $(addprefix $(USERLAND_DIR)/,helloworld.elf doom.elf doomlauncher.elf oschinpt.elf) $(OSCHINPT_INDEX) $(BUILD_INFO_HEADER) $(RPR_INPUTS) $(APK_MANIFEST) $(LEONOS_SRC)/tools/build/rpr-apps.sh
	$(Q)sh $(LEONOS_SRC)/tools/build/rpr-apps.sh $(LEONOS_SRC) $(O) $(BUILD_INFO_HEADER) $(OSCHINPT_INDEX) $(UPSTREAM_APK) '$(APK_SIGNING_KEY)' $(RPR_APPS) $(SOURCE_DATE_EPOCH)
RPR_PAGES := $(O)/rpr-pages
$(RPR_PAGES)/.complete $(RPR_PAGES)/manifest.json &: $(RPR_APP_PACKAGES) $(APK_MANIFEST) $(APK_REPOSITORY)/packages.adb $(LEONOS_KERNEL_SYS) $(MIDDLELAYER_SYS) $(BUILD_INFO_HEADER) $(LEONOS_SRC)/tools/build/rpr-pages.sh
	$(Q)SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH) sh $(LEONOS_SRC)/tools/build/rpr-pages.sh $(APK_REPOSITORY) $(RPR_APPS) $(LEONOS_KERNEL_SYS) $(MIDDLELAYER_SYS) $(BUILD_INFO_HEADER) $(UPSTREAM_APK) '$(APK_SIGNING_KEY)' $(RPR_PAGES)
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh write $(RPR_PAGES) $(O_META)/rpr-pages.files
$(O_META)/rpr-pages-present.sig: FORCE
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh check $(RPR_PAGES) $(O_META)/rpr-pages.files $@
$(RPR_PAGES)/.complete $(RPR_PAGES)/manifest.json: $(O_META)/rpr-pages-present.sig $(LEONOS_SRC)/tools/build/stage-inventory.sh
.PHONY: rpr-apps rpr-pages
rpr-apps: $(RPR_APP_PACKAGES)
rpr-pages: $(RPR_PAGES)/.complete $(RPR_PAGES)/manifest.json
tools: $(LEONOS_OSCHINPT_INDEX)
