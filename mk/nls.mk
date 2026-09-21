# Locale / NLS pipeline.
#
# The tool variable and its LEONOS_HOST_TOOLS registration live in mk/host.mk;
# only the link rule is here, because the object comes from that file's generic
# host rule, which already matches tools/host/nls/*.c.
$(LEONOS_NLS_EXTRACT): $(O_HOST)/obj/tools/host/nls/leonos-nls-extract.c.o | $(LEONOS_HOST_BIN)
	$(call LEONOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

LEONOS_NLS_PO := $(LEONOS_SRC)/configs/nls/po
LEONOS_NLS_LANGS := $(strip $(shell cat $(LEONOS_SRC)/configs/nls/LINGUAS))
NLS_DIR := $(O_GENERATED)/nls
NLS_MO := $(addsuffix /LC_MESSAGES/leonos.mo,$(addprefix $(NLS_DIR)/,$(LEONOS_NLS_LANGS)))
NLS_MUSL_MO := $(addsuffix .UTF-8,$(addprefix $(O_GENERATED)/musl-locales/,$(LEONOS_NLS_LANGS)))
LEONOS_NLS_SOURCES := $(sort $(wildcard $(LEONOS_SRC)/userland/apps/*/*.c \
    $(LEONOS_SRC)/userland/apps/*/*.h $(LEONOS_SRC)/userland/libc/src/*.c $(LEONOS_SRC)/configs/nls/messages.c))
LEONOS_NLS_REL_SOURCES := $(patsubst $(LEONOS_SRC)/%,%,$(LEONOS_NLS_SOURCES))
LEONOS_SIG_nls := msgfmt=$(shell msgfmt --version 2>/dev/null | head -n1)|flags=-c --check --endianness=little|langs=$(LEONOS_NLS_LANGS)|sources=$(LEONOS_NLS_REL_SOURCES)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,nls)))

define leonos-nls-mo-rule
$(NLS_DIR)/$(1)/LC_MESSAGES/leonos.mo: $(LEONOS_NLS_PO)/$(1).po $(O_META)/nls.sig $(LEONOS_SRC)/mk/nls.mk
	$$(call LEONOS_LOG,MSGFMT,$$@)
	$$(Q)mkdir -p $$(@D)
	$$(Q)set -eu; trap 'rm -f $$@.tmp $$@.log' EXIT HUP INT TERM; \
	    if msgfmt -c --check --endianness=little -o $$@.tmp $$< >$$@.log 2>&1 && test ! -s $$@.log; then \
	        mv $$@.tmp $$@; \
	    else cat $$@.log >&2; exit 1; fi
endef
$(foreach lang,$(LEONOS_NLS_LANGS),$(eval $(call leonos-nls-mo-rule,$(lang))))

.PHONY: nls nls-update
nls: $(NLS_MO)

define leonos-musl-locale-rule
$(O_GENERATED)/musl-locales/$(1).UTF-8: $(LEONOS_SRC)/configs/locale/$(1).po $(LEONOS_SRC)/mk/nls.mk
	$$(call LEONOS_LOG,MSGFMT,$$@)
	$$(Q)mkdir -p $$(@D)
	$$(Q)msgfmt --check --endianness=little -o $$@ $$<
endef
$(foreach lang,$(LEONOS_NLS_LANGS),$(eval $(call leonos-musl-locale-rule,$(lang))))

.PHONY: musl-locales
musl-locales: $(NLS_MUSL_MO)

# Explicit maintenance only: normal builds never rewrite the source corpus.
nls-update: $(LEONOS_NLS_EXTRACT)
	$(Q)mkdir -p $(NLS_DIR)
	$(Q)cd $(LEONOS_SRC) && $(abspath $(LEONOS_NLS_EXTRACT)) $(abspath $(NLS_DIR))/leonos.pot $(LEONOS_NLS_REL_SOURCES)
	$(Q)set -eu; cd $(LEONOS_SRC); for lang in $(LEONOS_NLS_LANGS); do \
	    $(abspath $(LEONOS_NLS_EXTRACT)) --merge $(LEONOS_NLS_PO)/$$lang.po $(abspath $(NLS_DIR))/$$lang.po $(LEONOS_NLS_REL_SOURCES); \
	    msgfmt -c --check --endianness=little -o /dev/null $(abspath $(NLS_DIR))/$$lang.po; \
	 done
	$(Q)cp $(NLS_DIR)/leonos.pot $(LEONOS_NLS_PO)/leonos.pot
	$(Q)set -eu; for lang in $(LEONOS_NLS_LANGS); do cp $(NLS_DIR)/$$lang.po $(LEONOS_NLS_PO)/$$lang.po; done
