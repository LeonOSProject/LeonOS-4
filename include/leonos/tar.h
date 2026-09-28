/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/tar.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_TAR_H
#define LEONOS_TAR_H
#include <reliefos/tar.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_TAR_BLOCK_SIZE RELIEFOS_TAR_BLOCK_SIZE
#define LEONOS_TAR_MAX_FILE_SIZE RELIEFOS_TAR_MAX_FILE_SIZE
#define LEONOS_TAR_NAME_LEN RELIEFOS_TAR_NAME_LEN
#define LEONOS_TAR_TYPE_DIR RELIEFOS_TAR_TYPE_DIR
#define LEONOS_TAR_TYPE_FILE RELIEFOS_TAR_TYPE_FILE
#define leonos_tar_create reliefos_tar_create
#define leonos_tar_extract_all reliefos_tar_extract_all
#define leonos_tar_extract_all_with_progress reliefos_tar_extract_all_with_progress
#define leonos_tar_extract_file reliefos_tar_extract_file
#define leonos_tar_finalize reliefos_tar_finalize
#define leonos_tar_list reliefos_tar_list
#define leonos_tar_pack_dir reliefos_tar_pack_dir
#define leonos_tar_pack_dir_append reliefos_tar_pack_dir_append
#define leonos_tar_pack_file reliefos_tar_pack_file
#define leonos_tar_pack_file_append reliefos_tar_pack_file_append
#define leonos_tar_progress_fn reliefos_tar_progress_fn
#endif /* LEONOS_TAR_H */
