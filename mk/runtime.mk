# LeonOS extensions; musl alone owns POSIX and executable startup.
RUNTIME_DIR := $(O)/system/lib
RUNTIME_SO := $(RUNTIME_DIR)/libleonos.so.2
RUNTIME_ARCHIVE := $(O)/musl/lib/libleonos.a
RUNTIME_INSTALLER_SO := $(O)/installer/lib/libleonos.so.2
RUNTIME_INSTALLER_ARCHIVE := $(O)/musl/lib/libleonos-installer.a
GBK_TABLE := $(O_INCLUDE)/generated/leonos_gbk_table.h
PNG_CONFIG := $(O_INCLUDE)/libpng/pnglibconf.h

RUNTIME_MBEDTLS_NAMES := aes asn1parse asn1write base64 bignum cipher cipher_wrap \
 constant_time ctr_drbg ecdh ecdsa ecp ecp_curves entropy gcm md oid pem pk \
 pkparse pk_wrap pkcs5 platform platform_util rsa rsa_internal sha1 sha256 sha512 \
 ssl_ciphersuites ssl_cli ssl_msg ssl_tls x509 x509_crt
RUNTIME_ZLIB_NAMES := adler32 compress crc32 deflate infback inffast inflate inftrees trees uncompr zutil
RUNTIME_PNG_NAMES := png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil \
 pngset pngtrans pngwio pngwrite pngwtran pngwutil
RUNTIME_SOURCES := $(sort $(patsubst $(LEONOS_SRC)/%,%,$(wildcard \
 $(LEONOS_SRC)/userland/libc/src/*.c $(LEONOS_SRC)/userland/libc/src/*.S \
 $(LEONOS_SRC)/userland/auth/*.c)) \
 $(addprefix third_party/mbedtls/library/,$(addsuffix .c,$(RUNTIME_MBEDTLS_NAMES))) \
 $(addprefix third_party/zlib/,$(addsuffix .c,$(RUNTIME_ZLIB_NAMES))) \
 $(addprefix third_party/libpng/,$(addsuffix .c,$(RUNTIME_PNG_NAMES))))
RUNTIME_OBJECTS := $(addprefix $(O_OBJ)/runtime/,$(addsuffix .o,$(RUNTIME_SOURCES)))
RUNTIME_INSTALLER_OBJECTS := $(addprefix $(O_OBJ)/installer-runtime/,$(addsuffix .o,$(RUNTIME_SOURCES)))
RUNTIME_FLAGS := --target=$(TRIPLE_USER) $(LEONOS_OPTIMIZATION_FLAGS) -std=c11 \
 -ffreestanding -fno-stack-protector -fPIC -ffunction-sections -fdata-sections \
 -Wall -Wextra -DLEONOS_USE_MUSL -D_GNU_SOURCE -mno-avx -mno-avx2 \
 -I$(LEONOS_SRC)/include/uapi -I$(PAM_ROOT)/usr/include -I$(AUTH_ROOT)/usr/include -I$(MUSL_SYSROOT)/include \
 -I$(LEONOS_SRC)/userland/libc/include \
 -I$(O_INCLUDE) -I$(LEONOS_SRC)/include -I$(LEONOS_SRC)/third_party/mbedtls/include \
 -I$(LEONOS_SRC)/third_party/zlib -I$(LEONOS_SRC)/third_party/libpng \
 -I$(O_INCLUDE)/libpng -DMBEDTLS_CONFIG_FILE='"leonos_mbedtls_config.h"' \
 -ffile-prefix-map=$(LEONOS_SRC)=. -ffile-prefix-map=$(O)=out
RUNTIME_CFLAGS ?=
RUNTIME_AUTH_LIBS := $(PAM_LIB) $(AUTH_ROOT)/lib/libcrypt.so.2
RUNTIME_HEADERS := $(PAM_HEADER) $(AUTH_ROOT)/usr/include/crypt.h
# Resolve the archive from the selected compiler, not from an unrelated LLVM install.
RUNTIME_BUILTINS := $(if $(LEONOS_PASSIVE),,$(shell $(TARGET_CC) --target=$(TRIPLE_USER) --rtlib=compiler-rt -print-libgcc-file-name 2>/dev/null))

LEONOS_SIG_runtime-cc := cc=$(TARGET_CC)|identity=$(shell $(TARGET_CC) --version 2>/dev/null | head -n1)|flags=$(RUNTIME_FLAGS) $(RUNTIME_CFLAGS)
LEONOS_SIG_runtime-link := ld=$(TARGET_LD)|identity=$(shell $(TARGET_LD) --version 2>/dev/null | head -n1)|ar=$(TARGET_AR)|builtins=$(RUNTIME_BUILTINS)|sources=$(RUNTIME_SOURCES)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,runtime-cc)))
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,runtime-link)))

$(GBK_TABLE): $(LEONOS_GBK_TOOL) $(LEONOS_SRC)/third_party/litehtml/src/encodings.cpp
	$(Q)mkdir -p $(dir $@)
	$(Q)$(LEONOS_GBK_TOOL) $(LEONOS_SRC)/third_party/litehtml/src/encodings.cpp $@

$(PNG_CONFIG): $(LEONOS_SRC)/third_party/libpng/scripts/pnglibconf.h.prebuilt $(LEONOS_SRC)/tools/build/png-config.sh $(LEONOS_EMIT)
	$(Q)mkdir -p $(dir $@)
	$(Q)sh $(LEONOS_SRC)/tools/build/png-config.sh $< $@ $(LEONOS_EMIT)

define LEONOS_RUNTIME_COMPILE
$(O_OBJ)/$(1)/%.c.o: $(LEONOS_SRC)/%.c $(2) $(GBK_TABLE) $(PNG_CONFIG) $(MUSL_STAMP) $(RUNTIME_HEADERS) $(O_META)/runtime-cc.sig
	$$(Q)mkdir -p $$(dir $$@)
	$$(Q)printf '  %-8s %s\n' CC $$<
	$$(Q)$$(TARGET_CC) $$(RUNTIME_FLAGS) $$(RUNTIME_CFLAGS) -include $(2) \
	 $$(if $$(findstring /zlib/,$$<),-DZ_SOLO -include stddef.h) \
	 $$(if $$(findstring /libpng/,$$<),-DLEONOS_LIBPNG_FIXED_POINT=3) \
	 -MMD -MP -MF $$@.d -MT $$@ -c $$< -o $$@.tmp
	$$(Q)mv $$@.tmp $$@
$(O_OBJ)/$(1)/%.S.o: $(LEONOS_SRC)/%.S $(2) $(O_META)/runtime-cc.sig
	$$(Q)mkdir -p $$(dir $$@)
	$$(Q)$$(TARGET_CC) --target=$$(TRIPLE_USER) -fPIC -I$$(LEONOS_SRC)/include/uapi -MMD -MP -MF $$@.d -MT $$@ -c $$< -o $$@.tmp
	$$(Q)mv $$@.tmp $$@
endef
$(eval $(call LEONOS_RUNTIME_COMPILE,runtime,$(AUTOCONF_H)))
$(eval $(call LEONOS_RUNTIME_COMPILE,installer-runtime,$(AUTOCONF_INSTALLER_H)))

$(RUNTIME_SO) $(RUNTIME_INSTALLER_SO): $(RUNTIME_BUILTINS)

$(RUNTIME_SO): $(RUNTIME_OBJECTS) $(RUNTIME_AUTH_LIBS) $(MUSL_SYSROOT)/lib/libc.so $(MUSL_SYSROOT)/lib/libmimalloc.so.3 $(O_META)/runtime-link.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)test -f '$(RUNTIME_BUILTINS)' || { echo 'missing compiler-rt builtins; select a complete Clang toolchain' >&2; exit 1; }
	$(Q)$(TARGET_LD) -shared --no-undefined --hash-style=both -soname libleonos.so.2 -o $@.tmp \
	 $(RUNTIME_OBJECTS) -L$(MUSL_SYSROOT)/lib -l:libmimalloc.so.3 $(RUNTIME_AUTH_LIBS) -lc $(RUNTIME_BUILTINS)
	$(Q)mv $@.tmp $@

$(RUNTIME_INSTALLER_SO): $(RUNTIME_INSTALLER_OBJECTS) $(RUNTIME_AUTH_LIBS) $(MUSL_SYSROOT)/lib/libc.so $(MUSL_SYSROOT)/lib/libmimalloc.so.3 $(O_META)/runtime-link.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)test -f '$(RUNTIME_BUILTINS)'
	$(Q)$(TARGET_LD) -shared --no-undefined --hash-style=both -z max-page-size=0x1000 -soname libleonos.so.2 -o $@.tmp \
	 $(RUNTIME_INSTALLER_OBJECTS) -L$(MUSL_SYSROOT)/lib -l:libmimalloc.so.3 $(RUNTIME_BUILTINS) $(RUNTIME_AUTH_LIBS) -lc
	$(Q)mv $@.tmp $@

$(RUNTIME_ARCHIVE): $(RUNTIME_OBJECTS) $(O_META)/runtime-link.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)rm -f $@.tmp
	$(Q)$(TARGET_AR) rcsD $@.tmp $(RUNTIME_OBJECTS)
	$(Q)mv $@.tmp $@

$(RUNTIME_INSTALLER_ARCHIVE): $(RUNTIME_INSTALLER_OBJECTS) $(O_META)/runtime-link.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)rm -f $@.tmp
	$(Q)$(TARGET_AR) rcsD $@.tmp $(RUNTIME_INSTALLER_OBJECTS)
	$(Q)mv $@.tmp $@

runtime: $(RUNTIME_SO) $(RUNTIME_ARCHIVE) $(RUNTIME_INSTALLER_SO) $(RUNTIME_INSTALLER_ARCHIVE)
-include $(addsuffix .d,$(RUNTIME_OBJECTS) $(RUNTIME_INSTALLER_OBJECTS))
