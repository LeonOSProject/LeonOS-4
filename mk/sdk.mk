# Relocatable musl SDK subset (the developer kit assembled from build output).
MUSL_SDK := $(O)/sdk/leonos-musl-sdk
MUSL_SDK_ARCHIVE := $(O_PACKAGES)/leonos-musl-sdk.tar.gz
SDK_EPOCH := $(or $(SOURCE_DATE_EPOCH),$(shell git -C $(LEONOS_SRC) show -s --format=%ct HEAD))
SDK_INPUT_HEADERS := $(shell find $(LEONOS_SRC)/include/uapi $(LEONOS_SRC)/include/leonos $(LEONOS_SRC)/userland/libc/include/leonos -type f -name '*.h' | LC_ALL=C sort)
LEONOS_SIG_sdk := epoch=$(SDK_EPOCH)|headers=$(SDK_INPUT_HEADERS)|driver=$(LEONOS_SDK_DRIVER)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,sdk)))

$(COMPONENT_SELECTION): $(O_CONFIG)/components.mk $(LEONOS_CONFIG_FILE) $(LEONOS_SRC)/configs/components.toml $(LEONOS_COMPONENT_TOOL)
	$(Q)set -eu; mkdir -p $(dir $@)
	$(Q)set -eu; $(O_HOST)/bin/leonos-components --input $(LEONOS_SRC)/configs/components.toml \
	 --config $(LEONOS_CONFIG_FILE) --output $(O_CONFIG)/components.mk --selection $@

MUSL_SDK_REQUIRED := $(MUSL_SDK)/bin/leonos-musl-cc $(MUSL_SDK)/include/stdio.h \
 $(MUSL_SDK)/include/pnglibconf.h $(MUSL_SDK)/lib/crt1.o $(MUSL_SDK)/lib/crti.o \
 $(MUSL_SDK)/lib/crtn.o $(MUSL_SDK)/lib/libc.a $(MUSL_SDK)/lib/libc.so \
 $(MUSL_SDK)/lib/libleonos.so.2 $(MUSL_SDK)/lib/libleonos.a
$(MUSL_SDK_REQUIRED) &: $(RUNTIME_SO) $(RUNTIME_ARCHIVE) $(RUNTIME_BUILTINS) \
 $(MUSL_STAMP) $(LEONOS_MUSL_ARTIFACTS) $(PAM_STAMP) $(AUTH_STAMP) $(SDK_INPUT_HEADERS) \
 $(HEADER_EXPORT_MANIFEST) \
 $(PNG_CONFIG) $(LEONOS_SDK_DRIVER) $(O_META)/sdk.sig $(LEONOS_SRC)/tools/build/musl-sdk.sh
	$(Q)set -eu; RUNTIME_BUILTINS=$(RUNTIME_BUILTINS) PNG_CONFIG=$(PNG_CONFIG) sh $(LEONOS_SRC)/tools/build/musl-sdk.sh $(LEONOS_SRC) $(MUSL_SYSROOT) $(RUNTIME_SO) $(RUNTIME_ARCHIVE) $(PAM_ROOT) $(AUTH_ROOT) $(LEONOS_SDK_DRIVER) $(MUSL_SDK) $(SDK_EPOCH) $(HEADER_EXPORT_INCLUDE)
	$(Q)set -eu; for product in $(MUSL_SDK_REQUIRED); do test -f "$$product" || exit 1; touch "$$product"; done
	$(Q)set -eu; find $(MUSL_SDK) -mindepth 1 ! -type d -printf '%P\n' | LC_ALL=C sort >$(MUSL_SDK).files

$(MUSL_SDK_ARCHIVE): $(MUSL_SDK_REQUIRED) $(O_META)/sdk.sig
	$(Q)set -eu; mkdir -p $(dir $@)
	$(Q)set -eu; tar --sort=name --mtime=@$(SDK_EPOCH) --owner=0 --group=0 --numeric-owner -cf $@.tar.tmp -C $(dir $(MUSL_SDK)) $(notdir $(MUSL_SDK))
	$(Q)set -eu; gzip -n -c $@.tar.tmp >$@.tmp
	$(Q)set -eu; mv $@.tmp $@
	$(Q)set -eu; rm $@.tar.tmp

.PHONY: musl-sdk sdk
musl-sdk: $(MUSL_SDK_ARCHIVE)
# `sdk` is the relocatable musl SDK built entirely from build output. The old
# checked-in devtools kit and the LeonOS4-Developer-SDK.zip are gone (user
# decision 2026-09-25); musl is the single C sysroot source.
sdk: $(MUSL_SDK_ARCHIVE)

# Every staged member participates in recovery, including non-primary headers,
# documentation and symlinks. Presence signatures are stable on intact trees.
define LEONOS_SDK_PRESENCE
$(O_META)/$(1)-present.sig: FORCE $(LEONOS_SRC)/tools/build/upstream-verify.sh
	$$(Q)sh $(LEONOS_SRC)/tools/build/upstream-verify.sh $(2) $(2).files $$@
$(3): $(O_META)/$(1)-present.sig
endef
$(eval $(call LEONOS_SDK_PRESENCE,musl-sdk,$(MUSL_SDK),$(MUSL_SDK_REQUIRED)))
