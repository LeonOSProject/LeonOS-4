# Linux-PAM 1.7.2 Make adapter

This is the LeonOS musl build profile, not a general replacement for upstream
Meson. Its source lists and install policy follow Linux-PAM 1.7.2's checked-in
Meson files. It builds all 39 modules enabled in the former musl configuration,
libpam 0.85.1, libpam_misc/libpamc 0.82.1, five installed ELF sbin helpers,
upperLOWER, the namespace shell helper, headers, configuration, pkg-config files
and all translation catalogs. Documentation remains disabled as before.

The caller supplies a read-only, verified, patched source tree (including all
`patches/linux-pam/*.patch`), musl sysroot and a dependency stage containing
libxcrypt and Linux UAPI headers. No old generated config or build output is read
by this adapter. `configure.sh` cross-compiles and links the capability checks;
it never executes the resulting ELF. `crypt_rn` is mandatory, preserving the
sized-libcrypt security patch. Optional external libraries absent in the old
profile (SELinux, audit, econf, systemd/elogind, NIS, db and OpenSSL) stay disabled.
The old musl profile also did not enable pam_lastlog or pam_rhosts.

```sh
make -f tools/build/pam/Makefile \
  SRC=/absolute/patched/Linux-PAM-1.7.2 \
  BUILD=/absolute/output/pam \
  DESTDIR=/absolute/unpublished/stage \
  SYSROOT=/absolute/musl/sysroot DEPS=/absolute/dependency/stage \
  CC=/usr/bin/clang TARGET=x86_64-linux-musl \
  CFLAGS='-O2 -mno-avx -mno-avx2' install
```

`KERNEL_HEADERS` defaults to `DEPS/usr/include`. Override only when UAPI headers
are staged elsewhere. `CPPFLAGS` and `LDFLAGS` are accepted. The parent must
validate supported paths, hold its output-directory lock, and publish a fresh
staging tree atomically; `install` writes PAM-owned paths into that unpublished
tree. It intentionally does not delete arbitrary files from a shared stage.
Do not use spaces or Make metacharacters in path arguments. Recursive callers
should use `$(MAKE)` so jobserver settings are inherited. No nested jobs are
started by this adapter. `msgfmt` is required to install the upstream catalogs; `bison` and `flex` build
the upstream pam_conv1 conversion tool.

The `all` target is incremental: each source has a separate object and depfile,
including system-header dependencies; linker scripts, compiler identity, flags,
and port scripts invalidate relevant outputs. Objects and ELF files are renamed
only after successful compilation/linking. Header search order prioritizes the
provided source tree over any existing staged PAM headers. The internal library
is linked as explicit objects, so it cannot retain removed archive members.
`install` is intended for a fresh stage and always runs when explicitly requested.

Run the cross-build integration contract with:

```sh
CC=/usr/bin/clang \
PAM_TEST_SOURCE=/absolute/patched/Linux-PAM-1.7.2 \
PAM_TEST_SYSROOT=/absolute/musl/sysroot \
PAM_TEST_DEPS=/absolute/dependency/stage \
sh tests/integration/pam.sh
```

The test builds a private copy of the source and checks SONAMEs, symbol versions,
module/helper installation, dynamic libcrypt and crypt_rn imports, no-op mtimes,
missing-output recovery, flag invalidation, failed-compiler preservation, and
public-header rebuild isolation. It does not execute target ELF files. Upstream
uninstalled examples/test binaries are not production outputs of
this adapter; target VM authentication tests remain a separate required check.

The old non-installed `pam_conv1`, `bigcrypt` and `hmacfile` utilities are also
built under `BUILD/bin`; they are deliberately not added to the installed tree.
