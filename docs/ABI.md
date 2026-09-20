# LeonOS 4 ABI

## Linux x86_64 syscall convention

- `rax`: syscall number.
- `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`: arguments 0 through 5.
- `rax`: return value.
- Negative return values are `-errno`.

Unimplemented syscalls return `-ENOSYS`.

## GPU Rendering ABI

`include/leonos/gpu.h` defines the versioned, process-owned offscreen rendering
API used by glxgears. `leonos_gpu_create`, `leonos_gpu_render` and
`leonos_gpu_destroy` manage bounded SVGA3D triangle batches; `leonos_gpu_info`
reports capabilities, resource counters and estimated busy time.
`leonos_gpu_diagnostics` retrieves the latest device render-failure
snapshot only for its owning process, without advancing the hardware FIFO.
Existing rendering request layouts and the CPU/memory performance structure
are unchanged. See [SVGA3D.md](SVGA3D.md)
for layout semantics, limits, synchronization, software fallback and validation.

## Dynamic Library ABI

Dynamic PIE executables use `/lib/ld-musl-x86_64.so.1`. musl supplies the
loader and standard C/POSIX ABI; mimalloc supplies public allocation functions.
LeonOS extensions live in `/usr/lib/leonos/libleonos.so.2`. The build driver writes
`/usr/lib/leonos:/lib:/usr/lib` as the application search path. A private LeonOS ABI note
is not required for Linux ELF programs. Static binaries use musl CRT and the
same allocator policy.

The old `libleonos.so.1` alias and native loader are not distributed. Native
binaries and API packages using the retired root layout must be rebuilt for
the current paths; there is no automatic migration of stored paths.

## Syscall subset

LeonOS keeps Linux-compatible syscall numbers for the current user ABI:

- File I/O: `read`, `write`, `open`, `close`, `stat`, `fstat`, `lseek`
- Filesystem mutation: `mkdir`, `unlink`, `rmdir`, `rename`
- Process and scheduler: `execve`, `wait4`, `exit`, `getpid`, `sched_yield`,
  `nanosleep`
- Working directory: `getcwd`, `chdir`
- Memory: `mmap`, `munmap`
- Device and system extensions: `ioctl`

Standard libc wrappers come from `third_party/musl`. LeonOS extensions live in
`userland/libc`. Canonical kernel wire definitions are in `include/uapi`; the
complete per-call status is in `LINUX_ABI_SYSCALLS_2026-09-07.csv`. `mmap`
supports anonymous private mappings and private file mappings; `munmap`
supports whole or partial unmapping.

## Shared POSIX porting surface

`libleonos.so.2` provides the ANSI curses subset used by nano and `sl`.
Applications include `<curses.h>` or `<ncurses.h>` from the SDK. This is a
LeonOS terminal API, not binary compatibility with host ncurses.

Standard file, process, pthread, signal and socket APIs come from musl without
private POSIX adapters. Raw syscalls return negative errno; musl converts
errors according to each libc function's contract. For example raw getcwd
returns a byte count, while libc getcwd returns a pointer.

The kernel supports native signal frames, red-zone preservation, alternate
stacks and FP/SIMD restoration for the tested configurations. `forkpty` uses
Linux ioctl encodings, setsid and TIOCSCTTY. The native regression suite covers
pthread contention/cancellation and Unix descriptor passing. This does not
certify complete signal, PTY, wait, clone, socket or SMP behavior; consult
[the implementation ledger](LINUX_ABI_PROGRESS_2026-09-08.md).

`stat`, `fstat` and `lstat` use Linux x86-64 layouts, including the 144-byte raw
stat record. Compact LeonOS metadata is available through explicitly named
`leonos_stat_legacy` and `leonos_fstat_legacy` extension functions only.

See [Syscalls](SYSCALLS.md) for extension interfaces and the audit CSV for the
full native Linux v6.12 syscall scope.

## Time Synchronization ABI

`LEONOS_IOCTL_TIME_NTP_SYNC` accepts a `struct leonos_time_sync` from
`include/leonos/system.h`. The request may leave `server` empty to use
`pool.ntp.org`; the result returns the selected server, resolved IPv4 address,
network status, validated Unix seconds, and `valid=1` only after the kernel
updates its software wall clock. The ioctl is limited to trusted background
service tasks, so ordinary applications cannot change system time.

## Device model

The runtime exposes a synthetic devfs namespace. Common nodes are
`/dev/null`, `/dev/zero`, `/dev/full`, `/dev/random`, `/dev/urandom`,
`/dev/tty`, `/dev/console`, `/dev/ptmx` and `/dev/pts/<id>`, `/dev/fb0`, `/dev/dsp`, `/dev/serial0`,
`/dev/ttyS0`, `/dev/net0`, `/dev/ethernet0`, `/dev/disk0`, `/dev/sda`,
`/dev/vda`, `/dev/nvme0n1`, `/dev/rtc`, `/dev/kmsg`, and `/dev/input/event0`.
`/dev/stdin`, `/dev/stdout`, and `/dev/stderr` alias the current process
streams. `/dev/input` and `/dev/pts` are directories and are enumerated
through normal directory syscalls. Device nodes are synthetic and are not
stored in the filesystem image.

Framebuffer, audio, input, network, disk, PTY, and GUI libc helpers open their
corresponding `/dev` node and issue the device ABI. PTY users can use the
standard `posix_openpt/openpty/forkpty` functions; master/slave data flows
through normal `read/write/poll` descriptors. Calls made by old
applications with the historical descriptor 3 are translated to the matching
node by libc, so existing binaries continue to work while using the devfs
namespace. System applications query the complete hardware inventory through
`LEONOS_IOCTL_DEVICE_LIST`.

PCM applications use OSS `/dev/dsp` with `<linux/soundcard.h>`. The current
device accepts 16-bit little-endian stereo output and provides normal
`write`, `O_NONBLOCK`, and `poll(POLLOUT)` behavior plus the basic
`SNDCTL_DSP_*` format and queue ioctls. `/dev/audio` and `/dev/audio0` are
temporary aliases retained only for binaries that still use the private audio
request family.

## Driver Module ABI

Loadable Ring 0 driver modules use the public definitions in
`include/leonos/driver.h`. `LEONOS_IOCTL_DRIVER_LIST` exposes the discovered
module filename, ABI version, state, and diagnostic text to all user sessions.
`LEONOS_IOCTL_DRIVER_CONTROL` accepts load, unload, forced-unload, rescan, and
boot-enable actions, but the kernel permits it only for administrator tasks.

The kernel loads unsigned ELF64 `ET_REL` files from `/drivers` after the
root filesystem is mounted. The complete binary format, restricted kernel API,
and persistent `/etc/leonos/drivers.conf` policy are documented in
[Drivers](DRIVERS.md).

## Kernel Debug Module ABI

`/usr/lib/leonos/kerneldebug.sys` is a built-in-only x86_64 little-endian `ET_REL`
module. It must contain a `.note.leonos.kerneldebug` ELF note owned by
`LEONKDBG`, type `0x4c4b4447`, ABI `1`, and the fixed entry-name hash. The
loader accepts only PIC-free kernel sections and the `NONE`, `64`, `32`,
`32S`, `PC32`, and `PLT32` relocations; dynamic segments, TLS, IFUNC,
undefined symbols, W+X sections, and unknown sections are rejected.

The module receives the fixed `leonos_kernel_debug_api` table declared in
`kernel/ntclks/include/ntclks/kernel_debug.h`. It provides ostui output/input,
TSC timing, controlled syscall/ioctl benchmark callbacks, and explicit
continue, reboot, and shutdown operations. A valid module owns the diagnostic
session; the kernel's minimal menu is only a recovery path for a missing or
rejected module.

The one-shot marker is `/boot/leonos/state/kerneldebug.next`. The loader consumes
and deletes it before validating its contents, preventing repeated entry after
an interrupted or malformed debug boot. The persistent activation flag is
`/var/lib/leonos/kerneldebug.enabled`.

## Appearance ABI

The runtime UI appearance is a Desktop-owned state. `Metro` is the default
(`LEONOS_UI_THEME_METRO`); `LEONOS_UI_THEME_WIN95` restores the legacy Win95
palette and bevelled controls. The global `/etc/leonos/display.conf`
`theme=` key remains the boot/default style used before a user session is
available, including early framebuffer output and bugcheck rendering.

Per-user personalization is saved separately in
`/home/<name>/appearance.conf`. `struct leonos_appearance_state` and
`struct leonos_appearance_request` carry the active theme, independent Metro
and Win95 basic color scheme IDs, a wallpaper display mode, and a wallpaper BMP
path. Wallpaper BMP decoding is bounded to 1280 x 720 and accepts
uncompressed 24-bit or 32-bit BMP files.

The independent color scheme IDs are `Blue`, `Teal`, `Green`, `Purple`,
`Red`, `Graphite`, and `Kawaii Pink`. The `Kawaii Pink` scheme is available
for both Metro and Win95; each theme keeps its own palette and persisted
`metro.color` / `win95.color` value. Its configuration value remains `pink`
for compatibility.

`LEONOS_GUI_IOCTL_APPEARANCE_STATE` reads the current Desktop-published state.
Logged-in user tasks may submit `LEONOS_GUI_IOCTL_APPEARANCE_REQUEST`; the
window server polls and publishes the updated state through the paired
appearance ioctls, writes the current user's `appearance.conf`, reloads the
wallpaper, then sends `LEONOS_GUI_APP_EVENT_THEME_CHANGED` to active
application windows. The event carries the theme in `x`, the Metro color
scheme in `y`, and the Win95 color scheme in `dx`.

## Authentication ABI

Multi-user state is exposed through `include/leonos/auth.h` and libc wrappers
in `userland/libc/src/libc.c`.

User-facing roles are:

- `LEONOS_AUTH_ROLE_ADMIN`
- `LEONOS_AUTH_ROLE_USER`

The account database supports up to `LEONOS_AUTH_MAX_USERS` accounts. Usernames
are lowercase letters, digits, and `_`; passwords must be non-empty. The current
v1 password verifier is salted SHA-256 and is meant for this OS stage, not as a
complete modern password-storage design.

Authentication requests use `ioctl` IDs:

- `LEONOS_AUTH_IOCTL_STATUS`
- `LEONOS_AUTH_IOCTL_CURRENT`
- `LEONOS_AUTH_IOCTL_LIST_USERS`
- `LEONOS_AUTH_IOCTL_LOGIN`
- `LEONOS_AUTH_IOCTL_LOGOUT`
- `LEONOS_AUTH_IOCTL_CREATE_USER`
- `LEONOS_AUTH_IOCTL_UPDATE_USER`
- `LEONOS_AUTH_IOCTL_CHANGE_PASSWORD`

Task snapshots now include `uid`, `role`, `session_id`, and `username`.
Children inherit identity and current directory from the parent task.
File access decisions are made in the kernel: `kernel/ntclks/permissions.c`
compares a task's filesystem UID/GID and role against the permission value the
storage layer reports for the path. Protected service work is gated by kernel
task flags (`TASK_FLAG_SERVICE`, `TASK_FLAG_WINDOW_SERVER`) and by
`LEONOS_AUTH_ROLE_ADMIN`. There is no authorization opcode set or policy
callback in the ABI; the former `LEONOS_AUTHZ_*` request record was an internal
RPC envelope and is not published.

## Filesystem permission ABI

POSIX permissions reach userland through the Linux ABI only: `stat`, `chmod`,
`chown` and their `*at` variants. `struct leonos_fs_acl` and the
`leonos_fs_acl_*` helpers in `include/leonos/fs.h` are a libc-level convenience
API implemented on top of those same calls (see `userland/libc/src/libc.c`);
they are not ioctls and the kernel exposes no ACL ioctl. On FAT32 and exFAT the
kernel stores the resulting mode, owner and group in the hidden `LEONACL.SYS`
sidecar owned by `drivers/bootstrap/storage/storage_sidecar.c`; on ext2 it uses
the native inode fields. See
[KERNEL_USERSPACE_BOUNDARIES.md](KERNEL_USERSPACE_BOUNDARIES.md).

`struct leonos_fs_acl` contains the owner uid and up to
`LEONOS_FS_ACL_MAX_ACE` ACE rows. Supported principals are Owner, System,
Administrators, Users, and Everyone. Supported permission bits are Read/List,
Write/Create, Execute/Traverse, Delete, and Manage Permissions. ACL rows only
grant allowed permissions; an unchecked permission bit means no grant.

## Block Storage ABI

Disk tools operate on `/dev/diskN` and `/dev/diskNpN`. They obtain capacity
and sector geometry through `<linux/fs.h>` `BLKGETSIZE64` and `BLKSSZGET`,
update GPT metadata with aligned raw I/O followed by `BLKRRPART`, and mount
FAT32, exFAT, or ext2 volumes with `<sys/mount.h>` `mount(2)` and `umount2(2)`.
The SDK no longer defines LeonOS-specific disk-management ioctl records.

## Boot handoff ABI

`include/leonos/boot_handoff.h` describes exactly one thing: the record the EFI
loader hands to `kernel.sys`. `struct leonos_boot_handoff` carries the Multiboot2
and framebuffer facts, the memory maps, and the `loader`, `kernel` and
`installer_root` module ranges. `LEONOS_BOOT_HANDOFF_VERSION` is `7`.

Version 7 removed the second boot module and every table that used to cross it:
the middlelayer module range and API pointer, the kernel service table, the
mount-policy record, the VFS resolve record, and the raw-device catalog query.
Those services are now ordinary kernel-internal calls compiled into
`kernel.sys`, so no wire format is needed for them. A kernel that sees any other
handoff version rejects it in `boot_handoff_is_current()` rather than reading a
layout it does not understand.

Kernel code owns hardware probing, interrupts, page tables, physical memory,
scheduling, user pointer validation, storage block I/O, exFAT/FAT32/ext2
mutation, path resolution and the final DAC decision. The storage layer in
`drivers/bootstrap/storage/` owns on-disk metadata, including `LEONACL.SYS`.
The full ownership map is in
[KERNEL_USERSPACE_BOUNDARIES.md](KERNEL_USERSPACE_BOUNDARIES.md).

## Path resolution

Path normalization lives in the kernel: `fs_permissions_resolve()` and
`fs_permissions_resolve_flags()` in `kernel/ntclks/permissions.c` combine the
task's current directory with the input, walk components and symlinks, and check
directory search permission on the way. Whether the final symlink is followed is
decided by the syscall the caller made (for example `O_NOFOLLOW`); more than 40
traversals returns `ELOOP`. There is no resolution ABI and no separate
resolution service.

## Device catalog

`userland/apps/device-agent` is the producer: it reads `/dev` through
`leonos_readdir()`, classifies each node into `struct leonos_device_info`
(display, input, storage, audio, serial, network, system), and answers clients
over the devmand IPC that `userland/libc/src/devmand_client.c` wraps as
`leonos_device_list()`. Driver records come from `/proc/leonos-drivers`.

## Machine Identity ABI

`LEONOS_IOCTL_MACHINE_IDENTITY` returns `struct leonos_machine_identity` from
`include/leonos/system.h`. The kernel fills stable platform identity from SMBIOS
System UUID when available and augments it with boot GPT disk and ESP partition
GUIDs after storage is mounted. License code uses SMBIOS UUID as the primary
machine binding and falls back to the boot GPT GUID pair when firmware does not
provide a valid UUID. Network adapter MAC addresses are deliberately excluded
from the license machine ID so hot-adding or removing e1000 hardware does not
invalidate an existing activation.

## Network ABI

`include/leonos/net.h` exposes the network ioctl ABI. It covers configuration,
DHCP renew, DNS A lookups, ICMP ping, a compatibility fixed-buffer
`leonos_net_http_get` helper, and TCP client sockets.

Runtime DHCP renew mutates the global IPv4 configuration, so
`LEONOS_IOCTL_NET_DHCP` is restricted to administrators and trusted service
tasks. The license OOBE has a narrow pre-login exception: `/usr/lib/leonos/apps/oobe/oobe.elf`
may renew DHCP only while `/var/lib/leonos/oobe.done` is absent. Ordinary users can still
read network configuration and use DNS, HTTP, ping, and TCP client socket APIs.

Socket requests use:

- `LEONOS_IOCTL_NET_SOCKET_OPEN`
- `LEONOS_IOCTL_NET_SOCKET_CONNECT`
- `LEONOS_IOCTL_NET_SOCKET_SEND`
- `LEONOS_IOCTL_NET_SOCKET_RECV`
- `LEONOS_IOCTL_NET_SOCKET_CLOSE`
- `LEONOS_IOCTL_NET_CONNECTIONS`

libc wraps those as `leonos_socket_tcp`, `leonos_socket_connect`,
`leonos_socket_send`, `leonos_socket_recv`, `leonos_socket_close`, and
`leonos_net_connections`. A socket is an integer task-owned TCP client handle.
`connect` accepts a host name or IPv4 literal, resolves DNS A records when
needed, and returns the selected remote IP and local port. `send` and `recv` are
synchronous byte-stream operations with per-call timeouts and status fields.
Connection states exported to userland are `SYN_SENT`, `ESTABLISHED`,
`TIME_WAIT`, and `CLOSED`. `leonos_net_connections` is filtered by identity:
administrators and trusted service tasks see all sockets, while normal users
see only sockets owned by their uid.

`include/leonos/http.h` adds a libc HTTP client on top of those sockets:
`leonos_http_get`, `leonos_http_request`, and `leonos_http_resolve_url`.
It follows bounded redirects, reports final URL/status/content type, copies
response headers, decodes chunked transfer bodies, and returns truncation flags
for callers with small buffers. `httpget.elf`, `browser.elf`, and
`downloadmgr.elf` use this library for `http://` and `https://` traffic;
lower-level tools such as `ping.elf` and `netctl.elf` continue to use
ICMP/DHCP/DNS/socket status APIs directly. HTTPS uses a TLS 1.2 Mbed TLS client
profile, a bundled CA store, hostname validation, and a valid system clock. TCP
server/listener sockets, UDP sockets, cookies, cache, and full TCP window
management are still out of scope for this ABI version.

## Audio ABI

`include/leonos/audio.h` exposes a bounded, non-blocking PCM submission
interface backed by autoloaded audio driver modules. `LEONOS_IOCTL_AUDIO_CONFIGURE`
selects the stream format, `LEONOS_IOCTL_AUDIO_WRITE` submits at most 64 KiB per
call and may return a short write when the device cannot accept more data, and
`LEONOS_IOCTL_AUDIO_GET_STATE` returns device and stream state. The initial
`ac97.drv` supports QEMU's Intel ICH AC'97 controller, while `es1371.drv`
supports VMware's Ensoniq AudioPCI ES1371 controller. Both accept 16-bit,
stereo PCM at 8000–48000 Hz. The AC'97 backend keeps a persistent DMA ring and
reports short writes with `LEONOS_AUDIO_STATUS_WOULD_BLOCK`; callers should
retain and retry the unwritten tail. `doom.elf` and the built-in `wavplay.elf`
test tone use native 48000 Hz output to avoid emulator-side resampling.
`wavplay.elf` also opens matching PCM WAV files and is the default `.wav`
handler.

## Application Services

## PortableGL Rendering ABI

`/usr/lib/libportablegl.so.1` provides the PortableGL 0.101 API with the
LeonOS ABI-v1 window wrapper declared by `leonos/pgl.h`. The wrapper manages a
GUI window, an ABGR32 color buffer and a D24S8 depth/stencil buffer, and submits
frames through `leonos_gui_present_window`. Contexts are single-process and
single-current; callers must handle resize events through
`leonos_pgl_process_event` before drawing the next frame. The SDK includes the
matching `portablegl.h`, `leonos/pgl.h`, shared library and static archive. The
system build limits one draw call to 50,000 output vertices, leaving the
renderer usable within the current user address-space budget.

The launcher library in `leonos/launch.h` owns user-facing file launch policy.
It supports `.lnk` shortcuts, built-in program aliases, and persistent extension
associations stored in `/etc/leonos/fileassoc.cfg`. Settings can edit the common
associations for `.txt`, `.md`, `.html`, `.htm`, `.bmp`, `.wav`, and `.hlp`.
The default `.hlp` handler is `/usr/lib/leonos/apps/oshlp/oshlp.elf`; it accepts
`oshlp.elf <file.hlp> [doc.id]` and opens a Markdown page inside a LeonOS help
container.

Current companion applications:

- `downloadmgr.elf`: uses the libc HTTP client and saves HTTP/HTTPS downloads to
  the current user's `/home/<name>/downloads` directory.
- `imageview.elf`: opens uncompressed 24/32-bit BMP/DIB files and PNG files,
  supports Fit/1x/2x zoom, and can move to previous/next supported images in the
  same directory.
- `wavplay.elf`: plays 16-bit stereo PCM WAV files through the active audio
  driver, or a built-in test melody when started without a file.
- `oshlp.elf`: opens LeonOS `.hlp` help containers from `/usr/share/doc/leonos` or any path
  passed by another app. The help viewer uses the current system language as its
  default but language changes inside the window are local to that process.
- `serviced.elf`: protected background service runtime. Desktop starts it once
  after the window server is ready. It writes `/run/leonos/services.state`,
  consumes `/run/leonos/services.cmd`, logs to `/var/log/services.log`, and
  keeps retrying DHCP while the static fallback is active.
- `servicemgr.elf`: edits `/etc/leonos/services.cfg`, reads the runtime state file,
  and queues administrator start/stop/restart commands through
  `/run/leonos/services.cmd`.

The current service keys are `desktop`, `dhcp`, `network_icon`, `rtc_clock`,
and `ntp_sync`. `desktop` is fixed on. `dhcp` controls whether kernel boot
network initialization attempts DHCP before keeping the static fallback and is
also supervised by `serviced.elf` after the desktop starts. `network_icon` and
`rtc_clock` are read by the desktop taskbar. `ntp_sync` asks the protected
`serviced.elf` task to resolve `pool.ntp.org`, send an NTP UDP request, and set
the kernel software wall clock after validating the server reply. It retries
failed synchronization after five minutes and refreshes a successful sync every
six hours. This updates the runtime clock only; it does not write the RTC/CMOS.
## POSIX/Linux ABI migration status

The public `stat`, `fstat`, and `lstat` symbols use the musl/Linux x86-64
`struct stat` layout. LeonOS first-party code that needs the compact metadata
record must call the explicitly named `leonos_stat_legacy` or
`leonos_fstat_legacy` functions. Linux fbdev applications can open `/dev/fb0`,
map its framebuffer, and use the `FBIOGET_VSCREENINFO`, `FBIOGET_FSCREENINFO`,
and `FBIOPUT_VSCREENINFO` requests from `<linux/fb.h>`.
