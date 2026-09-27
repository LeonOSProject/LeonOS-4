# Host toolchain, this project's own C helper programs, and the shared
# command-signature mechanism.

# --- which goals must stay side-effect free ---------------------------------
# help, clean and distclean may not probe a compiler, regenerate configuration
# or start production work (plan section 4). Deciding it once here stops the
# other fragments from attaching that work implicitly.
RELIEFOS_PASSIVE_GOALS := help doctor clean distclean test-legacy migrate-config regen-component-kconfig
RELIEFOS_INSPECT := $(or $(findstring n,$(firstword -$(MAKEFLAGS))),$(findstring q,$(firstword -$(MAKEFLAGS))))
ifeq ($(MAKECMDGOALS),)
	RELIEFOS_PASSIVE := 1
else ifeq ($(words $(filter $(RELIEFOS_PASSIVE_GOALS),$(MAKECMDGOALS))),$(words $(MAKECMDGOALS)))
	RELIEFOS_PASSIVE := 1
endif

# --- verbosity --------------------------------------------------------------
ifeq ($(V),1)
	Q :=
	SILENT :=
	MAKE_OUTPUT_SYNC :=
else
	Q := @
	SILENT := --silent
endif

# --- host compiler selection ------------------------------------------------
# HOSTCC is independent of the cross compiler in mk/toolchain.mk, and an
# environment value is ignored so an unrelated shell setting cannot change it.
ifeq ($(origin HOSTCC),undefined)
HOSTCC := cc
endif
HOST_CFLAGS ?= -O2 -g
HOST_LDFLAGS ?=

# The warning set the plan requires for all new C code (section 8).
RELIEFOS_STRICT_WARNINGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 \
	-Wshadow -Wstrict-prototypes -Wmissing-prototypes

# Generated headers are searched before the source tree so a stale committed
# header cannot shadow what this build just produced.
RELIEFOS_HOST_INCLUDES := -I$(RELIEFOS_SRC) -I$(O_INCLUDE)

# Sanitiser build used by `make test-tools`.
RELIEFOS_SANITISE := -fsanitize=address,undefined -fno-omit-frame-pointer

# --- source inventory -------------------------------------------------------
RELIEFOS_HOST_COMMON_SRCS := \
	tools/host/common/buffer.c \
	tools/host/common/io.c \
	tools/host/common/process.c

# Upstream reference code gets its own diagnostic scope instead of the whole
# project losing warnings: it is not ours to reformat (plan section 8).
RELIEFOS_HOST_PUFF_SRC := third_party/zlib/contrib/puff/puff.c
RELIEFOS_HOST_PUFF_WARNINGS := -std=c11 -O2 -w -I$(RELIEFOS_SRC)/third_party/zlib/contrib/puff

RELIEFOS_HOST_BIN := $(O_HOST)/bin
RELIEFOS_EMIT ?= $(if $(strip $(LEONOS_EMIT)),$(LEONOS_EMIT),$(RELIEFOS_HOST_BIN)/reliefos-emit)
RELIEFOS_CONFIG_TOOL ?= $(if $(strip $(LEONOS_CONFIG_TOOL)),$(LEONOS_CONFIG_TOOL),$(RELIEFOS_HOST_BIN)/reliefos-config)
RELIEFOS_VERSION_TOOL ?= $(if $(strip $(LEONOS_VERSION_TOOL)),$(LEONOS_VERSION_TOOL),$(RELIEFOS_HOST_BIN)/reliefos-version)
RELIEFOS_DEPS_TOOL ?= $(if $(strip $(LEONOS_DEPS_TOOL)),$(LEONOS_DEPS_TOOL),$(RELIEFOS_HOST_BIN)/reliefos-deps)
RELIEFOS_GBK_TOOL ?= $(if $(strip $(LEONOS_GBK_TOOL)),$(LEONOS_GBK_TOOL),$(RELIEFOS_HOST_BIN)/reliefos-gbk)
RELIEFOS_SDK_DRIVER ?= $(if $(strip $(LEONOS_SDK_DRIVER)),$(LEONOS_SDK_DRIVER),$(RELIEFOS_HOST_BIN)/reliefos-musl-cc)
RELIEFOS_APK_OWN ?= $(if $(strip $(LEONOS_APK_OWN)),$(LEONOS_APK_OWN),$(RELIEFOS_HOST_BIN)/reliefos-apk-own)
RELIEFOS_NLS_EXTRACT ?= $(if $(strip $(LEONOS_NLS_EXTRACT)),$(LEONOS_NLS_EXTRACT),$(RELIEFOS_HOST_BIN)/reliefos-nls-extract)

RELIEFOS_HOST_TOOLS := $(RELIEFOS_EMIT) $(RELIEFOS_CONFIG_TOOL) \
	$(RELIEFOS_VERSION_TOOL) $(RELIEFOS_DEPS_TOOL) $(RELIEFOS_GBK_TOOL) $(RELIEFOS_SDK_DRIVER)

RELIEFOS_HOST_TOOLS += $(RELIEFOS_APK_OWN)
# Registered here although mk/nls.mk owns the link rule: this list is what
# `make tools` builds, so an unlisted tool would never be compiled by the
# generic host rule with its strict warning set.
RELIEFOS_HOST_TOOLS += $(RELIEFOS_NLS_EXTRACT)

RELIEFOS_HOST_COMMON_OBJS := $(patsubst %.c,$(O_HOST)/obj/%.c.o,$(RELIEFOS_HOST_COMMON_SRCS))

# --- command signatures -----------------------------------------------------
# One signature per action class records the real argv, the absolute tool path
# and the tool identity. Objects depend on the signature rather than on FORCE, so
# overriding e.g. KERNEL_CFLAGS on the command line rebuilds exactly that class
# and leaves other profiles and components alone (plan section 6.2).
#
# The candidate text is written while parsing with $(file), which keeps arbitrary
# flag text out of a shell. The FORCE rule then promotes it only when the content
# differs, so an unchanged signature never moves its mtime and nothing
# downstream rebuilds.
#
# $(call RELIEFOS_SIGNATURE_RULE,class)
#
# cmp-then-move keeps this self-hosting: promoting a signature with reliefos-emit
# would need reliefos-emit built first, and that build depends on a signature.
# rename() preserves the candidate's fresh mtime, so the signature moves only
# when its content actually differed.
# The candidate carries an id unique to this make process: two makes that share
# an output tree parse with different flags (an override here, a different goal
# there), and a single fixed candidate name lets the later parse overwrite the
# earlier one, so a process would promote a signature it never computed.
# MAKEPID is only set on platforms that support it -- GNU Make leaves it empty on
# POSIX -- so the fallback is the pid of one subshell, expanded exactly once by
# the := below. Promoting is still compare-then-move, so the published .sig only
# ever moves when its content really changed.
RELIEFOS_PARSE_ID := $(or $(MAKEPID),$(shell echo $$$$))
RELIEFOS_CANDIDATE = $(O_META)/$(1).$(RELIEFOS_PARSE_ID).candidate

define RELIEFOS_SIGNATURE_RULE
$(if $(RELIEFOS_INSPECT),,$(file >$(call RELIEFOS_CANDIDATE,$(1)),$(strip $(RELIEFOS_SIG_$(1)))))

$(O_META)/$(1).sig: FORCE | $(O_META)
	$(Q)if cmp -s $(call RELIEFOS_CANDIDATE,$(1)) $$@ 2>/dev/null; then \
	    rm -f $(call RELIEFOS_CANDIDATE,$(1)); \
	else \
	    mv $(call RELIEFOS_CANDIDATE,$(1)) $$@; \
	fi
endef

# The candidate directory and file are created while parsing a production goal.
# Inspection goals still define signature rules, but never create their parse-time
# candidates; that lets fresh-output `make -n` resolve the same dependency graph.
#
# The ownership marker is written in the same breath as its own rule below,
# because a tree produced only by `make kernel` still has to be recognisable to
# `make clean`; a clean that refuses to touch its own output is a bug in clean.
$(if $(or $(RELIEFOS_PASSIVE),$(RELIEFOS_INSPECT)),,$(shell mkdir -p $(O_META) $(O_HOST)/obj $(RELIEFOS_HOST_BIN); \
	if [ ! -e $(RELIEFOS_O_MARKER) ]; then \
	    printf 'reliefos-build-out version=1 root=%s\n' '$(RELIEFOS_SRC)' \
	        > $(RELIEFOS_O_MARKER); \
	fi))

reliefos_host_tool_path := $(if $(RELIEFOS_PASSIVE),deferred,$(shell command -v $(HOSTCC) 2>/dev/null || echo unavailable))
reliefos_host_tool_identity := $(if $(RELIEFOS_PASSIVE),deferred,$(shell $(HOSTCC) --version 2>&1 | head -n1))

RELIEFOS_SIG_host-cc := argv=$(HOSTCC) $(RELIEFOS_STRICT_WARNINGS) $(RELIEFOS_HOST_INCLUDES) $(HOST_CFLAGS)|path=$(reliefos_host_tool_path)|identity=$(reliefos_host_tool_identity)

$(if $(RELIEFOS_PASSIVE),,$(eval $(call RELIEFOS_SIGNATURE_RULE,host-cc)))

# --- directories ------------------------------------------------------------
$(O_HOST) $(O_HOST)/obj $(RELIEFOS_HOST_BIN) $(O_META) $(O_INCLUDE) $(O_GENERATED) \
$(O_CONFIG) $(O_OBJ) $(O_SYSROOT) $(O_STAGE) $(O_PACKAGES) $(O_IMAGES) $(O_LOGS) \
$(O_INCLUDE)/generated $(O_GENERATED)/system $(O_CONFIG)/generated $(O_OBJ)/kernel:
	$(Q)mkdir -p $@

# Ownership marker for `clean`: it names the source root the tree belongs to.
$(RELIEFOS_O_MARKER): | $(O_META)
	$(Q)printf 'reliefos-build-out version=1 root=%s\n' '$(RELIEFOS_SRC)' > $@.tmp
	$(Q)mv $@.tmp $@

# --- host objects and programs ---------------------------------------------
$(O_HOST)/obj/tools/host/%.c.o: $(RELIEFOS_SRC)/tools/host/%.c $(O_META)/host-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(call RELIEFOS_LOG,HOSTCC,$<)
	$(Q)$(HOSTCC) $(RELIEFOS_STRICT_WARNINGS) $(RELIEFOS_HOST_INCLUDES) $(HOST_CFLAGS) \
		-MMD -MF $@.d -c $< -o $@

$(RELIEFOS_EMIT): $(O_HOST)/obj/tools/host/gen/reliefos-emit.c.o $(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(call RELIEFOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(RELIEFOS_CONFIG_TOOL): $(O_HOST)/obj/tools/host/config/reliefos-config.c.o $(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(call RELIEFOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(RELIEFOS_VERSION_TOOL): $(O_HOST)/obj/tools/host/version/reliefos-version.c.o $(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(call RELIEFOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

# The lock-file reader links the JSON module as well as the shared primitives.
RELIEFOS_JSON_OBJ := $(O_HOST)/obj/tools/host/manifest/json.c.o
$(RELIEFOS_SDK_DRIVER): $(O_HOST)/obj/tools/host/sdk/reliefos-musl-cc.c.o | $(RELIEFOS_HOST_BIN)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(RELIEFOS_GBK_TOOL): $(O_HOST)/obj/tools/host/assets/reliefos-gbk.c.o $(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(RELIEFOS_DEPS_TOOL): $(O_HOST)/obj/tools/host/manifest/reliefos-deps.c.o $(RELIEFOS_JSON_OBJ) \
	$(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(call RELIEFOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(RELIEFOS_APK_OWN): $(O_HOST)/obj/tools/host/apk/reliefos-apk-own.c.o $(RELIEFOS_JSON_OBJ) \
	$(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(call RELIEFOS_LOG,HOSTLD,$@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

-include $(shell find $(O_HOST)/obj -name '*.o.d' 2>/dev/null)

.PHONY: FORCE
FORCE:
