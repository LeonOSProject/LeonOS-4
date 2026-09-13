#ifndef NTCLKS_TMPFS_H
#define NTCLKS_TMPFS_H
#include <ntclks/storage.h>

/* Callers serialize these operations with the storage execution lock. */
struct tmpfs_super;
int tmpfs_new(uint32_t volume, uint64_t flags, const char *options, uint32_t uid, uint32_t gid,
              struct tmpfs_super **out);
void tmpfs_destroy(struct tmpfs_super *fs);
int tmpfs_lookup(struct tmpfs_super *fs, const char *path, struct storage_node *out);
int tmpfs_create(struct tmpfs_super *fs, const char *path, uint32_t mode, const char *target,
                 struct storage_node *out);
int tmpfs_read(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, void *buffer, uint32_t length,
               uint32_t *done);
int tmpfs_write(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, const void *buffer, uint32_t length,
                uint32_t *done);
int tmpfs_truncate(struct tmpfs_super *fs, uint32_t ino, uint64_t length);
/* Returns a retained file page. The caller releases it with mm_free_page(). */
int tmpfs_get_page(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, uint64_t *phys);
int tmpfs_readdir(struct tmpfs_super *fs, uint32_t ino, uint64_t *offset, struct leonos_dir_entry *out);
int tmpfs_unlink(struct tmpfs_super *fs, const char *path, bool directory);
int tmpfs_link(struct tmpfs_super *fs, const char *old_path, const char *new_path);
int tmpfs_rename(struct tmpfs_super *fs, const char *old_path, const char *new_path);
int tmpfs_readlink(struct tmpfs_super *fs, uint32_t ino, void *buffer, uint32_t length, uint32_t *done);
int tmpfs_hold(struct tmpfs_super *fs, uint32_t ino);
void tmpfs_drop(struct tmpfs_super *fs, uint32_t ino);
int tmpfs_stat(struct tmpfs_super *fs, uint32_t ino, struct linux_stat_abi *out);
int tmpfs_permissions(struct tmpfs_super *fs, uint32_t ino, struct leonos_permissions *value, bool write);
int tmpfs_utimens(struct tmpfs_super *fs, uint32_t ino, int64_t atime, int64_t mtime, bool set_atime,
                  bool set_mtime);
void tmpfs_statfs(struct tmpfs_super *fs, struct linux_statfs_abi *out);
void tmpfs_set_flags(struct tmpfs_super *fs, uint64_t flags);
#endif
