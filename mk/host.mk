# Host toolchain, this project's own C helper programs, and the shared
# command-signature mechanism.

# --- which goals must stay side-effect free ---------------------------------
# help, clean and distclean may not probe a compiler, regenerate configuration
# or start production work (plan section 4). Deciding it once here stops the
# other fragments from attaching that work implicitly.
LEONOS_PASSIVE_GOALS := help clean distclean
ifeq ($(MAKECMDGOALS),)
	LEONOS_PASSIVE := 1
else ifeq ($(words $(filter $(LEONOS_PASSIVE_GOALS),$(MAKECMDGOALS))),$(words $(MAKECMDGOALS)))
	LEONOS_PASSIVE := 1
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
LEONOS_STRICT_WARNINGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 \
	-Wshadow -Wstrict-prototypes -Wmissing-prototypes

# Generated headers are searched before the source tree so a stale committed
# header cannot shadow what this build just produced.
LEONOS_HOST_INCLUDES := -I$(LEONOS_SRC) -I$(O_INCLUDE)

# Sanitiser build used by `make test-tools`.
LEONOS_SANITISE := -fsanitize=address,undefined -fno-omit-frame-pointer

# --- source inventory -------------------------------------------------------
LEONOS_HOST_COMMON_SRCS := \
	tools/host/common/buffer.c \
	tools/host/common/io.c \
	tools/host/common/process.c

# Upstream reference code gets its own diagnostic scope instead of the whole
# project losing warnings: it is not ours to reformat (plan section 8).
LEONOS_HOST_PUFF_SRC := third_party/zlib/contrib/puff/puff.c
LEONOS_HOST_PUFF_WARNINGS := -std=c11 -O2 -w -I$(LEONOS_SRC)/third_party/zlib/contrib/puff

LEONOS_HOST_BIN := $(O_HOST)/bin
LEONOS_EMIT := $(LEONOS_HOST_BIN)/leonos-emit
LEONOS_CONFIG_TOOL := $(LEONOS_HOST_BIN)/leonos-config
LEONOS_VERSION_TOOL := $(LEONOS_HOST_BIN)/leonos-version
LEONOS_BOOT_LOGO_TOOL := $(LEONOS_HOST_BIN)/leonos-boot-logo
LEONOS_DEPS_TOOL := $(LEONOS_HOST_BIN)/leonos-deps

LEONOS_HOST_TOOLS := $(LEONOS_EMIT) $(LEONOS_CONFIG_TOOL) \
	$(LEONOS_VERSION_TOOL) $(LEONOS_BOOT_LOGO_TOOL) $(LEONOS_DEPS_TOOL)

LEONOS_HOST_COMMON_OBJS := $(patsubst %.c,$(O_HOST)/obj/%.c.o,$(LEONOS_HOST_COMMON_SRCS))
LEONOS_HOST_PUFF_OBJ := $(O_HOST)/obj/$(LEONOS_HOST_PUFF_SRC).o

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
# $(call LEONOS_SIGNATURE_RULE,class)
#
# cmp-then-move keeps this self-hosting: promoting a signature with leonos-emit
# would need leonos-emit built first, and that build depends on a signature.
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
LEONOS_PARSE_ID := $(or $(MAKEPID),$(shell echo $$$$))
LEONOS_CANDIDATE = $(O_META)/$(1).$(LEONOS_PARSE_ID).candidate

define LEONOS_SIGNATURE_RULE
$(file >$(call LEONOS_CANDIDATE,$(1)),$(strip $(LEONOS_SIG_$(1))))

$(O_META)/$(1).sig: FORCE | $(O_META)
	$(Q)if cmp -s $(call LEONOS_CANDIDATE,$(1)) $$@ 2>/dev/null; then \
	    rm -f $(call LEONOS_CANDIDATE,$(1)); \
	else \
	    mv $(call LEONOS_CANDIDATE,$(1)) $$@; \
	fi
endef

# The candidate directory has to exist before parsing finishes. This is a
# deliberate parse-time side effect: `make -n` is not promised to be effect free
# (plan section 4), and a generated include has nowhere to live on a fresh
# output directory otherwise.
$(if $(LEONOS_PASSIVE),,$(shell mkdir -p $(O_META) $(O_HOST)/obj $(LEONOS_HOST_BIN)))

leonos_host_tool_path := $(if $(LEONOS_PASSIVE),deferred,$(shell command -v $(HOSTCC) 2>/dev/null || echo unavailable))
leonos_host_tool_identity := $(if $(LEONOS_PASSIVE),deferred,$(shell $(HOSTCC) --version 2>&1 | head -n1))

LEONOS_SIG_host-cc := argv=$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_HOST_INCLUDES) $(HOST_CFLAGS)|path=$(leonos_host_tool_path)|identity=$(leonos_host_tool_identity)

$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,host-cc)))

# --- directories ------------------------------------------------------------
$(O_HOST) $(O_HOST)/obj $(LEONOS_HOST_BIN) $(O_META) $(O_INCLUDE) $(O_GENERATED) \
$(O_CONFIG) $(O_OBJ) $(O_SYSROOT) $(O_STAGE) $(O_PACKAGES) $(O_IMAGES) $(O_LOGS) \
$(O_INCLUDE)/generated $(O_GENERATED)/system $(O_CONFIG)/generated $(O_OBJ)/kernel:
	$(Q)mkdir -p $@

# Ownership marker for `clean`: it names the source root the tree belongs to.
$(LEONOS_O_MARKER): | $(O_META)
	$(Q)printf 'leonos4-build-out version=1 root=%s\n' '$(LEONOS_SRC)' > $@.tmp
	$(Q)mv $@.tmp $@

# --- host objects and programs ---------------------------------------------
$(O_HOST)/obj/tools/host/%.c.o: $(LEONOS_SRC)/tools/host/%.c $(O_META)/host-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' HOSTCC $<
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_HOST_INCLUDES) $(HOST_CFLAGS) \
		-MMD -MF $@.d -c $< -o $@

$(LEONOS_HOST_PUFF_OBJ): $(LEONOS_SRC)/$(LEONOS_HOST_PUFF_SRC) $(O_META)/host-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' HOSTCC $<
	$(Q)$(HOSTCC) $(LEONOS_HOST_PUFF_WARNINGS) $(HOST_CFLAGS) \
		-MMD -MF $@.d -c $< -o $@

$(LEONOS_EMIT): $(O_HOST)/obj/tools/host/gen/leonos-emit.c.o $(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)printf '  %-8s %s\n' HOSTLD $@
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(LEONOS_CONFIG_TOOL): $(O_HOST)/obj/tools/host/config/leonos-config.c.o $(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)printf '  %-8s %s\n' HOSTLD $@
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(LEONOS_VERSION_TOOL): $(O_HOST)/obj/tools/host/version/leonos-version.c.o $(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)printf '  %-8s %s\n' HOSTLD $@
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(LEONOS_BOOT_LOGO_TOOL): $(O_HOST)/obj/tools/host/assets/leonos-boot-logo.c.o \
	$(LEONOS_HOST_PUFF_OBJ) $(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)printf '  %-8s %s\n' HOSTLD $@
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

# The lock-file reader links the JSON module as well as the shared primitives.
LEONOS_JSON_OBJ := $(O_HOST)/obj/tools/host/manifest/json.c.o
$(LEONOS_DEPS_TOOL): $(O_HOST)/obj/tools/host/manifest/leonos-deps.c.o $(LEONOS_JSON_OBJ) \
	$(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)printf '  %-8s %s\n' HOSTLD $@
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

-include $(shell find $(O_HOST)/obj -name '*.o.d' 2>/dev/null)

.PHONY: FORCE
FORCE:
