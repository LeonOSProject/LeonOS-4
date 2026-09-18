# Third-party components: the dependency lock file, the download cache, and the
# upstream sources the userland links against.
#
# Nothing here decides *how* an upstream project builds. Each component is
# configured and compiled by its own upstream build system, driven by a short
# script in tools/build; this fragment only says which pinned source is used,
# which products prove it worked, and when the work has to be redone.

LEONOS_LOCK := $(LEONOS_SRC)/configs/dependencies.lock.json
LEONOS_FETCH_SCRIPT := $(LEONOS_SRC)/tools/build/fetch.sh

# --- the lock file must describe what it claims -------------------------------
# Parse-time, but read-only: `make fetch` and every adapter consult the lock
# through leonos-deps, and a broken file has to stop the build before anything
# is downloaded.
$(LEONOS_CACHE):
	$(Q)mkdir -p $@

.PHONY: leonos-check-lock
leonos-check-lock: | $(LEONOS_O_MARKER) $(LEONOS_DEPS_TOOL)
	$(Q)printf '  %-8s %s\n' CHECK $(LEONOS_LOCK)
	$(Q)$(LEONOS_DEPS_TOOL) --lock $(LEONOS_LOCK) --check --root $(LEONOS_SRC)

fetch: leonos-check-lock | $(LEONOS_CACHE)
	$(Q)printf '  %-8s %s\n' FETCH $(LEONOS_CACHE)
	$(Q)sh $(LEONOS_FETCH_SCRIPT) --deps $(LEONOS_DEPS_TOOL) --lock $(LEONOS_LOCK) \
		--cache $(LEONOS_CACHE)

# Used by the build itself: a dependency that was never fetched stops the build
# with the id and the command that fixes it, instead of silently going online
# (plan section 9).
.PHONY: leonos-verify-cache
leonos-verify-cache: | $(LEONOS_DEPS_TOOL)
	$(Q)sh $(LEONOS_FETCH_SCRIPT) --deps $(LEONOS_DEPS_TOOL) --lock $(LEONOS_LOCK) \
		--cache $(LEONOS_CACHE) --verify-only

# --- musl and mimalloc --------------------------------------------------------
MUSL_SYSROOT := $(O_SYSROOT)/musl
MUSL_STAMP := $(MUSL_SYSROOT)/.leonos-musl.json
MUSL_WORK := $(O_THIRD_PARTY)/musl
MUSL_SCRIPT := $(LEONOS_SRC)/tools/build/musl-sysroot.sh

# musl's own configure probes with the compiler word, so the target triple has
# to stay inside CC; these are the flags the retired Python driver passed.
LEONOS_MUSL_CFLAGS := -O2 -fno-stack-protector -mno-avx

# The products the rest of the build links against. Declaring them makes an
# install that "succeeded" while leaving something out a Make error rather than
# a link failure three stages later.
LEONOS_MUSL_ARTIFACTS := \
	$(MUSL_SYSROOT)/lib/libc.so \
	$(MUSL_SYSROOT)/lib/libc.a \
	$(MUSL_SYSROOT)/lib/libmimalloc.so.3 \
	$(MUSL_SYSROOT)/lib/mimalloc.o \
	$(MUSL_SYSROOT)/lib/Scrt1.o \
	$(MUSL_SYSROOT)/lib/crt1.o \
	$(MUSL_SYSROOT)/lib/crti.o \
	$(MUSL_SYSROOT)/lib/crtn.o \
	$(MUSL_SYSROOT)/lib/rcrt1.o \
	$(MUSL_SYSROOT)/lib/libssp_nonshared.a \
	$(MUSL_SYSROOT)/include/stdio.h \
	$(MUSL_SYSROOT)/include/mimalloc.h \
	$(MUSL_SYSROOT)/share/licenses/musl/COPYRIGHT \
	$(MUSL_SYSROOT)/share/licenses/mimalloc/LICENSE

# The whole lock file digest is part of the signature rather than just the musl
# entries. That is deliberate: reading a subset here would need leonos-deps to
# exist before it has been built, and this project does not parse JSON with sed.
# The cost is one extra sysroot rebuild when an unrelated dependency is edited.
LEONOS_LOCK_DIGEST := $(if $(LEONOS_PASSIVE),unavailable,\
	$(shell sha256sum $(LEONOS_LOCK) 2>/dev/null | cut -d' ' -f1))
# git is part of the contract: the pinned trees are submodule checkouts and the
# adapter verifies their HEAD before unpacking.
leonos_git_path := $(if $(LEONOS_PASSIVE),deferred,$(shell command -v git 2>/dev/null || echo unavailable))

LEONOS_SIG_musl-sysroot := script=$(MUSL_SCRIPT)|lock=$(LEONOS_LOCK_DIGEST)|cc=$(TARGET_CC)|ar=$(TARGET_AR)|ranlib=$(TARGET_RANLIB)|ld=$(TARGET_LD)|triple=$(TRIPLE_USER)|cflags=$(LEONOS_MUSL_CFLAGS)|git=$(leonos_git_path)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,musl-sysroot)))

$(MUSL_STAMP): $(LEONOS_LOCK) $(MUSL_SCRIPT) $(LEONOS_DEPS_TOOL) $(O_META)/musl-sysroot.sig \
	| $(MUSL_SYSROOT) $(MUSL_WORK) $(O_LOGS)
	$(Q)printf '  %-8s %s\n' SYSROOT musl
	$(Q)sh $(MUSL_SCRIPT) --src '$(LEONOS_SRC)' --deps '$(LEONOS_DEPS_TOOL)' \
		--lock '$(LEONOS_LOCK)' --work '$(MUSL_WORK)' --sysroot '$(MUSL_SYSROOT)' \
		--cc '$(TARGET_CC)' --ar '$(TARGET_AR)' --ranlib '$(TARGET_RANLIB)' \
		--ld '$(TARGET_LD)' --target '$(TRIPLE_USER)' \
		--cflags '$(LEONOS_MUSL_CFLAGS)' --log $(O_LOGS)/musl.log

$(LEONOS_MUSL_ARTIFACTS): | $(MUSL_STAMP)

$(MUSL_SYSROOT) $(MUSL_WORK) $(O_THIRD_PARTY):
	$(Q)mkdir -p $@

.DELETE_ON_ERROR: $(MUSL_STAMP)
