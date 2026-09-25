/*
 * LeonOS storage interface: declares filesystem and storage-node operations.
 * Provides path lookup, directory access, file reads, and mount abstractions.
 */
#ifndef NTCLKS_STORAGE_H
#define NTCLKS_STORAGE_H

#include <leonos/boot_handoff.h>
#include <leonos/fs_abi.h>
#include <leonos/system_abi.h>
#include <ntclks/types.h>
#include <linux/stat.h>
#include <linux/statfs.h>

/**
 * @brief Internal POSIX permission triple carried between the storage
 *        backends, the ACL sidecar and the permission checker.
 *
 * Ring-3 code never sees this layout: userland reads and writes POSIX
 * permissions through the Linux stat/chmod/chown ABI, which is translated at
 * the syscall boundary.  Keeping it here stops an internal representation from
 * being frozen as a published ABI.
 */
struct leonos_permissions {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
};

/* Legacy installation records are internal-only while boot storage is being
 * simplified.  They are deliberately absent from the public SDK; userland
 * reaches disks exclusively through /dev and Linux-compatible interfaces. */
#define LEONOS_INSTALL_MAX_DISKS 8U
#define LEONOS_INSTALL_DISK_FLAG_BOOT_ROOT 0x00000001U
#define LEONOS_INSTALL_DISK_FLAG_TARGET_MOUNTED 0x00000002U
#define LEONOS_DISK_MAX_PARTITIONS 128U
#define LEONOS_DISK_PARTITION_NAME_LEN 72U
#define LEONOS_DISK_PARTITION_TYPE_BASIC_DATA 1U
#define LEONOS_DISK_PARTITION_TYPE_ESP 2U
#define LEONOS_DISK_PARTITION_TYPE_LINUX 3U
#define LEONOS_DISK_PARTITION_EDIT_TYPE 0x00000001U
#define LEONOS_DISK_PARTITION_EDIT_NAME 0x00000002U
#define LEONOS_DISK_FILESYSTEM_UNKNOWN 0U
#define LEONOS_DISK_FILESYSTEM_FAT32 1U
#define LEONOS_DISK_FILESYSTEM_EXT2 2U
#define LEONOS_DISK_FILESYSTEM_ISO9660 3U
#define LEONOS_DISK_FILESYSTEM_EXFAT 4U
#define LEONOS_DISK_PARTITION_FLAG_ESP 0x00000001U
#define LEONOS_DISK_PARTITION_FLAG_BOOT_ROOT 0x00000002U
#define LEONOS_DISK_PARTITION_FLAG_TARGET_MOUNTED 0x00000004U
#define LEONOS_DISK_PARTITION_FLAG_PROTECTED 0x00000008U
#define LEONOS_DISK_PARTITION_FLAG_MOUNTED 0x00000010U
#define LEONOS_DISK_GPT_INITIALIZE_FORCE 0x00000001U

struct leonos_install_disk {
    uint32_t id, port, sector_size, flags;
    uint64_t sector_count;
    char name[32];
};
struct leonos_disk_partition {
    uint32_t disk_id, index, filesystem, flags;
    char mount_path[LEONOS_FS_PATH_LEN];
    uint64_t first_lba, sector_count;
    uint8_t type_guid[16];
    char name[LEONOS_DISK_PARTITION_NAME_LEN];
};
struct leonos_disk_partition_format { uint32_t disk_id, partition_index, filesystem, reserved; };
struct leonos_disk_partition_delete { uint32_t disk_id, partition_index, reserved0, reserved1; };
struct leonos_disk_partition_create { uint32_t disk_id, filesystem, size_mib, reserved; char name[LEONOS_DISK_PARTITION_NAME_LEN]; };
struct leonos_disk_partition_mount { uint32_t disk_id, partition_index; char mount_path[LEONOS_FS_PATH_LEN]; };
struct leonos_disk_partition_unmount { uint32_t disk_id, partition_index, reserved0, reserved1; };
struct leonos_disk_partition_edit { uint32_t disk_id, partition_index, edit_mask, type, reserved; char name[LEONOS_DISK_PARTITION_NAME_LEN]; };
struct leonos_disk_gpt_initialize { uint32_t disk_id, flags; };

struct storage_node {
    uint32_t type;
    uint32_t flags;
    uint32_t first_cluster;
    uint32_t volume_id;
    uint64_t size;
};

struct storage_inode_ref;
int storage_inode_get(const struct storage_node *node, struct storage_inode_ref **out);
void storage_inode_retain(struct storage_inode_ref *reference);
int storage_inode_put(struct storage_inode_ref *reference);
int storage_inode_refresh(struct storage_node *node);
int storage_node_mount_flags(const struct storage_node *node, uint64_t *flags);
int storage_tmpfs_get_page(const struct storage_node *node, uint64_t offset, uint64_t *phys);
int storage_remount_path(const char *path, uint64_t flags);
int storage_mount_tmpfs(const char *source, const char *target, uint64_t flags,
                         const char *options, uint32_t uid, uint32_t gid);
int storage_sync_volume(uint32_t volume_id);
int storage_sync_all(void);
int storage_sync_disk(uint32_t disk_id);
int storage_write_held_node(struct storage_node *node, uint64_t offset,
                            const void *buffer, uint32_t length, uint32_t *written);
int storage_truncate_held_node(struct storage_node *node, uint64_t length);

int storage_inode_permissions(const struct storage_node *node,
                              struct leonos_permissions *value, bool write);
int storage_inode_stat(const struct storage_node *node, struct linux_stat_abi *value);
int storage_inode_utimensat(const struct storage_node *node, int64_t atime, int64_t mtime,
                            bool set_atime, bool set_mtime);
/**
 * @brief Read or write uid/gid/mode for a member of a sidecar metadata volume.
 * @param path Absolute, already-normalized path; ':' is not accepted.
 * @param value Metadata; input for write, output otherwise. Must not be NULL.
 * @param write True to store `value` for this path, false to recall it.
 * @return Zero, -EINVAL for a malformed path, or -EIO when the directory's
 *         sidecar file is damaged. A path with no stored record yields the
 *         filesystem default rather than an error.
 * @context Process context like the other storage entry points; performs file
 *          I/O on the volume's own locking and holds no lock of its own.
 */
int storage_sidecar_permissions(const char *path, struct leonos_permissions *value,
                                bool write);
/**
 * @brief Drop the sidecar record of a member that has just been deleted.
 * @param path Absolute path that no longer exists.
 * @return Zero, or a negative errno when the directory's records could not be
 *         rewritten. A damaged sidecar is left untouched and reported as zero.
 * @context Process context; performs file I/O.
 */
int storage_sidecar_note_deleted(const char *path);
/**
 * @brief Re-key a sidecar record after a rename within the same directory.
 * @param path Absolute path before the rename.
 * @param new_path Absolute path after the rename; must share `path`'s directory,
 *                 otherwise nothing changes and zero is returned.
 * @return Zero, or a negative errno when the records could not be rewritten.
 * @context Process context; performs file I/O.
 */
int storage_sidecar_note_renamed(const char *path, const char *new_path);
/**
 * @brief Create a socket or FIFO inode on a supporting filesystem.
 * @param path Resolved, parent-authorized absolute path.
 * @param mode S_IFSOCK or S_IFIFO; permissions are applied separately.
 * @param out Receives the created inode identity.
 * @return Zero or negative errno, including EOPNOTSUPP for unsupported backends.
 */
int storage_create_special(const char *path, uint32_t mode, struct storage_node *out);
int storage_create_socket(const char *path, struct storage_node *out);
/**
 * @brief Creates a symbolic link on the filesystem containing a resolved parent.
 * @param target Literal link text; never resolved during creation.
 * @param path Absolute link name with its parent already resolved and authorized.
 * @return Zero or negative errno; serializes allocation and restores the active volume.
 */
int storage_symlink(const char *target, const char *path);
/**
 * @brief Reads literal symlink bytes while holding the storage execution lock.
 * @param path Absolute name whose intermediate components have been resolved.
 * @param buffer Writable kernel buffer; no NUL terminator is appended.
 * @param capacity Positive buffer capacity in bytes.
 * @param out_len Optional byte count, reset to zero on failure.
 * @return Zero or negative errno, including EINVAL for a non-symlink.
 */
int storage_readlink(const char *path, char *buffer, uint32_t capacity, uint32_t *out_len);
int storage_statfs(const struct storage_node *node, struct linux_statfs_abi *value);

/**
 * @brief Maintains the next FAT32 cluster for one sequential file reader.
 *
 * The cursor is deliberately owned by the open file descriptor.  It is not
 * part of a storage node because several descriptors may read the same node
 * at different offsets.
 */
struct storage_read_cursor {
    uint64_t offset;
    uint32_t cluster;
    uint32_t valid;
};

#define STORAGE_NODE_FLAG_ROOT    0x00000001u
#define STORAGE_NODE_FLAG_DEV_DIR 0x00000002u
#define STORAGE_NODE_FLAG_DEV_FB0 0x00000004u
#define STORAGE_NODE_FLAG_EXT2    0x00000008u
#define STORAGE_NODE_FLAG_EXFAT   0x00000010u
#define STORAGE_NODE_FLAG_EXFAT_NOFAT 0x00000020u
#define STORAGE_NODE_FLAG_DEV_NODE 0x00000040u
#define STORAGE_NODE_FLAG_DEV_BLOCK 0x00000080u
#define STORAGE_NODE_FLAG_PROC    0x00000100u
#define STORAGE_NODE_FLAG_SYSFS 0x00000400u
#define STORAGE_NODE_FLAG_PTY   0x00000800u
#define STORAGE_NODE_FLAG_TMPFS 0x00001000u
#define STORAGE_SYSFS_DEVICE 202u
#define STORAGE_NODE_FLAG_DEV_LINK 0x00000200u
/* Anonymous filesystem device numbers, also exported in proc mountinfo. */
#define STORAGE_DEVFS_DEVICE 200u
#define STORAGE_PROCFS_DEVICE 201u

/* Device-node volume_id encoding for block devices.  The low 16 bits select
 * the physical disk; the high 16 bits contain GPT entry + 1, or zero for the
 * whole disk.  Device nodes never use volume_id for mounted-volume lookup. */
#define STORAGE_BLOCK_DISK_ID(value) ((uint32_t)(value) & 0xffffu)
#define STORAGE_BLOCK_PARTITION(value) ((int32_t)(((uint32_t)(value) >> 16) & 0xffffu) - 1)
#define STORAGE_BLOCK_MAJOR 259u
#define STORAGE_BLOCK_MINOR(value) (STORAGE_BLOCK_DISK_ID(value) * 256u + STORAGE_BLOCK_PARTITION(value) + 1u)
static inline uint64_t storage_block_rdev(uint32_t value)
{
    uint32_t minor = STORAGE_BLOCK_MINOR(value);
    return ((uint64_t)STORAGE_BLOCK_MAJOR << 8) | (minor & 255u) | ((uint64_t)(minor & ~255u) << 12);
}
#define STORAGE_BLOCK_VOLUME_ID(disk, partition) \
    ((uint32_t)(disk) & 0xffffu) | ((uint32_t)((partition) + 1) << 16)

/* Synthetic devfs node kinds.  They are stored in storage_node.first_cluster
 * for device nodes; filesystem nodes continue to use that field as their
 * backend cluster/inode identifier. */
#define STORAGE_DEV_KIND_DIR       1u
#define STORAGE_DEV_KIND_NULL      2u
#define STORAGE_DEV_KIND_ZERO      3u
#define STORAGE_DEV_KIND_FULL      4u
#define STORAGE_DEV_KIND_RANDOM    5u
#define STORAGE_DEV_KIND_URANDOM   6u
#define STORAGE_DEV_KIND_TTY       7u
#define STORAGE_DEV_KIND_CONSOLE   8u
#define STORAGE_DEV_KIND_PTMX      9u
#define STORAGE_DEV_KIND_FB0       10u
#define STORAGE_DEV_KIND_KEYBOARD  11u
#define STORAGE_DEV_KIND_MOUSE     12u
#define STORAGE_DEV_KIND_AUDIO     13u
#define STORAGE_DEV_KIND_SERIAL    14u
#define STORAGE_DEV_KIND_DISK      15u
#define STORAGE_DEV_KIND_INPUT_DIR 16u
#define STORAGE_DEV_KIND_PTS_DIR   17u
#define STORAGE_DEV_KIND_NET       18u
#define STORAGE_DEV_KIND_RTC       19u
#define STORAGE_DEV_KIND_KMSG      20u
#define STORAGE_DEV_KIND_GPU         24u
#define STORAGE_DEV_KIND_SHM         25u
#define STORAGE_DEV_KIND_DISK_DIR    26u
#define STORAGE_DEV_KIND_PARTUUID_DIR 27u
#define STORAGE_DEV_KIND_DRIVERCTL 28u

struct boot_info;

/**
 * @brief Initialize the storage subsystem and mount the boot filesystems.
 */
void storage_init(void);
/**
 * @brief Toggle asynchronous I/O for the calling context on or off.
 */
void storage_set_io_async_context(bool enabled);
/* Read under the kernel execution lock; changes on writes and mount/cache resets. */
uint64_t storage_metadata_generation(void);
/**
 * @brief Abandon any in-flight I/O owned by pid without completing it.
 */
void storage_release_task_io(uint32_t pid);
/**
 * @brief Wait for pid's outstanding I/O to finish.
 */
void storage_drain_task_io(uint32_t pid);
/**
 * @brief Mount the installer's root filesystem from boot info.
 */
void storage_init_installer_root(const struct boot_info *boot);
/**
 * @brief Mount the root filesystem for this boot.
 * @param boot Parsed Multiboot modules and kernel command line; the installer
 *             RAM disk is located by module name within it.
 * @param ramdisk_root True for an installer or live session, whose root is the
 *                     `leonos-installer-root` module instead of a partition.
 * @return Nothing. A failed RAM-root mount leaves no root ready; the caller
 *         retries through storage_init_installer_root() to report why.
 * @context Boot phase, before any task can issue storage syscalls. Probes and
 *          writes disks through the storage subsystem's own locking.
 */
void storage_mount_boot_root(const struct boot_info *boot, bool ramdisk_root);
/**
 * @brief Return true once the root filesystem is mounted and usable.
 */
bool storage_ready(void);
/**
 * @brief Returns the mounted filesystem type for the runtime root volume.
 * @return Stable lowercase filesystem name, or "none" before a root mounts.
 */
const char *storage_root_filesystem_name(void);
/**
 * @brief Return true when the installer's root filesystem is mounted.
 */
bool storage_installer_root_active(void);
/**
 * @brief Mount a boot-module FAT32 image (len bytes) as a writable live root; 0 on success.
 *
 * The caller owns the image backing and must keep it mapped and reserved for
 * the lifetime of the mount. Writes are kept in memory for the current
 * session and never reach the ISO source; the installer passes its Multiboot
 * module, which the physical-memory manager reserves before storage starts.
 */
int storage_mount_ramdisk_root(const void *image, uint64_t len);
/**
 * @brief Resolve input against cwd into out (cap bytes); 0 on success.
 */
int storage_resolve_path(const char *cwd, const char *input, char *out, uint32_t cap);
/** Resolves a path to its internal mounted-volume identity. */
int storage_path_volume_id(const char *path, uint32_t *out_volume_id);
/**
 * @brief Look up path and fill out with its storage node; 0 on success.
 */
int storage_lookup_path(const char *path, struct storage_node *out);
/**
 * @brief Read len bytes of node from offset into buf, reporting bytes read in out_read.
 */
int storage_read_node(const struct storage_node *node, uint64_t offset,
                      void *buf, uint32_t len, uint32_t *out_read);
/**
 * @brief Reads a file range while reusing a cursor for sequential FAT32 I/O.
 * @param node File node to read.
 * @param offset Byte offset at which to begin the read.
 * @param buf Destination buffer.
 * @param len Maximum number of bytes to read.
 * @param out_read Receives the number of bytes copied.
 * @param cursor Optional descriptor-owned sequential-read cursor.
 * @return Zero on success or a negative storage error.
 */
int storage_read_node_cursor(const struct storage_node *node, uint64_t offset,
                             void *buf, uint32_t len, uint32_t *out_read,
                             struct storage_read_cursor *cursor);
/**
 * @brief Read the next directory entry of node into entry, advancing cursor; 0 on success.
 */
int storage_readdir_node(const struct storage_node *node, uint64_t *cursor,
                         struct leonos_dir_entry *entry);
/**
 * @brief Load the whole file at path into a buffer returned via out_data/out_len.
 */
int storage_read_file(const char *path, const void **out_data, size_t *out_len);
/**
 * @brief Overwrite the file at path with len bytes from buf; 0 on success.
 */
int storage_write_file(const char *path, const void *buf, uint32_t len);
/**
 * @brief Resize the file at path to length bytes; 0 on success.
 */
int storage_truncate_file(const char *path, uint64_t length);
/**
 * @brief Write len bytes of buf to the node at path from offset; reports bytes in out_written.
 */
int storage_write_node(const char *path, uint64_t offset,
                       const void *buf, uint32_t len, uint32_t *out_written);
/**
 * @brief List up to capacity directory entries of path into entries; count in out_count.
 */
int storage_list_dir(const char *path, struct leonos_dir_entry *entries,
                     uint32_t capacity, uint32_t *out_count);
/**
 * @brief Fill st with metadata for path; 0 on success.
 */
int storage_stat_path(const char *path, struct leonos_stat *st);
/** @brief Read a byte range of the current mount table, with proc-style escaping. */
int storage_read_mounts(uint64_t offset, void *buffer, uint32_t capacity,
                         uint32_t *out_read);
/** @brief Read Linux mountinfo with IDs, parentage, devices and escaped paths. */
int storage_read_mountinfo(uint64_t offset, void *buffer, uint32_t capacity,
                           uint32_t *out_read);
/** @brief Return a GPT UUID in a 37-byte buffer under the storage lock, or negative errno. */
int storage_disk_partition_uuid(uint32_t disk_id, uint32_t partition_index, char uuid[37]);
/**
 * @brief Create the directory path; 0 on success.
 */
int storage_mkdir(const char *path);
/**
 * @brief Delete the file path; 0 on success.
 */
int storage_unlink(const char *path);
/**
 * Writes a small control file to the ESP belonging to the current boot disk.
 * The ESP is available through the normal `/boot` mount.
 */
int storage_write_boot_esp_file(const char *path, const void *buf, uint32_t len);
/** Removes a control file from the current boot disk ESP. */
int storage_unlink_boot_esp_file(const char *path);
/**
 * @brief Remove the empty directory path; 0 on success.
 */
int storage_rmdir(const char *path);
/**
 * @brief Rename old_path to new_path; 0 on success.
 */
int storage_rename(const char *old_path, const char *new_path);
/** @brief Create a hard link to an existing regular file. */
int storage_link(const char *old_path, const char *new_path);
/**
 * @brief List up to capacity install disks into disks; count in out_count.
 */
int storage_install_list_disks(struct leonos_install_disk *disks,
                               uint32_t capacity, uint32_t *out_count);
/**
 * @brief Format the EFI system partition on disk_id; 0 on success.
 */
int storage_install_format_esp(uint32_t disk_id);
/**
 * @brief Formats an installer target as a GPT disk with FAT32 ESP and ext2 root.
 * @param disk_id Installer-selected AHCI, IDE/PATA, or NVMe disk identifier.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_install_format_target(uint32_t disk_id);
/**
 * @brief Mount the installer target on disk_id for file placement; 0 on success.
 */
int storage_install_mount_target(uint32_t disk_id);
/**
 * @brief Lists GPT partitions on a detected block disk exposed to disk management.
 * @param disk_id Detected AHCI, IDE/PATA, or NVMe disk identifier.
 * @param partitions Caller buffer receiving partition metadata.
 * @param capacity Number of partition records available in @p partitions.
 * @param out_count Receives the complete number of usable GPT entries.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_list_partitions(uint32_t disk_id,
                                 struct leonos_disk_partition *partitions,
                                 uint32_t capacity, uint32_t *out_count);
/** Returns the LBA range represented by a whole-disk or partition node. */
int storage_disk_block_info(uint32_t disk_id, int32_t partition_index,
                            uint64_t *out_first_lba, uint64_t *out_sector_count);
/** Reads/writes a block node at a byte offset; offsets and lengths are sector aligned. */
int storage_disk_block_read(uint32_t disk_id, int32_t partition_index,
                            uint64_t offset, void *buffer, uint32_t length,
                            uint32_t *out_read);
int storage_disk_block_write(uint32_t disk_id, int32_t partition_index,
                             uint64_t offset, const void *buffer, uint32_t length,
                             uint32_t *out_written);
/** Revalidates a disk's GPT metadata after an external partition-table change. */
int storage_disk_block_reread(uint32_t disk_id);
/** Mount a GPT partition exposed through a /dev block node. */
int storage_mount_block_partition(uint32_t disk_id, uint32_t partition_index,
                                  const char *target, const char *filesystem,
                                  uint64_t flags, uint32_t *out_volume_id);
/** Resolve an exact mounted path to its internal volume identity. */
int storage_mount_path_volume_id(const char *target, uint32_t *out_volume_id);
/** Tear down an exact standard mount after the syscall layer checks use. */
int storage_unmount_path(const char *target, uint32_t *out_volume_id);
/**
 * @brief Formats an unprotected GPT partition as FAT32, exFAT, or ext2.
 * @param request Partition selector and requested filesystem.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_format_partition(const struct leonos_disk_partition_format *request);
/**
 * @brief Removes one unprotected GPT partition entry without wiping its data area.
 * @param request Partition selector.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_delete_partition(const struct leonos_disk_partition_delete *request);
/**
 * @brief Creates and formats one GPT data partition in an unallocated disk range.
 * @param request Disk, filesystem, size, and display-name request.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_create_partition(const struct leonos_disk_partition_create *request);
/**
 * @brief Mounts one FAT32, exFAT, or ext2 data partition at its deterministic path.
 * @param request Disk and GPT-entry selector; receives the mount path.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_mount_partition(struct leonos_disk_partition_mount *request);
/**
 * @brief Returns the mounted path for a runtime data partition.
 * @param disk_id AHCI, IDE/PATA, or NVMe disk identifier.
 * @param partition_index Zero-based GPT entry index.
 * @param out_path Receives the mounted absolute path.
 * @return Zero when mounted, or a negative errno-style storage error.
 */
int storage_disk_partition_mount_path(uint32_t disk_id, uint32_t partition_index,
                                      char *out_path, uint32_t capacity);
/** Returns the internal mounted-volume identity for kernel busy checks. */
int storage_disk_partition_volume_id(uint32_t disk_id, uint32_t partition_index,
                                     uint32_t *out_volume_id);
/**
 * @brief Tears down one runtime data-partition mount after the kernel checks usage.
 * @param request Disk and GPT-entry selector.
 * @return Zero on success or a negative errno-style storage error.
 */
int storage_disk_unmount_partition(const struct leonos_disk_partition_unmount *request);
/** Edit GPT partition type GUID and/or UTF-16 name metadata. */
int storage_disk_edit_partition(const struct leonos_disk_partition_edit *request);
/** Initialize an empty primary/backup GPT on a managed disk. */
int storage_disk_initialize_gpt(const struct leonos_disk_gpt_initialize *request);
/**
 * @brief Fill identity with the boot disk/model information known to storage.
 */
void storage_boot_identity(struct leonos_machine_identity *identity);

#endif
