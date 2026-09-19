# PAM adapter verification (2026-09-19)

This file records only the adapter, not whole-system/VM acceptance.

- TDD red: the original integration invocation failed with a missing Makefile.
- Regression red: requiring the `crypt_rn` dynamic import failed before adding
  the cross-link probe; the final config requires both crypt_r and crypt_rn.
- Regression red: editing the source pam_misc.h did not rebuild misc_conv.o
  while DEPS headers preceded source headers. Source headers now come first.
- Regression red: placing Linux UAPI and crypt.h in the same DEPS/usr/include
  caused HAVE_CRYPT_RN to fail because Clang demotes a path repeated in -I and
  -idirafter. The adapter now copies crypt.h to a private include directory, keeping UAPI
  after musl without duplicating the directory in -I and -idirafter. The integration
  test always assembles this merged layout, including for older split fixtures.
- Regression red: pam_conv1, bigcrypt and hmacfile did not exist before their
  Make rules were added. They are now built, but are not installed upstream.

Integration test command (the parent generates these independent new dependencies):

```sh
CC=/usr/bin/clang \
PAM_TEST_SOURCE="$PWD/out/takeover/third-party/pam/src/Linux-PAM-1.7.2" \
PAM_TEST_SYSROOT="$PWD/out/takeover/sysroot/musl" \
PAM_TEST_DEPS="$PWD/out/takeover/auth/root" \
sh tests/integration/pam.sh
```

Coverage: fresh parallel cross-build; 39 modules, all installed library/helper
paths; libpam SONAMEs and versioned pam_start; libcrypt.so.2 and crypt_rn imports;
no-op mtime stability; deleted DSO recovery; compiler flag invalidation and next
no-op; broken compiler failure preserving the previously published DSO; edited
public-header rebuild without recompiling an unrelated libpam object. No target
ELF is executed.

Retained logs:

- `/tmp/leonos-pam-takeover-test.log`: integration against the new Make dependencies.
- `/tmp/leonos-pam-final-build.log`: persistent build in `out/pam-port-validation`.
- `/tmp/leonos-pam-final-exec.trace`: execve trace of that adapter build/install.
- `/tmp/leonos-pam-header-layout-red.log`: merged-header-layout failure before fix.
- `/tmp/leonos-pam-crypt-rn-red.log`: regression assertion failure before fix.

The previous baseline for comparison is
`build/auth-upstream/linux-pam-404f040518b42d15/meson-logs/install-log.txt` and its
`libpam/include/config.h`. The installed path list is identical. All previous
config defines match; only an extra internal LEONOS_PAM_KEYINIT probe marker is
present. The port does not consume either baseline file during its build.

Not verified: authentication in a LeonOS VM, QEMU boot, full installer or SDK.
The adapter does not compile the upstream uninstalled example or regression test
suite. Cross capability checks establish libc/linker interfaces, not kernel
runtime support for each syscall. The caller owns locking, patches, cache
verification, clean staging publication, licenses and pam_leonos_password.
