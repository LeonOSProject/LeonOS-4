/*
 * ReliefOS VFS adapter for SQLite.
 *
 * SQLite is built with SQLITE_OS_OTHER, so this file is the complete file
 * and clock boundary.  ReliefOS currently exposes no cross-process advisory
 * locking primitive to userland; locking callbacks therefore serialize only
 * within SQLite's single-threaded connection and WAL is disabled by the
 * build.  Do not use this VFS for concurrent writers until that contract is
 * extended.
 */

#include "sqlite3.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct ReliefosStatRaw {
    uint32_t type;
    uint32_t reserved;
    uint64_t size;
};

extern int reliefos_stat_raw_call(const char *path, struct ReliefosStatRaw *status)
    __asm__("stat");
extern int reliefos_fstat_raw_call(int fd, struct ReliefosStatRaw *status)
    __asm__("fstat");
extern int reliefos_ftruncate_call(int fd, long length) __asm__("ftruncate");
extern int sleep_ms(unsigned long milliseconds);

typedef struct ReliefosFile {
    sqlite3_file base;
    int fd;
    int readonly;
} ReliefosFile;

static int reliefos_close(sqlite3_file *file)
{
    ReliefosFile *handle = (ReliefosFile *)file;
    int result = close(handle->fd);
    handle->fd = -1;
    return result == 0 ? SQLITE_OK : SQLITE_IOERR_CLOSE;
}

static int reliefos_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset)
{
    ReliefosFile *handle = (ReliefosFile *)file;
    long position;
    long received;
    position = lseek(handle->fd, (long)offset, SEEK_SET);
    if (position < 0) {
        return SQLITE_IOERR_SEEK;
    }
    received = read(handle->fd, buffer, (size_t)amount);
    if (received < 0) {
        return SQLITE_IOERR_READ;
    }
    if (received < amount) {
        memset((unsigned char *)buffer + received, 0, (size_t)amount - (size_t)received);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int reliefos_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset)
{
    ReliefosFile *handle = (ReliefosFile *)file;
    long position;
    long written;
    if (handle->readonly) {
        return SQLITE_READONLY;
    }
    position = lseek(handle->fd, (long)offset, SEEK_SET);
    if (position < 0) {
        return SQLITE_IOERR_SEEK;
    }
    written = write(handle->fd, buffer, (size_t)amount);
    return written == amount ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

static int reliefos_truncate(sqlite3_file *file, sqlite3_int64 size)
{
    ReliefosFile *handle = (ReliefosFile *)file;
    if (handle->readonly || reliefos_ftruncate_call(handle->fd, (long)size) != 0) {
        return handle->readonly ? SQLITE_READONLY : SQLITE_IOERR_TRUNCATE;
    }
    return SQLITE_OK;
}

static int reliefos_sync(sqlite3_file *file, int flags)
{
    (void)file;
    (void)flags;
    /* ReliefOS filesystem writes commit metadata as part of the write syscall. */
    return SQLITE_OK;
}

static int reliefos_size(sqlite3_file *file, sqlite3_int64 *size)
{
    struct ReliefosStatRaw status;
    ReliefosFile *handle = (ReliefosFile *)file;
    if (reliefos_fstat_raw_call(handle->fd, &status) != 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *size = (sqlite3_int64)status.size;
    return SQLITE_OK;
}

static int reliefos_lock(sqlite3_file *file, int lock)
{
    (void)file;
    (void)lock;
    return SQLITE_OK;
}

static int reliefos_unlock(sqlite3_file *file, int lock)
{
    (void)file;
    (void)lock;
    return SQLITE_OK;
}

static int reliefos_reserved(sqlite3_file *file, int *reserved)
{
    (void)file;
    *reserved = 0;
    return SQLITE_OK;
}

static int reliefos_file_control(sqlite3_file *file, int operation, void *argument)
{
    (void)file;
    (void)operation;
    (void)argument;
    return SQLITE_NOTFOUND;
}

static int reliefos_sector_size(sqlite3_file *file)
{
    (void)file;
    return 512;
}

static int reliefos_device_characteristics(sqlite3_file *file)
{
    (void)file;
    return 0;
}

static const sqlite3_io_methods reliefos_io = {
    .iVersion = 1,
    .xClose = reliefos_close,
    .xRead = reliefos_read,
    .xWrite = reliefos_write,
    .xTruncate = reliefos_truncate,
    .xSync = reliefos_sync,
    .xFileSize = reliefos_size,
    .xLock = reliefos_lock,
    .xUnlock = reliefos_unlock,
    .xCheckReservedLock = reliefos_reserved,
    .xFileControl = reliefos_file_control,
    .xSectorSize = reliefos_sector_size,
    .xDeviceCharacteristics = reliefos_device_characteristics,
};

static int reliefos_open(sqlite3_vfs *vfs, const char *path, sqlite3_file *file,
                       int flags, int *out_flags)
{
    ReliefosFile *handle = (ReliefosFile *)file;
    int native_flags;
    int fd;
    (void)vfs;
    memset(handle, 0, sizeof(*handle));
    handle->fd = -1;
    if (flags & SQLITE_OPEN_READWRITE) {
        native_flags = O_RDWR;
        if (flags & SQLITE_OPEN_CREATE) {
            native_flags |= O_CREAT;
        }
    } else {
        native_flags = O_RDONLY;
    }
    fd = open(path, native_flags, 0666);
    if (fd < 0 && (flags & SQLITE_OPEN_CREATE) && (flags & SQLITE_OPEN_READWRITE)) {
        return SQLITE_CANTOPEN;
    }
    if (fd < 0) {
        return SQLITE_CANTOPEN;
    }
    handle->fd = fd;
    handle->readonly = (native_flags & O_ACCMODE) == O_RDONLY;
    handle->base.pMethods = &reliefos_io;
    if (out_flags) {
        *out_flags = flags;
    }
    return SQLITE_OK;
}

static int reliefos_delete(sqlite3_vfs *vfs, const char *path, int sync_dir)
{
    (void)vfs;
    (void)sync_dir;
    return unlink(path) == 0 ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int reliefos_access(sqlite3_vfs *vfs, const char *path, int flags, int *result)
{
    struct ReliefosStatRaw status;
    (void)vfs;
    (void)flags;
    *result = reliefos_stat_raw_call(path, &status) == 0;
    return SQLITE_OK;
}

static int reliefos_full_pathname(sqlite3_vfs *vfs, const char *path, int length, char *output)
{
    (void)vfs;
    if (length <= 0) {
        return SQLITE_CANTOPEN;
    }
    strncpy(output, path, (size_t)length - 1U);
    output[length - 1] = '\0';
    return SQLITE_OK;
}

static int reliefos_randomness(sqlite3_vfs *vfs, int length, char *output)
{
    static uint32_t state = 0x9e3779b9U;
    int index;
    (void)vfs;
    for (index = 0; index < length; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        output[index] = (char)(state >> 24);
    }
    return length;
}

static int reliefos_sleep(sqlite3_vfs *vfs, int microseconds)
{
    (void)vfs;
    sleep_ms((unsigned long)((microseconds + 999) / 1000));
    return microseconds;
}

static int reliefos_current_time(sqlite3_vfs *vfs, double *now)
{
    (void)vfs;
    /* 2440587.5 is the Julian day at Unix epoch. */
    *now = 2440587.5 + ((double)time(NULL) / 86400.0);
    return SQLITE_OK;
}

static sqlite3_vfs reliefos_vfs = {
    .iVersion = 1,
    .szOsFile = sizeof(ReliefosFile),
    .mxPathname = 256,
    /* Public VFS selection name retained for existing applications. */
    .zName = "leonos",
    .xOpen = reliefos_open,
    .xDelete = reliefos_delete,
    .xAccess = reliefos_access,
    .xFullPathname = reliefos_full_pathname,
    .xRandomness = reliefos_randomness,
    .xSleep = reliefos_sleep,
    .xCurrentTime = reliefos_current_time,
};

int sqlite3_os_init(void)
{
    return sqlite3_vfs_register(&reliefos_vfs, 1);
}

int sqlite3_os_end(void)
{
    return sqlite3_vfs_unregister(&reliefos_vfs);
}
