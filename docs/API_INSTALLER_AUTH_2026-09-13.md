# API installer authorization and duplicate windows

## Scope and evidence

This repair addresses repeated API installation windows and Chinese askpass
text. It does not certify all API archives or redesign extraction, rollback,
or the package format.

The supplied `/home/xiaobai/installer-serial.log` records the old sequence:
GUI pid 18 immediately reports `parent failed to elevate for installation`,
sudo authenticates successfully, and pid 23 opens another full GUI wizard.
After a second Install click, worker pid 24 reports success and exits 0, but
its parent reports failure. The parent misinterpreted asynchronous elevation
as failure, and checked a shared status file before checking child completion.

## Implementation

- The original GUI remains alive during sudo/PAM authorization. Only
  `apiapp.elf --install-worker <archive> <destination> <shortcut>` is launched
  with privilege. That entry point never creates a GUI window.
- Progress uses an inherited private pipe instead of a predictable status
  pathname. The parent handles partial records and services GUI events while
  waiting. The real child exit status determines success, including failure
  after reporting 100% progress.
- The worker requires root and a pipe on stdout. It saves that descriptor and
  redirects ordinary library diagnostics to stderr before extraction, so log
  text cannot corrupt progress records. Both ends are closed on completion.
- The SDK stdout adapter forces GUI askpass even when launched from Terminal.
  A command-specific sudoers `!use_pty` entry preserves the worker's progress
  pipe, matching the existing file-operation helper policy. The default
  `use_pty`, wheel membership, authentication and command authorization rules
  still apply to other commands. No NOPASSWD rule is added.
- Askpass uses the configured UI language for its title, generic password
  prompt and confirmation/cancellation buttons. Custom PAM challenge strings
  are preserved. Password bytes are never written to diagnostics.
- The native `fstat` and `statx(AT_EMPTY_PATH)` paths now report anonymous
  pipes as `S_IFIFO | 0600`, with `st_rdev=0`, including musl's
  `newfstatat(AT_EMPTY_PATH)` route. Previously the legacy device classification
  became `S_IFCHR`, so the worker correctly rejected the misidentified output.
  The reference is Linux v6.12 `fs/pipe.c:get_pipe_inode()`. This is a type
  correction, not certification of all pipe stat metadata or chmod semantics.
  Empty-path resolution also accepts an open anonymous descriptor without
  demanding a backing pathname. Invalid descriptors remain EBADF; a nonempty
  path relative to a pipe is ENOTDIR, and empty paths without the flag are
  ENOENT.

## Verification

```sh
python3 tools/test_apiapp_authorization.py
python3 tools/test_linux_descriptors.py
visudo -c -I -f system/rootfs/etc/sudoers
python3 build.py run images-iso
python3 tools/test_apiapp_qemu.py
```

The host regression compiles the actual installation-controller source and
uses real child processes, pipes and wait statuses, with GUI and privilege
launch boundaries stubbed. Success, failure after complete progress, cancelled
authorization and failed launch pass; the old controller fails the assertion
against relaunching the full application.

The QEMU test uses a disposable copy of the production live root, Chinese UI,
the normal test account and the real sudo/PAM configuration. A valid archive
with a file blocking its destination directory produces a real installation
failure. A separate valid archive has an available destination. Each result
must remain unchanged for six seconds without creating another API process,
and the Close button must exit the original wizard. Only this disposable
image receives the fixtures and Chinese language override.

Intermediate testing exercised sudo's PTY execution path when launched from
Terminal and detected the kernel's incorrect pipe file type. Exit 126 is a
failed helper, not an extraction failure. The test checks complete numeric
exit codes and waits only for authorization started after the Install click,
excluding the earlier `sudo -k` command's exit status. The same static musl
pipe-type probe runs on host Linux and in the guest, checking raw fstat,
newfstatat, statx, musl fstat, dup, and an unaffected character device.

The implementation passes all three QEMU GUI cases and the raw pipe probe.
Logs are `build/apiapp-host.log`, `build/apiapp-descriptors.log`, and
`build/apiapp-gui.log`; guest serial output and screenshots are under
`build/apiapp-gui-test/`. Chinese text was visually checked in the authorization
screenshots, and both failure and success result screens were inspected.

## Production artifacts

Both builds completed with zero errors (`build/apiapp-images.log`). The
embedded ISO root files match the generated ext2 images byte for byte.
The live root, installer runtime root and installation payload all contain
the built apiapp, sudod, libleonos and sudoers files, verified by SHA256.

- Build: `4.6.2-3711`.
- `build/images/leonos4.iso`:
  `ef3d4ed11baecdc61df195f884f8b59414ab8046f41796c77c9ea2836413793d`.
- `build/images/leonos4-installer.iso`:
  `4ee43f2765f32a48edc78d477c511cdaa47cba31ddf737a86f99895c06270c7f`.
- Kernel:
  `8a9d7b440a2efd0172bac4dbd53f1077774752d5125e339f23ee0996442e249a`.

The final QEMU run exits 0 with all four PASS markers (raw pipe probe,
cancel, failure, success) and uses this same kernel hash and the final
production live root as its fixture base.

VMware validation of this change remains pending. Existing installations need
the updated kernel, userland and installer command's sudoers configuration;
replacing apiapp alone does not repair the kernel pipe-type defect.
