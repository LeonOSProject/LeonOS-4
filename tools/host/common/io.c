/*
 * Atomic, change-detecting file publication.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"

#define TEMP_SUFFIX      ".tmpXXXXXX"
#define TEMP_SUFFIX_SIZE (sizeof(TEMP_SUFFIX) - 1u)

/**
 * @brief Read an existing file completely.
 * @param path File to read; absence is not an error.
 * @param out Buffer the caller must destroy; filled with the file bytes.
 * @return 0 when the file was read (possibly zero bytes), 1 when it does not
 *         exist, -1 with errno set on any other failure.
 */
static int read_existing(const char *path, struct byte_buffer *out)
{
    int descriptor = open(path, O_RDONLY | O_CLOEXEC);

    if (descriptor < 0) {
        if (errno == ENOENT) {
            return 1;
        }
        return -1;
    }

    for (;;) {
        size_t chunk = 8192u;
        ssize_t got;
        size_t used = out->len;

        if (buffer_reserve(out, used + chunk) != 0) {
            close(descriptor);
            return -1;
        }
        got = read(descriptor, out->data + used, chunk);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(descriptor);
            return -1;
        }
        if (got == 0) {
            break;
        }
        out->len = used + (size_t)got;
    }

    if (close(descriptor) != 0) {
        /* The bytes were already complete, but a close failure can mean the
         * reader hit a limit; report it rather than claiming success. */
        return -1;
    }
    return 0;
}

/**
 * @brief Write every byte to `descriptor`, retrying short writes and EINTR.
 * @return 0 on success, -1 with errno set.
 */
static int write_all(int descriptor, const unsigned char *data, size_t size)
{
    size_t done = 0;

    while (done < size) {
        ssize_t written = write(descriptor, data + done, size - done);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (written == 0) {
            errno = ENOSPC;
            return -1;
        }
        done += (size_t)written;
    }
    return 0;
}

/* Creates the temporary in the destination directory by appending the template
 * to the target path, so no path splitting (and no truncation limit) is needed. */
static int create_temporary(const char *path, struct byte_buffer *template_buffer)
{
    size_t base_length = strlen(path);
    char *name;
    int descriptor;

    if (buffer_reserve(template_buffer, base_length + TEMP_SUFFIX_SIZE + 1u) != 0) {
        return -1;
    }
    name = (char *)template_buffer->data;
    memcpy(name, path, base_length);
    memcpy(name + base_length, TEMP_SUFFIX, TEMP_SUFFIX_SIZE + 1u);
    template_buffer->len = base_length + TEMP_SUFFIX_SIZE;

    descriptor = mkstemp(name);
    if (descriptor < 0) {
        return -1;
    }
    /* mkstemp() has now rewritten the XXXXXX placeholder; keep len accurate. */
    template_buffer->len = strlen(name);
    return descriptor;
}

int write_file_if_changed(const char *path, const void *data, size_t size,
                          unsigned int mode)
{
    struct byte_buffer existing = { NULL, 0, 0 };
    struct byte_buffer temp_name = { NULL, 0, 0 };
    int comparison;
    int descriptor;
    int result = -1;

    if (path == NULL || (data == NULL && size != 0)) {
        errno = EINVAL;
        return -1;
    }

    comparison = read_existing(path, &existing);
    if (comparison < 0) {
        int saved = errno;

        buffer_destroy(&existing);
        errno = saved;
        return -1;
    }
    if (comparison == 0 && existing.len == size &&
        (size == 0 || memcmp(existing.data, data, size) == 0)) {
        buffer_destroy(&existing);
        return 0;
    }
    buffer_destroy(&existing);

    descriptor = create_temporary(path, &temp_name);
    if (descriptor >= 0) {
        const char *temporary = (const char *)temp_name.data;
        int staged = 0;

        if (fchmod(descriptor, (mode_t)(mode & 07777u)) == 0 &&
            write_all(descriptor, (const unsigned char *)data, size) == 0) {
            staged = 1;
        }
        /* close() can still fail after a successful write (deferred ENOSPC), so
         * its result decides whether the bytes are really on disk. */
        if (close(descriptor) != 0) {
            staged = 0;
        }
        if (staged && rename(temporary, path) == 0) {
            result = 0;
        }
        if (result != 0) {
            int saved = errno;

            if (saved == 0) {
                saved = EIO;
            }
            (void)unlink(temporary);
            errno = saved;
        }
    }

    buffer_destroy(&temp_name);
    return result;
}
