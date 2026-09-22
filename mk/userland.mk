# Component selection is parsed by a constrained C TOML reader, never a shell
# pattern. Required applications ignore obsolete disabled config symbols.
LEONOS_COMPONENT_TOOL := $(LEONOS_HOST_BIN)/leonos-components
LEONOS_COMPONENT_MK := $(O_CONFIG)/components.mk
$(LEONOS_COMPONENT_TOOL): $(O_HOST)/obj/tools/host/manifest/leonos-components.c.o $(LEONOS_HOST_COMMON_OBJS) | $(LEONOS_HOST_BIN)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
$(LEONOS_COMPONENT_MK): $(LEONOS_SRC)/configs/components.toml $(LEONOS_CONFIG_FILE) $(LEONOS_COMPONENT_TOOL)
	$(Q)$(LEONOS_COMPONENT_TOOL) --input $(LEONOS_SRC)/configs/components.toml --config $(LEONOS_CONFIG_FILE) --output $@
ifeq ($(LEONOS_PASSIVE),)
ifneq ($(LEONOS_INSPECT),)
$(eval $(file <$(LEONOS_COMPONENT_MK)))
else
include $(LEONOS_COMPONENT_MK)
endif
endif

USERLAND_EXTERNAL_APPS := fastfetch pleditor stardusthello stardustlayout stardustshowcase
USERLAND_APPS := $(filter-out $(USERLAND_EXTERNAL_APPS),$(LEONOS_COMPONENT_APPS))
USERLAND_DIR := $(O)/userland
USERLAND_FLAGS := $(RUNTIME_FLAGS) -fPIE -nostdinc -isystem $(if $(LEONOS_PASSIVE),deferred,$(shell $(TARGET_CC) -print-resource-dir 2>/dev/null))/include -D_POSIX_C_SOURCE=200809L
USERLAND_CFLAGS ?=
USERLAND_LDFLAGS ?=
USERLAND_EXTRA_doom := -DLEONOS_DOOM -DFEATURE_SOUND -I$(LEONOS_SRC)/third_party/doomgeneric/doomgeneric
USERLAND_EXTRA_mp3play := -I$(LEONOS_SRC)/third_party/minimp3
USERLAND_EXTRA_glxgears = -I$(LEONOS_SRC)/third_party/portablegl -I$(LEONOS_SRC)/userland/apps/glxgears -I$(dir $(GLXGEARS_SOURCE))
PORTABLEGL_SO ?= $(O)/userland/libportablegl.so.1
GLXGEARS_SOURCE ?= $(O_GENERATED)/glxgears/gears-upstream.c
USERLAND_LIBS_glxgears = $(PORTABLEGL_SO)
USERLAND_DEPS_glxgears = $(GLXGEARS_SOURCE)
USERLAND_DOOM_EXCLUDE := doomgeneric_allegro.c doomgeneric_emscripten.c doomgeneric_linuxvt.c doomgeneric_sdl.c doomgeneric_soso.c doomgeneric_sosox.c doomgeneric_win.c doomgeneric_xlib.c i_allegromusic.c i_allegrosound.c i_sdlsound.c i_sdlmusic.c i_cdmus.c mus2mid.c
USERLAND_DOOM_SOURCES := $(filter-out $(addprefix third_party/doomgeneric/doomgeneric/,$(USERLAND_DOOM_EXCLUDE)),$(patsubst $(LEONOS_SRC)/%,%,$(wildcard $(LEONOS_SRC)/third_party/doomgeneric/doomgeneric/*.c)))
userland_sources = $(sort $(patsubst $(LEONOS_SRC)/%,%,$(wildcard $(LEONOS_SRC)/userland/apps/$(1)/*.c $(LEONOS_SRC)/userland/apps/$(1)/*.S)) $(if $(filter doom,$(1)),$(USERLAND_DOOM_SOURCES)))
USERLAND_CRT := $(MUSL_SYSROOT)/lib/Scrt1.o $(MUSL_SYSROOT)/lib/crti.o $(MUSL_SYSROOT)/lib/crtn.o
USERLAND_LINK_FLAGS := $(LEONOS_LINK_POLICY_FLAGS) --gc-sections -z max-page-size=0x1000 -pie --hash-style=both --dynamic-linker /lib/ld-musl-x86_64.so.1 -rpath /usr/lib/leonos:/lib:/usr/lib

# Arguments: logical name, source app, output, autoconf, runtime.
# Source membership is in the link signature, so removing a source relinks.
define LEONOS_APP
USERLAND_SOURCES_$(1) := $$(call userland_sources,$(2))
USERLAND_OBJECTS_$(1) := $$(addprefix $(O_OBJ)/app-$(1)/,$$(addsuffix .o,$$(USERLAND_SOURCES_$(1))))
LEONOS_SIG_app-$(1)-cc := cc=$(TARGET_CC)|identity=$$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)|flags=$(USERLAND_FLAGS) $(USERLAND_CFLAGS) $$(USERLAND_EXTRA_$(2))|config=$(4)
LEONOS_SIG_app-$(1)-ld := ld=$(TARGET_LD)|identity=$$(shell $(TARGET_LD) --version 2>/dev/null | head -n1)|flags=$(USERLAND_LINK_FLAGS) $(USERLAND_LDFLAGS)|sources=$$(USERLAND_SOURCES_$(1))|libs=$(5) $$(USERLAND_LIBS_$(2))
$$(if $$(LEONOS_PASSIVE),,$$(eval $$(call LEONOS_SIGNATURE_RULE,app-$(1)-cc)))
$$(if $$(LEONOS_PASSIVE),,$$(eval $$(call LEONOS_SIGNATURE_RULE,app-$(1)-ld)))
$(O_OBJ)/app-$(1)/%.c.o: $(LEONOS_SRC)/%.c $(4) $(MUSL_STAMP) $(PNG_CONFIG) $$(USERLAND_DEPS_$(2)) $(O_META)/app-$(1)-cc.sig
	$$(Q)mkdir -p $$(dir $$@)
	$$(call LEONOS_LOG,CC,$$<)
	$$(Q)$$(TARGET_CC) $$(USERLAND_FLAGS) $$(USERLAND_CFLAGS) $$(USERLAND_EXTRA_$(2)) -include $(4) -MMD -MP -MF $$@.d -MT $$@ -c $$< -o $$@.tmp
	$$(Q)mv $$@.tmp $$@
$(O_OBJ)/app-$(1)/%.S.o: $(LEONOS_SRC)/%.S $(4) $(O_META)/app-$(1)-cc.sig
	$$(Q)mkdir -p $$(dir $$@)
	$$(Q)$$(TARGET_CC) --target=$$(TRIPLE_USER) -fPIC -I$(LEONOS_SRC)/include/uapi -MMD -MP -MF $$@.d -MT $$@ -c $$< -o $$@.tmp
	$$(Q)mv $$@.tmp $$@
$(3): $$(USERLAND_OBJECTS_$(1)) $(5) $$(USERLAND_LIBS_$(2)) $(USERLAND_CRT) $(MUSL_SYSROOT)/lib/libc.so $(MUSL_SYSROOT)/lib/libmimalloc.so.3 $(O_META)/app-$(1)-ld.sig
	$$(Q)mkdir -p $$(dir $$@)
	$$(call LEONOS_LOG,LD,$$@)
	$$(Q)$$(TARGET_LD) $$(USERLAND_LINK_FLAGS) $$(USERLAND_LDFLAGS) -o $$@.tmp $(MUSL_SYSROOT)/lib/Scrt1.o $(MUSL_SYSROOT)/lib/crti.o $$(USERLAND_OBJECTS_$(1)) -L$(MUSL_SYSROOT)/lib -l:libmimalloc.so.3 --start-group $(5) $$(USERLAND_LIBS_$(2)) -lc --end-group $(MUSL_SYSROOT)/lib/crtn.o
	$$(Q)mv $$@.tmp $$@
.PHONY: app-$(1)
app-$(1): $(3)
-include $$(addsuffix .d,$$(USERLAND_OBJECTS_$(1)))
endef
include $(LEONOS_SRC)/mk/components/graphics.mk
$(foreach app,$(USERLAND_APPS),$(eval $(call LEONOS_APP,$(app),$(app),$(USERLAND_DIR)/$(app).elf,$(AUTOCONF_H),$(RUNTIME_SO))))
$(foreach app,desktop settings,$(eval $(call LEONOS_APP,installer-$(app),$(app),$(O)/userland-installer-policy/$(app).elf,$(AUTOCONF_INSTALLER_H),$(RUNTIME_INSTALLER_SO))))
$(eval $(call LEONOS_APP,gptinit,gptinit,$(O)/userland-installer/gptinit.elf,$(AUTOCONF_INSTALLER_H),$(RUNTIME_INSTALLER_SO)))

# Only manifest-owned disabled ELF outputs may be removed; arbitrary files in
# this directory and old build/ images are outside this rule's ownership.
.PHONY: userland userland-prune installer-userland
userland-prune:
	$(Q)$(foreach app,$(LEONOS_DISABLED_APPS),rm -f $(USERLAND_DIR)/$(app).elf;) :
userland: userland-prune $(addprefix $(USERLAND_DIR)/,$(addsuffix .elf,$(LEONOS_COMPONENT_APPS)))
installer-userland: $(O)/userland-installer-policy/desktop.elf $(O)/userland-installer-policy/settings.elf $(O)/userland-installer/gptinit.elf

# Loader failures must still display a GUI when a shared library is absent.
# Keep this helper fully static, with the same mimalloc and musl startup policy.
DYNLINKERROR_OBJ := $(O_OBJ)/dynlinkerror/main.o
LEONOS_SIG_dynlinkerror := cc=$(TARGET_CC)|ld=$(TARGET_LD)|flags=$(USERLAND_FLAGS) $(USERLAND_CFLAGS) $(USERLAND_LDFLAGS)|builtins=$(RUNTIME_BUILTINS)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,dynlinkerror)))
$(DYNLINKERROR_OBJ): $(LEONOS_SRC)/userland/apps/dynlinkerror/main.c $(AUTOCONF_H) $(MUSL_STAMP) $(PNG_CONFIG) $(O_META)/dynlinkerror.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(TARGET_CC) $(USERLAND_FLAGS) $(USERLAND_CFLAGS) -include $(AUTOCONF_H) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(USERLAND_DIR)/dynlinkerror.elf: $(DYNLINKERROR_OBJ) $(RUNTIME_ARCHIVE) $(RUNTIME_BUILTINS) $(MUSL_SYSROOT)/lib/crt1.o $(MUSL_SYSROOT)/lib/crti.o $(MUSL_SYSROOT)/lib/crtn.o $(MUSL_SYSROOT)/lib/libc.a $(MUSL_SYSROOT)/lib/mimalloc.o $(O_META)/dynlinkerror.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(TARGET_LD) --gc-sections -z max-page-size=0x1000 $(LEONOS_LINK_POLICY_FLAGS) $(USERLAND_LDFLAGS) -static --image-base=0x4000000 -o $@.tmp $(MUSL_SYSROOT)/lib/crt1.o $(MUSL_SYSROOT)/lib/crti.o $(DYNLINKERROR_OBJ) -L$(MUSL_SYSROOT)/lib $(MUSL_SYSROOT)/lib/mimalloc.o --start-group $(RUNTIME_ARCHIVE) $(RUNTIME_BUILTINS) -lc --end-group $(MUSL_SYSROOT)/lib/crtn.o
	$(Q)mv $@.tmp $@
userland: $(USERLAND_DIR)/dynlinkerror.elf
-include $(DYNLINKERROR_OBJ).d

# PAM helper: ordinary musl/POSIX process, without a desktop component entry.
$(eval $(call LEONOS_APP,motd,motd,$(USERLAND_DIR)/motd.elf,$(AUTOCONF_H),))
userland: $(USERLAND_DIR)/motd.elf
