# Relocatable musl sysroot and the complete LeonOS developer kit.
MUSL_SDK := $(O)/sdk/leonos-musl-sdk
MUSL_SDK_ARCHIVE := $(O_PACKAGES)/leonos-musl-sdk.tar.gz
SDK_EPOCH := $(or $(SOURCE_DATE_EPOCH),$(shell git -C $(LEONOS_SRC) show -s --format=%ct HEAD))
SDK_INPUT_HEADERS := $(shell find $(LEONOS_SRC)/include/uapi $(LEONOS_SRC)/include/leonos $(LEONOS_SRC)/userland/libc/include/leonos -type f -name '*.h' | LC_ALL=C sort)
SDK_TEMPLATE_INPUTS := $(shell find $(LEONOS_SRC)/devtools -type f | LC_ALL=C sort)
SDK_SOURCE_INPUTS := $(shell find $(LEONOS_SRC)/userland/stardustui/include $(LEONOS_SRC)/third_party/stardustui/includes $(LEONOS_SRC)/userland/lua -type f | LC_ALL=C sort)
SDK_SOURCE_INPUTS += $(wildcard $(LEONOS_SRC)/third_party/lua/* $(LEONOS_SRC)/third_party/zlib/LICENSE $(LEONOS_SRC)/third_party/libpng/LICENSE $(LEONOS_SRC)/third_party/stardustui/LICENSE $(LEONOS_SRC)/third_party/sqlite/LICENSE.md)
SDK_SOURCE_INPUTS += $(shell find $(LEONOS_SRC)/third_party/stardustui/platforms $(LEONOS_SRC)/third_party/stardustui/examples -type f | LC_ALL=C sort)
SDK_SOURCE_INPUTS += $(LEONOS_SRC)/third_party/stardustui/settings.hpp $(LEONOS_SRC)/third_party/file/COPYING $(LEONOS_SRC)/third_party/portablegl/LICENSE $(LEONOS_SRC)/userland/apps/lua/lua.app.ini
SDK_SOURCE_INPUTS += $(LEONOS_SRC)/tools/build/sdk-versions.sh $(LEONOS_LOCK)
DEVELOPER_SDK := $(O)/sdk/devtools
DEVELOPER_SDK_ARCHIVE := $(O_PACKAGES)/LeonOS4-Developer-SDK.zip
SDK_EXTRA := $(O)/sdk/developer-extra
SDK_EXTRA_STAMP := $(SDK_EXTRA)/.complete
SDK_ZLIB_ARCHIVE := $(O)/sdk/libz.a
SDK_PNG_ARCHIVE := $(O)/sdk/libpng.a
SDK_ZLIB_OBJECTS := $(filter $(O_OBJ)/runtime/third_party/zlib/%,$(RUNTIME_OBJECTS))
SDK_PNG_OBJECTS := $(filter $(O_OBJ)/runtime/third_party/libpng/%,$(RUNTIME_OBJECTS))
SDK_OPTIONAL_INPUTS :=
SDK_EXAMPLES := $(filter stardusthello stardustlayout stardustshowcase,$(LEONOS_COMPONENTS_SDK))
SDK_OPTIONAL_INPUTS += $(addprefix $(USERLAND_DIR)/,$(addsuffix .elf,$(SDK_EXAMPLES)))
ifneq ($(filter ncurses,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(UPSTREAM_ROOT)/ncurses/root/.complete $(upstream_ncurses_products)
endif
ifneq ($(filter file,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(UPSTREAM_APP_DIR)/libmagic.a $(MAGIC_SO) $(UPSTREAM_APP_DIR)/magic.h
endif
ifneq ($(filter lua,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(UPSTREAM_APP_DIR)/liblua.a $(LUA_SO) $(USERLAND_DIR)/lua.elf
endif
ifneq ($(filter sqlite,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(UPSTREAM_APP_DIR)/libsqlite3.a $(SQLITE_SO) $(SQLITE_HEADER)
endif
ifneq ($(filter portablegl,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(PORTABLEGL_ARCHIVE) $(PORTABLEGL_SO)
endif
ifneq ($(filter stardustui,$(LEONOS_COMPONENTS_SDK)),)
SDK_OPTIONAL_INPUTS += $(STARDUSTUI_ARCHIVE)
endif
LEONOS_SIG_sdk := epoch=$(SDK_EPOCH)|headers=$(SDK_INPUT_HEADERS)|templates=$(SDK_TEMPLATE_INPUTS)|sources=$(SDK_SOURCE_INPUTS)|driver=$(LEONOS_SDK_DRIVER)|components=$(LEONOS_COMPONENTS_SDK)
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
 $(PNG_CONFIG) $(LEONOS_SDK_DRIVER) $(O_META)/sdk.sig $(LEONOS_SRC)/tools/build/musl-sdk.sh
	$(Q)set -eu; RUNTIME_BUILTINS=$(RUNTIME_BUILTINS) PNG_CONFIG=$(PNG_CONFIG) sh $(LEONOS_SRC)/tools/build/musl-sdk.sh $(LEONOS_SRC) $(MUSL_SYSROOT) $(RUNTIME_SO) $(RUNTIME_ARCHIVE) $(PAM_ROOT) $(AUTH_ROOT) $(LEONOS_SDK_DRIVER) $(MUSL_SDK) $(SDK_EPOCH)
	$(Q)set -eu; for product in $(MUSL_SDK_REQUIRED); do test -f "$$product" || exit 1; touch "$$product"; done
	$(Q)set -eu; find $(MUSL_SDK) -mindepth 1 ! -type d -printf '%P\n' | LC_ALL=C sort >$(MUSL_SDK).files

$(MUSL_SDK_ARCHIVE): $(MUSL_SDK_REQUIRED) $(O_META)/sdk.sig
	$(Q)set -eu; mkdir -p $(dir $@)
	$(Q)set -eu; tar --sort=name --mtime=@$(SDK_EPOCH) --owner=0 --group=0 --numeric-owner -cf $@.tar.tmp -C $(dir $(MUSL_SDK)) $(notdir $(MUSL_SDK))
	$(Q)set -eu; gzip -n -c $@.tar.tmp >$@.tmp
	$(Q)set -eu; mv $@.tmp $@
	$(Q)set -eu; rm $@.tar.tmp

$(SDK_ZLIB_ARCHIVE): $(SDK_ZLIB_OBJECTS) $(O_META)/runtime-link.sig $(LEONOS_SRC)/mk/sdk.mk
	$(Q)set -eu; mkdir -p $(dir $@); rm -f $@.tmp
	$(Q)set -eu; $(TARGET_AR) rcsD $@.tmp $(SDK_ZLIB_OBJECTS)
	$(Q)set -eu; mv $@.tmp $@

$(SDK_PNG_ARCHIVE): $(SDK_PNG_OBJECTS) $(O_META)/runtime-link.sig $(LEONOS_SRC)/mk/sdk.mk
	$(Q)set -eu; mkdir -p $(dir $@); rm -f $@.tmp
	$(Q)set -eu; $(TARGET_AR) rcsD $@.tmp $(SDK_PNG_OBJECTS)
	$(Q)set -eu; mv $@.tmp $@

$(SDK_EXTRA_STAMP): $(LEONOS_DEPS_TOOL) $(COMPONENT_METADATA) $(SDK_ZLIB_ARCHIVE) $(SDK_PNG_ARCHIVE) $(PAM_STAMP) $(AUTH_STAMP) \
 $(COMPONENT_SELECTION) $(SDK_OPTIONAL_INPUTS) $(SDK_SOURCE_INPUTS) $(O_META)/sdk.sig $(LEONOS_SRC)/mk/sdk.mk
	$(Q)set -eu; rm -rf $(SDK_EXTRA).tmp
	$(Q)set -eu; mkdir -p $(SDK_EXTRA).tmp/lib $(SDK_EXTRA).tmp/include/leonos $(SDK_EXTRA).tmp/THIRD_PARTY $(SDK_EXTRA).tmp/share/leonos $(SDK_EXTRA).tmp/lib/pkgconfig
	$(Q)set -eu; sh $(LEONOS_SRC)/tools/build/sdk-versions.sh $(LEONOS_DEPS_TOOL) $(LEONOS_LOCK) $(COMPONENT_METADATA) $(SDK_EXTRA).tmp/THIRD_PARTY
	$(Q)set -eu; cp $(SDK_ZLIB_ARCHIVE) $(SDK_EXTRA).tmp/lib/libz.a
	$(Q)set -eu; cp $(SDK_PNG_ARCHIVE) $(SDK_EXTRA).tmp/lib/libpng.a
	$(Q)set -eu; cp $(LEONOS_SRC)/third_party/zlib/LICENSE $(SDK_EXTRA).tmp/THIRD_PARTY/ZLIB-LICENSE
	$(Q)set -eu; cp $(LEONOS_SRC)/third_party/libpng/LICENSE $(SDK_EXTRA).tmp/THIRD_PARTY/LIBPNG-LICENSE
	$(Q)set -eu; if test '$(CONFIG_SDK_INCLUDE_COMPONENT_METADATA)' = y; then cp $(COMPONENT_SELECTION) $(SDK_EXTRA).tmp/share/leonos/components.json; fi
	$(Q)set -eu; cp $(PAM_WORK)/src/Linux-PAM-1.7.2/Copyright $(SDK_EXTRA).tmp/THIRD_PARTY/LINUX-PAM-COPYRIGHT
	$(Q)set -eu; cp $(AUTH_WORK)/src/libxcrypt-4.5.2/COPYING.LIB $(SDK_EXTRA).tmp/THIRD_PARTY/LIBXCRYPT-COPYING.LIB
	$(Q)set -eu; cp $(AUTH_WORK)/src/libxcrypt-4.5.2/LICENSING $(SDK_EXTRA).tmp/THIRD_PARTY/LIBXCRYPT-LICENSING
	$(Q)set -eu; cp $(AUTH_STAMP) $(SDK_EXTRA).tmp/THIRD_PARTY/LIBXCRYPT-BUILD.json
	$(Q)set -eu; printf '{"builder":"tools/build/pam","version":"1.7.2","target":"%s"}\n' '$(TRIPLE_USER)' >$(SDK_EXTRA).tmp/THIRD_PARTY/LINUX-PAM-BUILD.json
	$(Q)set -eu; if test -d $(PAM_ROOT)/lib/pkgconfig; then for pc in $(PAM_ROOT)/lib/pkgconfig/*.pc; do test -f "$$pc" || continue; sed -e 's|^prefix=.*|prefix=$${pcfiledir}/../..|' -e 's|^libdir=.*|libdir=$${prefix}/lib|' -e 's|^includedir=.*|includedir=$${prefix}/include|' "$$pc" >$(SDK_EXTRA).tmp/lib/pkgconfig/$${pc##*/}; done; fi
	$(Q)set -eu; if test -n "$(filter ncurses,$(LEONOS_COMPONENTS_SDK))"; then cp -a $(UPSTREAM_ROOT)/ncurses/root/usr/include/. $(SDK_EXTRA).tmp/include/; cp -a $(UPSTREAM_ROOT)/ncurses/root/usr/lib/. $(SDK_EXTRA).tmp/lib/; if test -d $(UPSTREAM_ROOT)/ncurses/root/usr/share; then cp -a $(UPSTREAM_ROOT)/ncurses/root/usr/share/. $(SDK_EXTRA).tmp/share/; fi; fi
	$(Q)set -eu; if test -n "$(filter file,$(LEONOS_COMPONENTS_SDK))"; then cp $(UPSTREAM_APP_DIR)/libmagic.a $(MAGIC_SO) $(SDK_EXTRA).tmp/lib/; cp $(UPSTREAM_APP_DIR)/magic.h $(SDK_EXTRA).tmp/include/; cp $(LEONOS_SRC)/third_party/file/COPYING $(SDK_EXTRA).tmp/THIRD_PARTY/LIBMAGIC-COPYING; fi
	$(Q)set -eu; if test -n "$(filter lua,$(LEONOS_COMPONENTS_SDK))"; then cp $(UPSTREAM_APP_DIR)/liblua.a $(LUA_SO) $(SDK_EXTRA).tmp/lib/; mkdir -p $(SDK_EXTRA).tmp/include/lua5.4; cp $(LEONOS_SRC)/third_party/lua/lua.h $(LEONOS_SRC)/third_party/lua/lauxlib.h $(LEONOS_SRC)/third_party/lua/lualib.h $(LEONOS_SRC)/third_party/lua/luaconf.h $(SDK_EXTRA).tmp/include/lua5.4/; fi
	$(Q)set -eu; if test -n "$(filter sqlite,$(LEONOS_COMPONENTS_SDK))"; then cp $(UPSTREAM_APP_DIR)/libsqlite3.a $(SDK_EXTRA).tmp/lib/sqlite.a; cp $(SQLITE_SO) $(SDK_EXTRA).tmp/lib/; cp $(SQLITE_HEADER) $(SDK_EXTRA).tmp/include/; fi
	$(Q)set -eu; if test -n "$(filter portablegl,$(LEONOS_COMPONENTS_SDK))"; then cp $(PORTABLEGL_ARCHIVE) $(PORTABLEGL_SO) $(SDK_EXTRA).tmp/lib/; cp $(LEONOS_SRC)/third_party/portablegl/portablegl.h $(SDK_EXTRA).tmp/include/; cp $(LEONOS_SRC)/userland/libc/include/leonos/pgl.h $(SDK_EXTRA).tmp/include/leonos/pgl.h; cp $(LEONOS_SRC)/third_party/portablegl/LICENSE $(SDK_EXTRA).tmp/THIRD_PARTY/PORTABLEGL-LICENSE; fi
	$(Q)set -eu; if test -n "$(filter stardustui,$(LEONOS_COMPONENTS_SDK))"; then cp $(STARDUSTUI_ARCHIVE) $(SDK_EXTRA).tmp/lib/; mkdir -p $(SDK_EXTRA).tmp/include/stardustui; cp -a $(LEONOS_SRC)/third_party/stardustui/includes $(LEONOS_SRC)/third_party/stardustui/platforms $(LEONOS_SRC)/third_party/stardustui/settings.hpp $(SDK_EXTRA).tmp/include/stardustui/; fi
	$(Q)set -eu; if test -n "$(filter stardustui,$(LEONOS_COMPONENTS_SDK))"; then mkdir -p $(SDK_EXTRA).tmp/include/stardustui/leonos; cp -a $(LEONOS_SRC)/userland/stardustui/include/. $(SDK_EXTRA).tmp/include/stardustui/leonos/; cp $(LEONOS_SRC)/third_party/stardustui/LICENSE $(SDK_EXTRA).tmp/THIRD_PARTY/STARDUSTUI-LICENSE; fi
	$(Q)set -eu; if test -n "$(filter lua,$(LEONOS_COMPONENTS_SDK))"; then mkdir -p $(SDK_EXTRA).tmp/components/lua/bin; cp $(USERLAND_DIR)/lua.elf $(SDK_EXTRA).tmp/components/lua/bin/; cp -a $(LEONOS_SRC)/third_party/lua $(SDK_EXTRA).tmp/components/lua/upstream; cp -a $(LEONOS_SRC)/userland/lua $(SDK_EXTRA).tmp/components/lua/port; rm -f $(SDK_EXTRA).tmp/components/lua/upstream/.git; cp $(LEONOS_SRC)/userland/apps/lua/lua.app.ini $(SDK_EXTRA).tmp/components/lua/; cp $(LEONOS_SRC)/userland/lua/LICENSE $(SDK_EXTRA).tmp/THIRD_PARTY/LUA-LICENSE; fi
	$(Q)set -eu; if test -n "$(filter sqlite,$(LEONOS_COMPONENTS_SDK))"; then cp $(LEONOS_SRC)/third_party/sqlite/LICENSE.md $(SDK_EXTRA).tmp/THIRD_PARTY/SQLITE-LICENSE; fi
	$(Q)set -eu; $(foreach app,$(SDK_EXAMPLES),mkdir -p $(SDK_EXTRA).tmp/components/$(app)/bin; cp $(USERLAND_DIR)/$(app).elf $(SDK_EXTRA).tmp/components/$(app)/bin/; cp $(LEONOS_SRC)/third_party/stardustui/examples/$(STARDUSTUI_EXAMPLE_$(app)) $(SDK_EXTRA).tmp/components/$(app)/example.cpp; cp $(LEONOS_SRC)/userland/apps/$(app)/main.c $(SDK_EXTRA).tmp/components/$(app)/leonos-main.c;)
	$(Q)set -eu; touch $(SDK_EXTRA).tmp/.complete
	$(Q)set -eu; rm -rf $(SDK_EXTRA); mv $(SDK_EXTRA).tmp $(SDK_EXTRA)
	$(Q)set -eu; find $(SDK_EXTRA) -mindepth 1 ! -type d -printf '%P\n' | LC_ALL=C sort >$(SDK_EXTRA).files

DEVELOPER_SDK_REQUIRED := $(DEVELOPER_SDK)/Makefile $(DEVELOPER_SDK)/bin/leonos-musl-cc \
 $(DEVELOPER_SDK)/include/stdio.h $(DEVELOPER_SDK)/lib/crt1.o \
 $(DEVELOPER_SDK)/lib/libleonos.a $(DEVELOPER_SDK)/lib/libz.a $(DEVELOPER_SDK)/lib/libpng.a
$(DEVELOPER_SDK_REQUIRED) $(DEVELOPER_SDK_ARCHIVE) &: $(MUSL_SDK_REQUIRED) $(SDK_EXTRA_STAMP) \
 $(SDK_TEMPLATE_INPUTS) $(LEONOS_SRC)/tools/build/developer-sdk.sh $(LEONOS_SRC)/mk/sdk.mk $(O_META)/sdk.sig
	$(Q)set -eu; sh $(LEONOS_SRC)/tools/build/developer-sdk.sh $(LEONOS_SRC) $(MUSL_SDK) $(SDK_EXTRA) $(DEVELOPER_SDK) $(DEVELOPER_SDK_ARCHIVE) $(SDK_EPOCH)
	$(Q)set -eu; for product in $(DEVELOPER_SDK_REQUIRED) $(DEVELOPER_SDK_ARCHIVE); do test -f "$$product" || exit 1; touch "$$product"; done
	$(Q)set -eu; find $(DEVELOPER_SDK) -mindepth 1 ! -type d -printf '%P\n' | LC_ALL=C sort >$(DEVELOPER_SDK).files

.PHONY: musl-sdk sdk
musl-sdk: $(MUSL_SDK_ARCHIVE)
sdk: $(DEVELOPER_SDK_ARCHIVE) $(MUSL_SDK_ARCHIVE)

# Every staged member participates in recovery, including non-primary headers,
# documentation and symlinks. Presence signatures are stable on intact trees.
define LEONOS_SDK_PRESENCE
$(O_META)/$(1)-present.sig: FORCE $(LEONOS_SRC)/tools/build/upstream-verify.sh
	$$(Q)sh $$(LEONOS_SRC)/tools/build/upstream-verify.sh $(2) $(2).files $$@
$(3): $(O_META)/$(1)-present.sig
endef
$(eval $(call LEONOS_SDK_PRESENCE,musl-sdk,$(MUSL_SDK),$(MUSL_SDK_REQUIRED)))
$(eval $(call LEONOS_SDK_PRESENCE,developer-sdk,$(DEVELOPER_SDK),$(DEVELOPER_SDK_REQUIRED) $(DEVELOPER_SDK_ARCHIVE)))
$(eval $(call LEONOS_SDK_PRESENCE,sdk-extra,$(SDK_EXTRA),$(SDK_EXTRA_STAMP)))
