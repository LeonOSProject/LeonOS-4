/*
 * Atomic, change-detecting file publication for generated build outputs.
 */
#ifndef LEONOS_HOST_COMMON_IO_H
#define LEONOS_HOST_COMMON_IO_H

#include <stddef.h>

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
