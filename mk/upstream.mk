# Include after third-party.mk, pam.mk and runtime.mk. Each package owns an isolated root.
UPSTREAM_ROOT := $(O)/upstream
UPSTREAM_SCRIPT := $(LEONOS_SRC)/tools/build/upstream.sh
UPSTREAM_PACKAGES := libmd libbsd util-linux sudo shadow e2fsprogs dosfstools exfatprogs
upstream_libmd_outputs := lib/libmd.so.0 usr/include/md5.h
upstream_libbsd_outputs := lib/libbsd.so.0 usr/include/bsd/stdlib.h
upstream_util-linux_outputs := usr/lib/libuuid.a usr/lib/libblkid.a bin/su usr/sbin/fdisk bin/mount bin/lsblk
upstream_sudo_outputs := usr/bin/sudo usr/lib/sudo/sudoers.so
upstream_shadow_outputs := bin/login usr/sbin/useradd usr/sbin/usermod usr/sbin/userdel
upstream_e2fsprogs_outputs := usr/sbin/mkfs.ext2 usr/sbin/fsck.ext2
upstream_dosfstools_outputs := usr/sbin/mkfs.fat usr/sbin/fsck.fat
upstream_exfatprogs_outputs := usr/sbin/mkfs.exfat usr/sbin/fsck.exfat
upstream_ncurses_outputs := usr/lib/libncursesw.a usr/lib/libtinfow.a usr/include/curses.h
upstream_vim_outputs := usr/bin/vim
$(foreach package,$(UPSTREAM_PACKAGES) ncurses vim,$(eval upstream_$(package)_primary := $(upstream_$(package)_outputs)))
upstream_libbsd_deps := libmd
upstream_shadow_deps := libmd libbsd
upstream_e2fsprogs_deps := util-linux
upstream_exfatprogs_deps := util-linux
# Install manifests register every published file, so deleting a non-primary
# header, library link or manual page also invalidates the package group.

define LEONOS_UPSTREAM_RULE
LEONOS_SIG_upstream-$(1) := cc=$(TARGET_CC)|triple=$(TRIPLE_USER)|lock=$(LEONOS_LOCK_DIGEST)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)
$$(if $$(LEONOS_PASSIVE),,$$(eval $$(call LEONOS_SIGNATURE_RULE,upstream-$(1))))
upstream_$(1)_products := $$(addprefix $$(UPSTREAM_ROOT)/$(1)/root/,$$(sort $$(upstream_$(1)_outputs)))
$$(UPSTREAM_ROOT)/$(1)/root/.complete $$(upstream_$(1)_products) &: $$(UPSTREAM_SCRIPT) $$(LEONOS_LOCK) $$(LEONOS_DEPS_TOOL) $$(O_META)/upstream-$(1).sig $$(AUTH_STAMP) $$(LEONOS_AUTH_ARTIFACTS) $$(PAM_STAMP) $$(PAM_LIB) $$(PAM_HEADER) $$(foreach dep,$$(upstream_$(1)_deps),$$(UPSTREAM_ROOT)/$$(dep)/root/.complete $$(upstream_$$(dep)_products))
	$$(Q)rm -rf $$(UPSTREAM_ROOT)/$(1)/deps
	$$(Q)mkdir -p $$(UPSTREAM_ROOT)/$(1)/deps $$(O_LOGS)
	$$(Q)cp -a $$(AUTH_ROOT)/. $$(UPSTREAM_ROOT)/$(1)/deps/
	$$(Q)$$(foreach dep,$$(upstream_$(1)_deps),cp -a $$(UPSTREAM_ROOT)/$$(dep)/root/. $$(UPSTREAM_ROOT)/$(1)/deps/;)
	+$$(Q)case "$$$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; sh $$(UPSTREAM_SCRIPT) $(1) $$(LEONOS_SRC) $$(abspath $$(LEONOS_DEPS_TOOL)) $$(LEONOS_LOCK) $$(LEONOS_CACHE) $$(abspath $$(UPSTREAM_ROOT))/$(1)/work $$(abspath $$(UPSTREAM_ROOT))/$(1)/root $$(abspath $$(MUSL_SYSROOT)) $$(abspath $$(UPSTREAM_ROOT))/$(1)/deps $$(abspath $$(PAM_ROOT)) $$(TARGET_CC) $$(TRIPLE_USER) >$$(O_LOGS)/upstream-$(1).log 2>&1 || { tail -n 50 $$(O_LOGS)/upstream-$(1).log >&2; exit 1; }
	$$(Q)for product in $$(addprefix $$(UPSTREAM_ROOT)/$(1)/root/,$$(upstream_$(1)_primary)); do test -e "$$$$product" || { echo "missing upstream product: $$$$product" >&2; exit 1; }; touch "$$$$product"; done
	$$(Q)touch $$(UPSTREAM_ROOT)/$(1)/root/.complete
.PHONY: upstream-$(1)
upstream-$(1): $$(UPSTREAM_ROOT)/$(1)/root/.complete $$(upstream_$(1)_products)
endef
$(foreach package,$(UPSTREAM_PACKAGES),$(eval $(call LEONOS_UPSTREAM_RULE,$(package))))
.PHONY: leonos-upstream leonos-storage
leonos-upstream: $(addprefix upstream-,$(UPSTREAM_PACKAGES))
leonos-storage: upstream-e2fsprogs upstream-dosfstools upstream-exfatprogs

# Terminal packages use the pinned submodules and upstream Makefiles.
UPSTREAM_TERMINAL_SCRIPT := $(LEONOS_SRC)/tools/build/upstream-terminal.sh
define LEONOS_TERMINAL_RULE
LEONOS_SIG_upstream-$(1) := cc=$(TARGET_CC)|triple=$(TRIPLE_USER)|lock=$(LEONOS_LOCK_DIGEST)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)
$$(if $$(LEONOS_PASSIVE),,$$(eval $$(call LEONOS_SIGNATURE_RULE,upstream-$(1))))
upstream_$(1)_products := $$(addprefix $$(UPSTREAM_ROOT)/$(1)/root/,$$(sort $$(upstream_$(1)_outputs)))
$$(UPSTREAM_ROOT)/$(1)/root/.complete $$(upstream_$(1)_products) &: $$(UPSTREAM_TERMINAL_SCRIPT) $$(LEONOS_LOCK) $$(LEONOS_DEPS_TOOL) $$(O_META)/upstream-$(1).sig $$(MUSL_STAMP) $$(LEONOS_MUSL_ARTIFACTS) $(if $(filter vim,$(1)),$$(UPSTREAM_ROOT)/ncurses/root/.complete $$(upstream_ncurses_products))
	$$(Q)mkdir -p $$(O_LOGS)
	+$$(Q)case "$$$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; sh $$(UPSTREAM_TERMINAL_SCRIPT) $(1) $$(LEONOS_SRC) $$(abspath $$(LEONOS_DEPS_TOOL)) $$(LEONOS_LOCK) $$(abspath $$(UPSTREAM_ROOT))/$(1)/work $$(abspath $$(UPSTREAM_ROOT))/$(1)/root $$(abspath $$(MUSL_SYSROOT)) $$(abspath $$(UPSTREAM_ROOT))/ncurses/root $$(TARGET_CC) $$(TRIPLE_USER) >$$(O_LOGS)/upstream-$(1).log 2>&1 || { tail -n 50 $$(O_LOGS)/upstream-$(1).log >&2; exit 1; }
	$$(Q)for product in $$(addprefix $$(UPSTREAM_ROOT)/$(1)/root/,$$(upstream_$(1)_primary)); do test -e "$$$$product" || exit 1; touch "$$$$product"; done
	$$(Q)touch $$(UPSTREAM_ROOT)/$(1)/root/.complete
.PHONY: upstream-$(1)
upstream-$(1): $$(UPSTREAM_ROOT)/$(1)/root/.complete $$(upstream_$(1)_products)
endef
$(foreach package,ncurses vim,$(eval $(call LEONOS_TERMINAL_RULE,$(package))))
leonos-upstream: upstream-ncurses upstream-vim

UPSTREAM_APK_ROOT := $(UPSTREAM_ROOT)/apk
UPSTREAM_APK := $(UPSTREAM_APK_ROOT)/apk.static
$(UPSTREAM_APK) $(UPSTREAM_APK_ROOT)/.complete &: $(LEONOS_SRC)/tools/build/upstream-apk.sh $(LEONOS_LOCK) $(LEONOS_DEPS_TOOL) $(wildcard $(LEONOS_SRC)/system/rootfs/etc/apk/keys/*)
	$(Q)sh $(LEONOS_SRC)/tools/build/upstream-apk.sh $(LEONOS_SRC) $(abspath $(LEONOS_DEPS_TOOL)) $(LEONOS_LOCK) $(LEONOS_CACHE) $(abspath $(UPSTREAM_APK_ROOT))
.PHONY: upstream-apk
upstream-apk: $(UPSTREAM_APK) $(UPSTREAM_APK_ROOT)/.complete
leonos-upstream: upstream-apk

UPSTREAM_APP_DIR := $(O)/userland
LUA_SO := $(UPSTREAM_APP_DIR)/liblua.so.5
MAGIC_SO := $(UPSTREAM_APP_DIR)/libmagic.so.1
SQLITE_SO := $(UPSTREAM_APP_DIR)/sqlite.so.3
SQLITE_HEADER := $(UPSTREAM_APP_DIR)/sqlite3.h
upstream_app_lua_outputs := lua.elf liblua.so.5 liblua.a
upstream_app_sl_outputs := sl.elf
upstream_app_less_outputs := less.elf
upstream_app_file_outputs := file.elf libmagic.so.1 libmagic.a magic.h
upstream_app_sqlite_outputs := sqlite.so.3 libsqlite3.a sqlite3.h

define LEONOS_UPSTREAM_APP
upstream_app_$(1)_inputs := $$(shell find $$(LEONOS_SRC)/third_party/$(if $(filter tcc,$(1)),tinycc,$(if $(filter pleditor,$(1)),pl_editor,$(1))) $$(LEONOS_SRC)/userland/$(if $(filter pleditor,$(1)),apps/pleditor,$(1)) -type f ! -name .git 2>/dev/null | LC_ALL=C sort)
LEONOS_SIG_upstream-app-$(1) := cc=$(TARGET_CC)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)|ld=$(TARGET_LD)|ar=$(TARGET_AR)|cflags=$(LEONOS_OPTIMIZATION_FLAGS)|ldflags=$(LEONOS_LINK_POLICY_FLAGS)|inputs=$$(upstream_app_$(1)_inputs)
$$(if $$(LEONOS_PASSIVE),,$$(eval $$(call LEONOS_SIGNATURE_RULE,upstream-app-$(1))))
$$(addprefix $$(UPSTREAM_APP_DIR)/,$$(sort $$(upstream_app_$(1)_outputs))) &: $$(upstream_app_$(1)_inputs) $$(LEONOS_SRC)/tools/build/upstream-app.sh $$(LEONOS_LOCK) $$(AUTOCONF_H) $$(RUNTIME_SO) $$(RUNTIME_ARCHIVE) $$(MUSL_STAMP) $$(O_META)/upstream-app-$(1).sig
	$$(Q)mkdir -p $$(O_LOGS)
	+$$(Q)case "$$$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; UPSTREAM_DEPS='$$(abspath $$(LEONOS_DEPS_TOOL))' UPSTREAM_CFLAGS='$$(LEONOS_OPTIMIZATION_FLAGS)' UPSTREAM_LDFLAGS='$$(LEONOS_LINK_POLICY_FLAGS)' sh $$(LEONOS_SRC)/tools/build/upstream-app.sh $(1) $$(LEONOS_SRC) $$(abspath $$(UPSTREAM_ROOT))/app-$(1) $$(abspath $$(UPSTREAM_APP_DIR)) $$(abspath $$(MUSL_SYSROOT)) $$(abspath $$(RUNTIME_SO)) $$(abspath $$(RUNTIME_ARCHIVE)) $$(abspath $$(O_INCLUDE)) $$(abspath $$(AUTOCONF_H)) $$(TARGET_CC) $$(TARGET_LD) $$(TARGET_AR) >$$(O_LOGS)/upstream-app-$(1).log 2>&1 || { tail -n 40 $$(O_LOGS)/upstream-app-$(1).log >&2; exit 1; }
	$$(Q)for product in $$(addprefix $$(UPSTREAM_APP_DIR)/,$$(upstream_app_$(1)_outputs)); do test -e "$$$$product" || exit 1; touch "$$$$product"; done
.PHONY: upstream-app-$(1)
upstream-app-$(1): $$(addprefix $$(UPSTREAM_APP_DIR)/,$$(sort $$(upstream_app_$(1)_outputs)))
endef
$(foreach package,lua sl less file sqlite,$(eval $(call LEONOS_UPSTREAM_APP,$(package))))
leonos-upstream: $(addprefix upstream-app-,lua sl less file sqlite)

BUSYBOX_ELF := $(UPSTREAM_APP_DIR)/busybox.elf
BUSYBOX_LINKS := $(UPSTREAM_APP_DIR)/busybox.links
UPSTREAM_EPOCH := $(if $(SOURCE_DATE_EPOCH),$(SOURCE_DATE_EPOCH),$(shell git -C $(LEONOS_SRC) log -1 --format=%ct))
LEONOS_SIG_upstream-busybox := cc=$(TARGET_CC)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)|flags=$(LEONOS_OPTIMIZATION_FLAGS)|ldflags=$(LEONOS_LINK_POLICY_FLAGS)|epoch=$(UPSTREAM_EPOCH)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,upstream-busybox)))
$(BUSYBOX_ELF) $(BUSYBOX_LINKS) &: $(LEONOS_SRC)/tools/build/upstream-busybox.sh $(LEONOS_SRC)/userland/busybox/leonos.config $(O_META)/upstream-busybox.sig $(LEONOS_LOCK) $(LEONOS_DEPS_TOOL) $(MUSL_STAMP) $(LEONOS_MUSL_ARTIFACTS) $(AUTH_STAMP) $(LEONOS_AUTH_ARTIFACTS)
	$(Q)mkdir -p $(O_LOGS)
	+$(Q)case "$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; UPSTREAM_CFLAGS='$(LEONOS_OPTIMIZATION_FLAGS)' UPSTREAM_LDFLAGS='$(LEONOS_LINK_POLICY_FLAGS)' sh $(LEONOS_SRC)/tools/build/upstream-busybox.sh $(LEONOS_SRC) $(abspath $(UPSTREAM_ROOT))/busybox $(abspath $(UPSTREAM_APP_DIR)) $(abspath $(MUSL_SYSROOT)) $(abspath $(AUTH_ROOT))/usr/include $(TARGET_CC) $(abspath $(LEONOS_DEPS_TOOL)) $(LEONOS_LOCK) $(UPSTREAM_EPOCH) >$(O_LOGS)/upstream-busybox.log 2>&1 || { tail -n 50 $(O_LOGS)/upstream-busybox.log >&2; exit 1; }
.PHONY: upstream-busybox
upstream-busybox: $(BUSYBOX_ELF) $(BUSYBOX_LINKS)
leonos-upstream: upstream-busybox
upstream_app_pleditor_outputs := pleditor.elf
$(eval $(call LEONOS_UPSTREAM_APP,pleditor))
$(UPSTREAM_APP_DIR)/pleditor.elf: $(LEONOS_SRC)/userland/apps/pleditor/platform_leonos.c $(LEONOS_SRC)/mk/upstream.mk
leonos-upstream: upstream-app-pleditor

FASTFETCH_ELF := $(UPSTREAM_APP_DIR)/fastfetch.elf
$(FASTFETCH_ELF): $(LEONOS_LOCK) $(LEONOS_DEPS_TOOL) $(LEONOS_SRC)/mk/upstream.mk
	$(Q)mkdir -p $(dir $@)
	$(Q)set -eu; url=$$($(LEONOS_DEPS_TOOL) --lock $(LEONOS_LOCK) --id fastfetch --print url); digest=$$($(LEONOS_DEPS_TOOL) --lock $(LEONOS_LOCK) --id fastfetch --print sha256); source=$(LEONOS_CACHE)/$${url##*/}; test -f "$$source" || { echo 'missing fastfetch: run make fetch' >&2; exit 1; }; test "$$(sha256sum "$$source" | cut -d' ' -f1)" = "$$digest"; readelf -h "$$source" | grep -q 'Advanced Micro Devices X86-64'; if readelf -l -d "$$source" | grep -E 'INTERP|\(NEEDED\)'; then exit 1; fi; cp "$$source" $@.tmp; chmod 755 $@.tmp; mv $@.tmp $@
leonos-upstream: $(FASTFETCH_ELF)
upstream_app_cmd_outputs := cmd.elf
upstream_app_tcc_outputs := tcc.elf tcc-runtime/lib/libtcc1.a tcc-runtime/lib/libc.a tcc-runtime/include/stdio.h
$(eval $(call LEONOS_UPSTREAM_APP,cmd))
$(eval $(call LEONOS_UPSTREAM_APP,tcc))
$(UPSTREAM_APP_DIR)/cmd.elf: $(LEONOS_SRC)/tools/build/upstream-cmd-patch.sh
$(addprefix $(UPSTREAM_APP_DIR)/,$(upstream_app_tcc_outputs)): $(LEONOS_SRC)/tools/build/upstream-tcc-patch.sh $(PNG_CONFIG)
leonos-upstream: upstream-app-cmd upstream-app-tcc
FILE_MAGIC := $(O)/resources/magic.mgc
$(FILE_MAGIC): $(LEONOS_SRC)/tools/build/upstream-magic.sh $(upstream_app_file_inputs)
	$(Q)mkdir -p $(O_LOGS)
	+$(Q)case "$${MAKEFLAGS%% *}" in *n*) exit 0;; esac; sh $(LEONOS_SRC)/tools/build/upstream-magic.sh $(LEONOS_SRC) $(abspath $(UPSTREAM_ROOT))/file-magic $(abspath $@) $(HOSTCC) >$(O_LOGS)/upstream-magic.log 2>&1 || { tail -n 40 $(O_LOGS)/upstream-magic.log >&2; exit 1; }
leonos-upstream: $(FILE_MAGIC)

# A content/presence signature avoids creating thousands of grouped peer nodes
# for terminfo/manpages, while detecting deletion of every installed product.
define LEONOS_UPSTREAM_PRESENCE
$(O_META)/upstream-$(1)-present.sig: FORCE $(LEONOS_SRC)/tools/build/upstream-verify.sh
	$(Q)sh $(LEONOS_SRC)/tools/build/upstream-verify.sh $(UPSTREAM_ROOT)/$(1)/root $(UPSTREAM_ROOT)/$(1)/work/installed-files $$@
$(UPSTREAM_ROOT)/$(1)/root/.complete $$(upstream_$(1)_products): $(O_META)/upstream-$(1)-present.sig
endef
$(foreach package,$(UPSTREAM_PACKAGES) ncurses vim,$(eval $(call LEONOS_UPSTREAM_PRESENCE,$(package))))
$(O_META)/upstream-tcc-present.sig: FORCE $(LEONOS_SRC)/tools/build/upstream-verify.sh
	$(Q)sh $(LEONOS_SRC)/tools/build/upstream-verify.sh $(UPSTREAM_APP_DIR) $(UPSTREAM_ROOT)/app-tcc/installed-files $@
$(addprefix $(UPSTREAM_APP_DIR)/,$(upstream_app_tcc_outputs)): $(O_META)/upstream-tcc-present.sig
$(O_META)/upstream-apk-present.sig: FORCE $(LEONOS_SRC)/tools/build/upstream-verify.sh
	$(Q)sh $(LEONOS_SRC)/tools/build/upstream-verify.sh $(UPSTREAM_APK_ROOT) $(UPSTREAM_APK_ROOT)/installed-files $@
$(UPSTREAM_APK) $(UPSTREAM_APK_ROOT)/.complete: $(O_META)/upstream-apk-present.sig
UPSTREAM_TCC_SDK_HEADERS := $(shell find $(LEONOS_SRC)/devtools/include $(LEONOS_SRC)/include/uapi -type f | LC_ALL=C sort)
$(addprefix $(UPSTREAM_APP_DIR)/,$(upstream_app_tcc_outputs)): $(UPSTREAM_TCC_SDK_HEADERS)
