# Signed APK repository and managed root. The rootfs fragment publishes a raw,
# unpackaged tree here; this fragment never infers or rebuilds that producer.
APK_RAW_ROOT ?= $(O_STAGE)/rootfs-raw
APK_RAW_STAMP ?= $(APK_RAW_ROOT)/.complete
APK_MANAGED_ROOT := $(O)/rootfs/managed
APK_WORK := $(O_PACKAGES)/apk
APK_REPOSITORY := $(APK_WORK)/repository
APK_MANIFEST := $(APK_WORK)/manifest.json
APK_OWNERSHIP := $(APK_WORK)/ownership.tsv
APK_SIGNING_KEY ?= $(HOME)/.local/share/leonos/apk-signing/key.pem
# Generation 2 supersedes the generation-1 BusyBox/binutils ownership regression,
# including targets partially updated by those media. Published releases must use
# an increasing SOURCE_DATE_EPOCH (or an explicit increasing version prefix).
APK_BUILD_VERSION ?= 2.$(SOURCE_DATE_EPOCH)-r0
APK_STAGE_SCRIPT := $(LEONOS_SRC)/tools/build/apk-stage.sh
APK_STAGE_INPUTS := $(LEONOS_SRC)/tools/build/apk-layout.sh $(LEONOS_LAYOUT_TOOL) $(LEONOS_LOCK)
BUILD_LOG_INPUTS := $(LEONOS_SRC)/tools/build/run-logged.sh $(LEONOS_SRC)/tools/build/format-log.awk
APK_STAGE_INPUTS += $(BUILD_LOG_INPUTS)

LEONOS_SIG_apk-stage := version=$(APK_BUILD_VERSION)|epoch=$(SOURCE_DATE_EPOCH)|raw=$(abspath $(APK_RAW_ROOT))|key=$(APK_SIGNING_KEY)|key-identity=$(shell sha256sum '$(APK_SIGNING_KEY)' 2>/dev/null | cut -d' ' -f1)|policy=$(shell sha256sum $(LEONOS_SRC)/configs/apk-ownership.json 2>/dev/null | cut -d' ' -f1)|script=$(shell sha256sum $(APK_STAGE_SCRIPT) 2>/dev/null | cut -d' ' -f1)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,apk-stage)))

# rootfs-raw/.complete is a durable producer interface. Its recipe is owned by
# mk/rootfs.mk; keeping it as a prerequisite catches missing staging explicitly.
$(APK_MANAGED_ROOT)/.apk-complete $(APK_REPOSITORY)/packages.adb $(APK_MANIFEST) $(APK_OWNERSHIP) &: \
	$(APK_RAW_STAMP) $(APK_STAGE_SCRIPT) $(APK_STAGE_INPUTS) $(LEONOS_APK_OWN) $(UPSTREAM_APK) \
	$(UPSTREAM_APK_ROOT)/.complete $(LEONOS_SRC)/configs/apk-ownership.json \
	$(LEONOS_SRC)/resources/licenses/apk-tools-LICENSE $(MUSL_STAMP) \
	$(LEONOS_SRC)/userland/storage/leonos-apk-update \
	$(LEONOS_SRC)/userland/storage/busybox-binutils-links \
	$(LEONOS_SRC)/system/rootfs/etc/apk/protected_paths.d/leonos.list \
	$(O_META)/apk-stage.sig
	$(Q)mkdir -p $(O_LOGS) $(O_PACKAGES)
	$(Q)SOURCE_DATE_EPOCH='$(SOURCE_DATE_EPOCH)' APK_MUSL_SYSROOT='$(abspath $(MUSL_SYSROOT))' sh $(LEONOS_SRC)/tools/build/run-logged.sh $(O_LOGS)/apk-stage.log sh $(APK_STAGE_SCRIPT) \
		$(LEONOS_SRC) $(abspath $(APK_RAW_ROOT)) $(abspath $(APK_MANAGED_ROOT)) \
		$(abspath $(APK_WORK)) $(abspath $(UPSTREAM_APK)) $(abspath $(UPSTREAM_APK_ROOT)) \
		$(LEONOS_SRC)/configs/apk-ownership.json $(abspath $(LEONOS_APK_OWN)) \
		$(APK_SIGNING_KEY) '$(APK_BUILD_VERSION)'
	$(Q)touch $(APK_MANAGED_ROOT)/.apk-complete
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh write $(APK_MANAGED_ROOT) $(O_META)/apk-root.files
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh write $(APK_REPOSITORY) $(O_META)/apk-repo.files

$(O_META)/apk-root-present.sig: FORCE
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh check $(APK_MANAGED_ROOT) $(O_META)/apk-root.files $@
$(O_META)/apk-repo-present.sig: FORCE
	$(Q)sh $(LEONOS_SRC)/tools/build/stage-inventory.sh check $(APK_REPOSITORY) $(O_META)/apk-repo.files $@
$(APK_MANAGED_ROOT)/.apk-complete $(APK_REPOSITORY)/packages.adb $(APK_MANIFEST) $(APK_OWNERSHIP): $(O_META)/apk-root-present.sig $(O_META)/apk-repo-present.sig $(LEONOS_SRC)/tools/build/stage-inventory.sh

apk-repo: $(APK_MANAGED_ROOT)/.apk-complete $(APK_REPOSITORY)/packages.adb $(APK_MANIFEST) $(APK_OWNERSHIP)
rootfs: $(APK_MANAGED_ROOT)/.apk-complete

.PHONY: test-apk
test-apk:
	$(Q)sh $(LEONOS_SRC)/tests/build/test-apk-ownership.sh $(LEONOS_SRC)
