# LeonOS BusyBox profile

The image builds BusyBox 1.36.1 as `/bin/busybox` with a
small, static collection of file and text applets. Double-clicking it opens a
terminal and prints the applet list. Invoke a specific applet with:

```text
/bin/busybox ls /
```

The profile includes BusyBox `ash` behind the `sh` applet with native
`fork`/`exec`, pipelines, redirections, and background process creation.
Interactive ash job-control (`jobs`/`fg`/`bg`) is disabled until the kernel
implements the Linux SIGTTIN/SIGTTOU stop-and-continue protocol; this avoids
an initialization loop before the first TTY prompt. It supports simple
command lines, shell built-ins, and the bundled applets (`ls`, `pwd`, `cat`,
`echo`, `clear`, `grep`, `head`, `tail`, `wc`, `sha256sum`, `basename`, `dirname`, `printf`, `diff`,
`less`, `ps`, and `kill`,
`mkdir`, `rmdir`, `cp`, `mv`, `rm`, `unlink`, `printenv`, `uname`, `sleep`,
`true`, `false`, `nohup`, `whoami`, and `vi`). The GUI terminal launches this
shell by default.

Storage administration commands are separate upstream programs, not BusyBox
applets. util-linux supplies `fdisk`, `sfdisk`, `blkid`, `lsblk`, `mount`,
`umount`, and `fsck`; e2fsprogs, dosfstools, and exfatprogs supply the matching
`mkfs.*` and `fsck.*` commands. `fdisk` initializes GPT directly with `g` and
uses its normal upstream interaction. FAT32 formatting requires
`mkfs.fat -F 32`; the `mkfs.fat32` name is only a symlink. Pass `-n` to a
filesystem checker when a read-only check is intended.

These programs access `/dev/disk0` and `/dev/disk0pN` through standard file
I/O, Linux block-device ioctls, and `mount(2)`/`umount2(2)`. Formatting,
partition changes, and mount operations require an administrator account.
`leonos-grub-installer ESP` remains a separate LeonOS script that copies the
prebuilt EFI/GRUB payload to an already-mounted ESP. The installer ISO also
retains the older installer-only `gptinit` utility, but it is no longer needed
for blank disks because upstream `fdisk` can create GPT itself.

Examples:

```text
fdisk -l /dev/disk0
fdisk /dev/disk0
blkid
lsblk
fsck.ext2 /dev/disk0p3
mkfs.ext2 -F /dev/disk0p3
mkfs.fat -F 32 /dev/disk0p1
mount -t ext2 /dev/disk0p3 /mnt/data
umount /mnt/data
```

Ash's fancy prompt support is enabled: `\\w` expands to the current directory
and `\\$` expands to `$` for ordinary users or `#` for root. BusyBox's line
editor calculates the visible prompt width while the Terminal consumes ANSI
color sequences without moving its cursor, so colored prompts can use the
usual `\\[...\\]` markers.

In TTY mode, `~` and `~/path` resolve to the home directory of the account that
logged in. This is resolved from the current LeonOS session, so it remains
correct even though the shell starts before the login program completes.

Interactive Ash uses BusyBox's line editor with Tab command/path completion. The terminal sends
the Tab byte to the PTY and applies only the cursor updates returned by Ash or the foreground
program, so programs that do not implement four-column Tab stops are not locally mis-rendered.

`diff` produces unified file differences.  `less` provides keyboard-controlled
pagination for text files; its input is capped at 8,192 lines to keep malformed
or exceptionally large files from exhausting the current user-space budget.
`ls` emits ANSI file-type colors by default when its output is a terminal; use
`ls --color=never` when plain output is required.

`grep` searches standard input or files with POSIX basic regular expressions.
It supports literal (`-F`), extended (`-E`), case-insensitive (`-i`), line-number
(`-n`), count (`-c`), recursive (`-r`), and before/after context (`-A`, `-B`, and
`-C`) modes.

`nohup PROG ARGS` is available in both the GUI Terminal and TTY shell. It
ignores `SIGHUP`, changes terminal stdin to `/dev/null`, and appends terminal
stdout/stderr to `nohup.out` in the current directory, falling back to
`$HOME/nohup.out` when the current directory is not writable.

`cp`, `mv`, and `rm` operate on regular files and directories through the
LeonOS filesystem ABI. Symbolic links, ownership changes, and special device
nodes remain unsupported by the filesystem and return an error.

The `file` command is provided as an external program backed by upstream
libmagic. Ash resolves it to `/usr/bin/file`; the matching
compiled database is installed at `/usr/share/misc/magic.mgc`.
`fastfetch` is likewise resolved to `/usr/lib/leonos/apps/fastfetch/fastfetch.elf`.
The `sl` terminal joke is resolved to `/usr/bin/sl`.

The kernel provides process inspection through the task snapshot ABI,
same-user signal termination, COW `fork`, `execve`, process groups, foreground
PTY groups, and nice-style priorities. `kill` and graphical task tools use
those interfaces. Ash uses normal pipelines and redirections (`<`, `>`, `>>`,
`2>`), and handles `Ctrl+C` through the PTY input path. Interactive ash job
control remains disabled because the kernel does not yet implement the
SIGTTIN/SIGTTOU stop-and-continue protocol. The
POSIX `SIG_DFL` and `SIG_IGN` dispositions are available; arbitrary user-space
signal handlers and shared file offsets after `fork` are not yet exposed.
Ash does not use the legacy PTY-launch adapter: its commands use the upstream
MMU `fork`/`pipe`/`dup2`/`execvp`/`waitpid` flow. The remaining
BusyBox adapter only maps bare applet names to the single
`/bin/busybox` executable and maps bundled external tools
to their installed paths.

BusyBox is GPL-2.0-only; `LICENSE` and upstream version information are staged
beside the executable in the image.
