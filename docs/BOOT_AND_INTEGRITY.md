# Boot and Integrity

## Boot flow

LeonOS 4 boots through GRUB and the custom loader:

1. GRUB starts `boot/loader.elf` through Multiboot2.
2. The loader locates `kernel.sys`.
3. The loader validates that file before loading its ELF image.
4. The kernel receives a `struct leonos_boot_handoff` with the loader, kernel and
   installer-root module ranges. Its version is
   `LEONOS_BOOT_HANDOFF_VERSION`; a kernel that expects a different layout
   refuses the handoff instead of partially reading it.

During early boot, normal disk images load components from the FAT32 ESP:

- `/boot/loader.elf`
- `/leonos/kernel.sys`

After the kernel starts, its storage layer mounts the separate ext2 partition
as the normal `/` runtime root. The ESP stays separate so a full root cannot
consume UEFI boot space.

Installer and live ISOs pass the kernel and installer root as GRUB modules:

- `/leonos/kernel.sys` with module tag `leonos-kernel`
- `/install/root.fat` with module tag `leonos-installer-root`

The installer root remains resident for the installer session. It is accessed
through a shared supervisor-only high direct map so user page tables cannot
replace its low physical placement. The VM must provide enough RAM for GRUB to
load the whole module; a 400 MiB root is supported with 1 GiB or more of guest
memory.

GRUB chooses module placement, while `kernel.sys` has a
fixed physical `PT_LOAD` destination. Before loading the executable, the
Loader checks that destination against the installer-root module. When they
overlap, it allocates replacement EFI LoaderData pages below 4 GiB, copies the
module, and records the new range in the boot handoff. That remains inside the
kernel's 16 GiB direct map. The kernel imports the range before physical-memory
initialization, so the original Multiboot range can be reclaimed without
corrupting the FAT filesystem.

## Build-time hashes

`tools/build/loader-integrity.sh` calculates the SHA-256 hash of
`$(O_GENERATED)/system/kernel.sys` and publishes
`$(O_INCLUDE)/generated/loader_integrity.h`, which the loader objects include.
`mk/boot.mk` declares that header as a prerequisite of every loader object, so a
changed kernel image relinks the loader; the rule's
`$(O_META)/boot.sig` command signature covers the compiler, flags and source
list. Do not hand-edit the generated header.

## Runtime behavior

Before `elf_load_exec`, the loader hashes the raw component bytes it is about
to load and compares them with the generated expected hash.

If they match, the serial log reports integrity success. If the component
differs, the loader prints a warning with expected and actual
SHA-256 values and waits for a user decision:

- `Y`: continue booting anyway.
- `N`: stop at the loader warning.

This check runs for both normal EFI filesystem loading and installer GRUB
module loading.

## GRUB framebuffer boot log

After GRUB supplies the Multiboot2 framebuffer tag, the loader creates an
on-screen boot log using the built-in 8x16 PSF font. All subsequent loader
serial output is mirrored to this panel, including component discovery,
integrity results, load failures, and the kernel handoff. The loader records
the panel geometry and cursor in `struct leonos_boot_handoff`, so the kernel
bootstrap console appends its startup log to the same GRUB framebuffer panel
instead of opening a separate top-corner framebuffer console.

The panel uses Metro blue by default and switches to the persisted Win95 or
Metro theme after the loader reads `/etc/leonos/display.conf`. It requires a 32-bit
linear framebuffer; serial logging remains available when GOP/framebuffer
output is unavailable.

`bootlog-pause=1` also enables the log panel, repeats the SVGA3D initialization
summary after the other boot services, and waits for Enter before launching
user processes. The installed system's `with boot log` and `SVGA II 3D test`
entries and the installer's `Enable boot log screen` entry enable this pause.
The normal boot entries and `bootlog=1` alone still continue automatically.
IRQ/timer input remains active while paused; queued input, mouse events, and
key releases are ignored. Press Enter in the virtual machine to resume boot.

## Installer compatibility

The installer writes target disks through AHCI, legacy IDE/PATA PIO, or NVMe.
NVMe namespaces with 512-byte logical sectors are supported as boot and install
targets. In VirtualBox, attach the destination VDI through a SATA/AHCI,
NVMe, or default PIIX4 IDE controller. IDE/ATAPI optical media is read-only
and is supported for ISO reads.

The installer payload is built from the same matched runtime staging tree:

- `build/esp` contains the loader, kernel, resources, config, and
  userland applications for the installed system.
- `tools/make_installer_root.py` splits `build/esp` into `install/esp` (the
  FAT32 ESP boot subset) and `install/root` (the exFAT runtime root) inside
  `build/install/root.fat`.
- `tools/make_installer_iso.py` stages top-level installer copies of
  `boot/loader.elf`, `system/kernel.sys`, and
  `install/root.fat`.

This keeps a freshly installed system consistent with the loader hashes instead
of producing an install that fails the next boot's integrity check.

## Trust boundary

This feature detects a mismatch between the loader's compiled-in component
hash and the `kernel.sys` bytes it is about to execute. It
is not a full Secure Boot chain, signature system, or anti-rollback mechanism.
If an attacker replaces the loader and its embedded hashes together, that is
outside the current trust model.
