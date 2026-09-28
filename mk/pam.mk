# Linux-PAM is built with the audited project Make adapter, without Meson.
PAM_ROOT := $(O)/pam/root
PAM_WORK := $(O_THIRD_PARTY)/pam
PAM_STAMP := $(O)/pam/.complete
PAM_PORT := $(RELIEFOS_SRC)/tools/build/pam
PAM_LIB := $(PAM_ROOT)/lib/libpam.so.0
PAM_HEADER := $(PAM_ROOT)/usr/include/security/pam_appl.h
PAM_RELIEFOS_MODULE := $(PAM_ROOT)/lib/security/pam_leonos_password.so
PAM_INPUTS := $(PAM_PORT)/Makefile $(PAM_PORT)/configure.sh $(PAM_PORT)/install.sh $(wildcard $(RELIEFOS_SRC)/patches/linux-pam/*.patch)
RELIEFOS_SIG_pam := compiler=$(TARGET_CC)|target=$(TRIPLE_USER)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)
$(if $(RELIEFOS_PASSIVE),,$(eval $(call RELIEFOS_SIGNATURE_RULE,pam)))

$(PAM_STAMP) $(PAM_LIB) $(PAM_HEADER) $(PAM_RELIEFOS_MODULE) &: $(AUTH_STAMP) $(RELIEFOS_AUTH_ARTIFACTS) $(MUSL_STAMP) $(RELIEFOS_LOCK) $(PAM_INPUTS) $(HEADER_EXPORT_MANIFEST) $(RELIEFOS_SRC)/tools/build/pam-stage.sh $(O_META)/pam.sig $(RELIEFOS_SRC)/userland/pam/pam_leonos_password.c $(RELIEFOS_SRC)/userland/runtime/src/auth_password.c $(RELIEFOS_SRC)/include/leonos/auth.h
	$(Q)mkdir -p $(dir $(PAM_STAMP)) $(O_LOGS)
	+$(Q)case "$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; UPSTREAM_UAPI='$(abspath $(HEADER_EXPORT_INCLUDE))' sh $(RELIEFOS_SRC)/tools/build/pam-stage.sh $(RELIEFOS_SRC) $(abspath $(RELIEFOS_DEPS_TOOL)) $(RELIEFOS_LOCK) $(RELIEFOS_CACHE) \
	 $(abspath $(PAM_WORK)) $(abspath $(PAM_ROOT)) $(abspath $(MUSL_SYSROOT)) $(abspath $(AUTH_ROOT)) $(TARGET_CC) $(TRIPLE_USER) \
	 >$(O_LOGS)/pam.log 2>&1 || { tail -n 40 $(O_LOGS)/pam.log >&2; exit 1; }
	$(Q)touch $(PAM_STAMP)
	$(Q)touch $(PAM_LIB) $(PAM_HEADER) $(PAM_RELIEFOS_MODULE)
.PHONY: reliefos-pam leonos-pam
reliefos-pam: $(PAM_STAMP) $(PAM_LIB) $(PAM_HEADER) $(PAM_RELIEFOS_MODULE)
leonos-pam: reliefos-pam
