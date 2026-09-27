# PortableGL and freestanding StardustUI, using the same ABI as their callers.
PORTABLEGL_SO := $(O)/system/lib/libportablegl.so.1
PORTABLEGL_ARCHIVE := $(O)/userland/libportablegl.a
GLXGEARS_SOURCE := $(O_GENERATED)/glxgears/gears-upstream.c
RELIEFOS_GEARS_TOOL := $(RELIEFOS_HOST_BIN)/reliefos-gears
$(RELIEFOS_GEARS_TOOL): $(O_HOST)/obj/tools/host/manifest/reliefos-gears.c.o $(RELIEFOS_HOST_COMMON_OBJS) | $(RELIEFOS_HOST_BIN)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
$(GLXGEARS_SOURCE): $(RELIEFOS_SRC)/third_party/portablegl/examples/classic/gears.c $(RELIEFOS_GEARS_TOOL)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(RELIEFOS_GEARS_TOOL) --input $< --output $@
PORTABLEGL_OBJ := $(O_OBJ)/portablegl/reliefos_pgl.o
PORTABLEGL_FLAGS := $(filter-out -fPIE,$(USERLAND_FLAGS)) -std=c99 -Wno-unused-parameter -I$(RELIEFOS_SRC)/third_party/portablegl -I$(RELIEFOS_SRC)/userland/portablegl
RELIEFOS_SIG_portablegl := cc=$(TARGET_CC)|flags=$(PORTABLEGL_FLAGS)|ld=$(TARGET_LD)|ar=$(TARGET_AR)|policy=$(RELIEFOS_LINK_POLICY_FLAGS)
$(if $(RELIEFOS_PASSIVE),,$(eval $(call RELIEFOS_SIGNATURE_RULE,portablegl)))
$(PORTABLEGL_OBJ): $(RELIEFOS_SRC)/userland/portablegl/reliefos_pgl.c $(AUTOCONF_H) $(MUSL_STAMP) $(HEADER_EXPORT_MANIFEST) $(O_META)/portablegl.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(TARGET_CC) $(PORTABLEGL_FLAGS) -include $(AUTOCONF_H) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(PORTABLEGL_SO): $(PORTABLEGL_OBJ) $(RUNTIME_SO) $(MUSL_SYSROOT)/lib/libc.so $(MUSL_SYSROOT)/lib/libmimalloc.so.3 $(O_META)/portablegl.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(TARGET_LD) -shared --no-undefined --hash-style=both -z max-page-size=0x1000 -soname libportablegl.so.1 $(RELIEFOS_LINK_POLICY_FLAGS) -o $@.tmp $(PORTABLEGL_OBJ) -L$(MUSL_SYSROOT)/lib -l:libmimalloc.so.3 $(RUNTIME_SO) -lc
	$(Q)mv $@.tmp $@
$(PORTABLEGL_ARCHIVE): $(PORTABLEGL_OBJ) $(O_META)/portablegl.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)rm -f $@.tmp
	$(Q)$(TARGET_AR) rcsD $@.tmp $(PORTABLEGL_OBJ)
	$(Q)mv $@.tmp $@

STARDUSTUI_ARCHIVE := $(O)/userland/libstardustui.a
STARDUSTUI_SOURCES := third_party/stardustui/src/file.cpp third_party/stardustui/src/network.cpp third_party/stardustui/src/sytel.cpp third_party/stardustui/src/theme.cpp third_party/stardustui/src/window.cpp third_party/stardustui/src/text/font.cpp userland/stardustui/src/platform_leonos.cpp $(patsubst $(RELIEFOS_SRC)/%,%,$(wildcard $(RELIEFOS_SRC)/third_party/stardustui/src/components/*.cpp))
STARDUSTUI_OBJECTS := $(addprefix $(O_OBJ)/stardustui/,$(addsuffix .o,$(STARDUSTUI_SOURCES)))
STARDUSTUI_FLAGS := $(filter-out -std=c11 -fPIE,$(USERLAND_FLAGS)) -std=c++17 -nostdinc++ -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics -Wno-unused-parameter -DSTARDUSTUI_LINUX -I$(RELIEFOS_SRC)/userland/stardustui/include -I$(RELIEFOS_SRC)/third_party/stardustui/includes -I$(RELIEFOS_SRC)/third_party/stardustui
RELIEFOS_SIG_stardustui-cc := cxx=$(TARGET_CXX)|identity=$(if $(RELIEFOS_PASSIVE),deferred,$(shell $(TARGET_CXX) --version 2>/dev/null | head -n1))|flags=$(STARDUSTUI_FLAGS)
RELIEFOS_SIG_stardustui-ar := ar=$(TARGET_AR)|sources=$(STARDUSTUI_SOURCES)
$(if $(RELIEFOS_PASSIVE),,$(eval $(call RELIEFOS_SIGNATURE_RULE,stardustui-cc)))
$(if $(RELIEFOS_PASSIVE),,$(eval $(call RELIEFOS_SIGNATURE_RULE,stardustui-ar)))
$(O_OBJ)/stardustui/%.cpp.o: $(RELIEFOS_SRC)/%.cpp $(AUTOCONF_H) $(MUSL_STAMP) $(PNG_CONFIG) $(HEADER_EXPORT_MANIFEST) $(O_META)/stardustui-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(TARGET_CXX) $(STARDUSTUI_FLAGS) -include $(AUTOCONF_H) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	$(Q)mv $@.tmp $@
$(STARDUSTUI_ARCHIVE): $(STARDUSTUI_OBJECTS) $(O_META)/stardustui-ar.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)rm -f $@.tmp
	$(Q)$(TARGET_AR) rcsD $@.tmp $(STARDUSTUI_OBJECTS)
	$(Q)mv $@.tmp $@
STARDUSTUI_EXAMPLE_stardusthello := helloworld/helloworld.cpp
STARDUSTUI_EXAMPLE_stardustlayout := layout/layout.cpp
STARDUSTUI_EXAMPLE_stardustshowcase := showcase/showcase.cpp
define RELIEFOS_STARDUST_EXAMPLE
USERLAND_LIBS_$(1) := $(STARDUSTUI_ARCHIVE) $(O_OBJ)/stardustui/third_party/stardustui/examples/$$(STARDUSTUI_EXAMPLE_$(1)).o
$$(eval $$(call RELIEFOS_APP,$(1),$(1),$(USERLAND_DIR)/$(1).elf,$(AUTOCONF_H),$(RUNTIME_SO)))
endef
$(foreach app,$(filter stardusthello stardustlayout stardustshowcase,$(RELIEFOS_COMPONENT_APPS)),$(eval $(call RELIEFOS_STARDUST_EXAMPLE,$(app))))
.PHONY: portablegl stardustui
portablegl: $(PORTABLEGL_SO) $(PORTABLEGL_ARCHIVE)
stardustui: $(STARDUSTUI_ARCHIVE)
userland: $(if $(filter portablegl,$(RELIEFOS_COMPONENTS_ENABLED)),$(PORTABLEGL_SO) $(PORTABLEGL_ARCHIVE)) $(if $(filter stardustui,$(RELIEFOS_COMPONENTS_ENABLED)),$(STARDUSTUI_ARCHIVE))
-include $(PORTABLEGL_OBJ).d $(addsuffix .d,$(STARDUSTUI_OBJECTS)) $(foreach app,stardusthello stardustlayout stardustshowcase,$(O_OBJ)/stardustui/third_party/stardustui/examples/$(STARDUSTUI_EXAMPLE_$(app)).o.d)
