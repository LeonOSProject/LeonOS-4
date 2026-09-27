#ifndef RELIEFOS_TAR_H
#define RELIEFOS_TAR_H

#include <stdint.h>

#define RELIEFOS_TAR_BLOCK_SIZE 512U
#define RELIEFOS_TAR_NAME_LEN 100U
#define RELIEFOS_TAR_MAX_FILE_SIZE (32U * 1024U * 1024U)

#define RELIEFOS_TAR_TYPE_FILE '0'
#define RELIEFOS_TAR_TYPE_DIR '5'

typedef int (*reliefos_tar_progress_fn)(uint32_t processed, uint32_t total,
                                      void *context);

int reliefos_tar_create(const char *tar_path);
int reliefos_tar_pack_file(const char *tar_path, const char *file_path,
                         const char *stored_name);
int reliefos_tar_pack_file_append(int tar_fd, const char *file_path,
                                const char *stored_name);
int reliefos_tar_finalize(int tar_fd);
int reliefos_tar_pack_dir(const char *tar_path, const char *dir_path);
int reliefos_tar_pack_dir_append(int tar_fd, const char *dir_path);
int reliefos_tar_extract_file(const char *tar_path, const char *stored_name,
                            const char *dest_path);
int reliefos_tar_extract_all(const char *tar_path, const char *dest_dir);
int reliefos_tar_extract_all_with_progress(const char *tar_path,
                                         const char *dest_dir,
                                         reliefos_tar_progress_fn progress,
                                         void *context);
int reliefos_tar_list(const char *tar_path, char *output, uint32_t capacity);

#endif
