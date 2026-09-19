/*
 * Atomic, change-detecting file publication for generated build outputs.
 */
#ifndef LEONOS_HOST_COMMON_IO_H
#define LEONOS_HOST_COMMON_IO_H

#include <stddef.h>

#include "tools/host/common/buffer.h"

/**
 * @brief Read a whole file into `out`, replacing whatever it held.
 *
 * Sized by the read loop rather than stat(), so a file that grows while it is
 * being read is still consumed completely, and a short read is an error rather
 * than a silently truncated buffer.
 *
 * @param path File to read; `-` means standard input.
 * @param out Buffer owned by the caller; its previous contents are released
 *            before the read starts, and on failure it is left empty.
 * @return 0 on success, -1 on failure with errno set.
 */
int read_file_all(const char *path, struct byte_buffer *out);

/**
 * @brief Write `data` to `path` only when the contents would actually change.
 *
 * The new bytes go to a uniquely named temporary in the destination directory
 * and are published with rename(), so a concurrent reader never observes a
 * partial file and a failure never destroys the previous contents. Identical
 * contents leave the existing file, including its mtime, alone: that is what
 * makes downstream Make targets stop rebuilding.
 *
 * @param path Destination file; its parent directory must already exist.
 * @param data Bytes to publish. May be NULL only when `size` is 0.
 * @param size Byte count; must not exceed the addressable range.
 * @param mode Permission bits applied to the published file (0777 masked).
 * @return 0 on success, -1 on failure with errno set and the original file
 *         preserved.
 */
int write_file_if_changed(const char *path, const void *data, size_t size,
              unsigned int mode);

#endif /* LEONOS_HOST_COMMON_IO_H */
