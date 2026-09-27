#ifndef RELIEFOS_FS_H
#define RELIEFOS_FS_H

#include <reliefos/fs_abi.h>

int reliefos_list_dir(const char *path, struct reliefos_dir_entry *entries, uint32_t capacity, uint32_t *out_count);
int reliefos_stat_legacy(const char *path, struct reliefos_stat *st);
int reliefos_fstat_legacy(int fd, struct reliefos_stat *st);
int reliefos_readdir(int fd, struct reliefos_dir_entry *entry);
int reliefos_fs_acl_get(const char *path, struct reliefos_fs_acl *acl);
int reliefos_fs_acl_set(const char *path, const struct reliefos_fs_acl *acl);
int reliefos_fs_acl_take_ownership(const char *path, struct reliefos_fs_acl *acl);
int reliefos_fs_acl_repair(const char *path, struct reliefos_fs_acl *acl);

#endif
