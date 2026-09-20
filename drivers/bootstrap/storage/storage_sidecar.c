/*
 * LeonOS FAT/exFAT POSIX metadata sidecar.
 *
 * FAT32 and exFAT directory entries carry no ownership fields, so uid/gid/mode
 * for those volumes live in one hidden `LEONACL.SYS` file per directory.  This
 * module owns that representation, including the version-1 access-control
 * records written by older releases, and is the only code that reads or writes
 * those files.  `fs_permissions_*` in kernel/ntclks/permissions.c keeps
 * enforcement; this module only stores and recalls what was stored.
 *
 * On-disk layout, little-endian throughout:
 *   offset 0   magic 'LCAL'
 *   offset 4   format version (1 or 2; both are read, 2 is written)
 *   offset 8   record count
 *   offset 12  checksum over bytes [16, length)
 *   offset 16  sequence of type/length/value records
 * A record is a 4-byte TLV head, a 12-byte version-1 or 20-byte version-2
 * prefix, the name bytes, and `ace_count` 12-byte access-control entries.
 * Version 2 is the only form this module writes; version 1 is still read, and
 * an unrelated version-1 record keeps its form across a rewrite.
 *
 * Concurrency: the module keeps no static or global state.  Every call works on
 * page buffers it allocates and releases itself, so two CPUs may query or
 * rewrite different directories without a lock here.  A read-modify-write of
 * one directory's records is not serialized against another CPU's write of the
 * same directory; that window predates this module and is unchanged.  Storage
 * I/O keeps whatever locking the storage subsystem itself applies.
 */
#include <ntclks/mm.h>
#include <ntclks/storage.h>
#include <ntclks/syscall.h>
#include <leonos/layout.h>

#define SIDECAR_FILE_NAME "LEONACL.SYS"
#define SIDECAR_MAGIC 0x4c43414cU
#define SIDECAR_FORMAT_V1 1U
#define SIDECAR_DISK_VERSION 2U
#define SIDECAR_MAX_BYTES 8192U
#define SIDECAR_PAGE_BYTES 4096u
#define SIDECAR_MAX_PAGES (SIDECAR_MAX_BYTES / SIDECAR_PAGE_BYTES)
#define SIDECAR_MAX_RECORDS 64U
#define SIDECAR_HEADER_BYTES 16U
#define SIDECAR_TLV_BYTES 4U
#define SIDECAR_RECORD_PREFIX_BYTES 12U
#define SIDECAR_POSIX_PREFIX_BYTES 20U
#define SIDECAR_ACE_BYTES 12U
#define SIDECAR_TLV_RECORD 1U
#define SIDECAR_TLV_POSIX 2U
/* Version-1 records marked a denying entry with this bit; a denied entry
 * carries no permissions at all once read, which is how it stays denied. */
#define SIDECAR_ACE_LEGACY_DENY 0x00000001U

/** @brief One directory member's metadata between reads and writes. */
struct sidecar_record {
    char name[LEONOS_FS_NAME_LEN];
    uint32_t owner_uid;
    uint32_t flags;
    uint32_t ace_count;
    uint32_t has_mode;
    uint32_t mode;
    uint32_t gid;
    struct leonos_fs_acl_ace aces[LEONOS_FS_ACL_MAX_ACE];
};

/** @brief How a queued rewrite touches the record named by the caller. */
enum sidecar_edit {
    SIDECAR_EDIT_SET,
    SIDECAR_EDIT_REMOVE,
    SIDECAR_EDIT_RENAME
};

/** @brief Read a little-endian 16-bit value from a validated image. */
static uint16_t sidecar_get_u16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

/** @brief Read a little-endian 32-bit value from a validated image. */
static uint32_t sidecar_get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

/** @brief Store a little-endian 16-bit value. */
static void sidecar_put_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
}

/** @brief Store a little-endian 32-bit value. */
static void sidecar_put_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
}

/** @brief FNV-1a checksum over the record area, excluding the header itself. */
static uint32_t sidecar_checksum(const uint8_t *bytes, uint32_t length)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = SIDECAR_HEADER_BYTES; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/** @brief True when two sidecar names denote the same directory member. */
static int sidecar_names_match(const char *a, const char *b)
{
    return storage_text_eq_ci(a, b);
}

/** @brief True when `text` begins with `prefix`. */
static int sidecar_text_starts_with(const char *text, const char *prefix)
{
    uint32_t i = 0;
    if (!text || !prefix) {
        return 0;
    }
    while (prefix[i]) {
        if (text[i] != prefix[i]) {
            return 0;
        }
        ++i;
    }
    return 1;
}

/** @brief True when `path` is `base` itself or below it at a '/' boundary. */
static int sidecar_path_under(const char *path, const char *base)
{
    uint32_t n = 0;
    if (!path || !base || !base[0]) {
        return 0;
    }
    if (storage_text_eq(path, base)) {
        return 1;
    }
    while (base[n]) {
        ++n;
    }
    return sidecar_text_starts_with(path, base) && path[n] == '/';
}

/**
 * @brief True for locations every account may read and execute.
 * @param path Absolute path being classified.
 *
 * /var/lib/leonos and /root are deliberately absent: accounts, licence state
 * and per-user data must not become world-readable through a default.  /home
 * itself is readable, while a user's own directory is not.
 */
static int sidecar_path_is_system_tree(const char *path)
{
    static const char *const roots[] = {
        "/boot", "/install", "/run", "/lib", "/etc", "/usr", "/opt",
        "/bin", "/sbin", "/dev", "/media", "/mnt"
    };
    if (storage_text_eq(path, "/") || storage_text_eq(path, "/home")) {
        return 1;
    }
    for (uint32_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        if (sidecar_path_under(path, roots[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Split an absolute path into its directory and final component.
 * @param path Absolute path; ':' is not accepted in a pathname.
 * @param parent Output directory, absolute; "/" for a top-level name.
 * @param parent_cap Bytes available at `parent`.
 * @param name Output component; "." for the root directory itself, so the root
 *             keeps its own record in the sidecar of '/'.
 * @param name_cap Bytes available at `name`.
 * @return Zero, -EINVAL for a relative, empty or over-long component, or
 *         -ENAMETOOLONG when a component does not fit its output.
 */
static int sidecar_parent_name(const char *path, char *parent, uint32_t parent_cap,
                               char *name, uint32_t name_cap)
{
    uint32_t slash = 0;
    uint32_t pos = 0;
    if (!path || path[0] != '/' || !parent || !name || parent_cap < 2 || name_cap == 0) {
        return -LEONOS_EINVAL;
    }
    for (uint32_t i = 0; path[i]; ++i) {
        if (path[i] == ':') {
            return -LEONOS_EINVAL;
        }
    }
    if (!path[1]) {
        if (parent_cap < 2 || name_cap < 2) {
            return -LEONOS_ENAMETOOLONG;
        }
        parent[0] = '/';
        parent[1] = 0;
        name[0] = '.';
        name[1] = 0;
        return 0;
    }
    for (uint32_t i = 1; path[i]; ++i) {
        if (path[i] == '/') {
            slash = i;
        }
    }
    if (slash == 0) {
        parent[0] = '/';
        parent[1] = 0;
    } else {
        if (slash + 1u > parent_cap) {
            return -LEONOS_ENAMETOOLONG;
        }
        for (uint32_t i = 0; i < slash; ++i) {
            parent[i] = path[i];
        }
        parent[slash] = 0;
    }
    for (uint32_t i = slash + 1u; path[i]; ++i) {
        if (pos + 1u >= name_cap) {
            return -LEONOS_ENAMETOOLONG;
        }
        name[pos++] = path[i];
    }
    name[pos] = 0;
    return name[0] ? 0 : -LEONOS_EINVAL;
}

/**
 * @brief Build the sidecar file path belonging to a directory.
 * @param directory Absolute directory path.
 * @param out Output buffer.
 * @param cap Bytes available at `out`.
 * @return Zero or -ENAMETOOLONG; a truncated path is never silently queried.
 */
static int sidecar_metadata_path(const char *directory, char *out, uint32_t cap)
{
    uint32_t pos = 0;
    const char *suffix = SIDECAR_FILE_NAME;
    if (!directory || !out || cap == 0) {
        return -LEONOS_EINVAL;
    }
    while (directory[pos] && pos + 1u < cap) {
        out[pos] = directory[pos];
        ++pos;
    }
    out[pos] = 0;
    if (directory[pos]) {
        return -LEONOS_ENAMETOOLONG;
    }
    if (pos > 1u) {
        if (pos + 1u >= cap) {
            return -LEONOS_ENAMETOOLONG;
        }
        out[pos++] = '/';
        out[pos] = 0;
    }
    while (*suffix) {
        if (pos + 1u >= cap) {
            return -LEONOS_ENAMETOOLONG;
        }
        out[pos++] = *suffix++;
        out[pos] = 0;
    }
    return 0;
}

/** @brief Page count a buffer of `length` bytes was allocated from. */
static uint32_t sidecar_pages(uint32_t length)
{
    return (uint32_t)((((length ? length : 1u) + SIDECAR_PAGE_BYTES - 1u) /
                       SIDECAR_PAGE_BYTES));
}

/**
 * @brief True when the volume owning `path` records metadata in a sidecar file.
 * @param path Absolute path whose mounted volume is in question.
 * @return Nonzero for FAT32 and exFAT volumes, which have no inode fields.
 *
 * ext2, tmpfs, proc-like and device nodes keep ownership elsewhere; probing
 * them for a sidecar would create a second authority over the same fact and
 * cost a directory scan on every unlink and rename.
 */
static int sidecar_volume_owns(const char *path)
{
    uint32_t volume_id = 0;
    if (storage_path_volume_id(path, &volume_id) < 0) {
        return 0;
    }
    if (volume_id >= STORAGE_MAX_VOLUMES || !g_volumes[volume_id].ready) {
        return 0;
    }
    return g_volumes[volume_id].filesystem == STORAGE_FILESYSTEM_FAT32 ||
           g_volumes[volume_id].filesystem == STORAGE_FILESYSTEM_EXFAT;
}

/**
 * @brief Read a directory's sidecar image into freshly allocated pages.
 * @param directory Absolute directory path owning the sidecar.
 * @param out_data Output image; NULL when the file is absent, which is a valid
 *                 "no explicit metadata" state rather than an error.
 * @param out_len Output; bytes at `*out_data`.
 * @return Zero or the storage error. Damaged content is reported by
 *         sidecar_is_corrupt(), not here, so callers can tell them apart.
 */
static int sidecar_load(const char *directory, const uint8_t **out_data,
                        uint32_t *out_len)
{
    char path[LEONOS_FS_PATH_LEN];
    const void *data = 0;
    size_t length = 0;
    int ret = sidecar_metadata_path(directory, path, sizeof(path));
    if (ret < 0) {
        return ret;
    }
    ret = storage_read_file(path, &data, &length);
    if (ret == -LEONOS_ENOENT) {
        *out_data = 0;
        *out_len = 0;
        return 0;
    }
    if (ret < 0) {
        return ret;
    }
    if (!data || length > SIDECAR_MAX_BYTES) {
        if (data) {
            mm_free_pages((uint64_t)(uintptr_t)data,
                          sidecar_pages((uint32_t)length));
        }
        return -LEONOS_EIO;
    }
    *out_data = (const uint8_t *)data;
    *out_len = (uint32_t)length;
    return 0;
}

/** @brief Release pages returned by sidecar_load(). Safe on NULL. */
static void sidecar_unload(const uint8_t *data, uint32_t length)
{
    if (data) {
        mm_free_pages((uint64_t)(uintptr_t)data, sidecar_pages(length));
    }
}

/**
 * @brief True when an image fails its magic, version or checksum.
 * @param data Image bytes, or NULL for an absent sidecar.
 * @param length Bytes at `data`.
 * @return Nonzero when the metadata is damaged; damaged metadata is never
 *         repaired by guessing and never reported as an ordinary mode.
 */
static int sidecar_is_corrupt(const uint8_t *data, uint32_t length)
{
    if (!data) {
        return 0;
    }
    if (length < SIDECAR_HEADER_BYTES ||
        sidecar_get_u32(data) != SIDECAR_MAGIC ||
        (sidecar_get_u32(data + 4) != SIDECAR_FORMAT_V1 &&
         sidecar_get_u32(data + 4) != SIDECAR_DISK_VERSION) ||
        sidecar_get_u32(data + 12) != sidecar_checksum(data, length)) {
        return 1;
    }
    return 0;
}

/**
 * @brief Decode the next record of an already validated image.
 * @param data Image bytes; must not be NULL.
 * @param length Bytes at `data`.
 * @param cursor Scan offset, advanced past this record on success.
 * @param out_record Output decoded record.
 * @return 1 for a decoded record, 0 at end of image, -EIO for a malformed one.
 *         Unknown TLV types are skipped so a future record kind stays readable.
 */
static int sidecar_next_record(const uint8_t *data, uint32_t length, uint32_t *cursor,
                               struct sidecar_record *out_record)
{
    for (;;) {
        uint32_t pos = *cursor;
        uint16_t type;
        uint16_t payload;
        uint32_t end;
        const uint8_t *body;
        uint16_t name_length;
        uint16_t ace_count;
        uint32_t prefix;
        uint32_t needed;
        if (pos + SIDECAR_TLV_BYTES > length) {
            return 0;
        }
        type = sidecar_get_u16(data + pos);
        payload = sidecar_get_u16(data + pos + 2u);
        end = pos + SIDECAR_TLV_BYTES + (uint32_t)payload;
        if (payload < SIDECAR_RECORD_PREFIX_BYTES || end > length) {
            return -LEONOS_EIO;
        }
        *cursor = end;
        if (type != SIDECAR_TLV_RECORD && type != SIDECAR_TLV_POSIX) {
            continue;
        }
        body = data + pos + SIDECAR_TLV_BYTES;
        name_length = sidecar_get_u16(body);
        ace_count = sidecar_get_u16(body + 2u);
        prefix = type == SIDECAR_TLV_POSIX ? SIDECAR_POSIX_PREFIX_BYTES
                                           : SIDECAR_RECORD_PREFIX_BYTES;
        needed = prefix + (uint32_t)name_length + (uint32_t)ace_count * SIDECAR_ACE_BYTES;
        if (name_length == 0 || name_length >= LEONOS_FS_NAME_LEN ||
            ace_count > LEONOS_FS_ACL_MAX_ACE || needed > (uint32_t)payload) {
            return -LEONOS_EIO;
        }
        storage_memzero(out_record, sizeof(*out_record));
        out_record->owner_uid = sidecar_get_u32(body + 4u);
        out_record->flags = sidecar_get_u32(body + 8u);
        out_record->ace_count = ace_count;
        out_record->has_mode = type == SIDECAR_TLV_POSIX;
        if (out_record->has_mode) {
            out_record->gid = sidecar_get_u32(body + 12u);
            out_record->mode = sidecar_get_u32(body + 16u) & 0177777u;
        }
        for (uint32_t i = 0; i < name_length; ++i) {
            out_record->name[i] = (char)body[prefix + i];
        }
        out_record->name[name_length] = 0;
        for (uint32_t i = 0; i < ace_count; ++i) {
            const uint8_t *ace = body + prefix + name_length + i * SIDECAR_ACE_BYTES;
            uint32_t legacy_flags = sidecar_get_u32(ace + 4u);
            out_record->aces[i].principal = sidecar_get_u32(ace);
            out_record->aces[i].flags = 0;
            out_record->aces[i].permissions =
                (legacy_flags & SIDECAR_ACE_LEGACY_DENY)
                    ? 0
                    : sidecar_get_u32(ace + 8u);
            out_record->aces[i].reserved = 0;
        }
        return 1;
    }
}

/**
 * @brief Append one record to the image being rebuilt.
 * @param out Image buffer whose 16-byte header is already reserved.
 * @param capacity Bytes available at `out`.
 * @param pos Write offset, advanced only on success.
 * @param count Records written so far, incremented only on success.
 * @param record Record to serialize; ACE flags and the legacy deny bit are
 *               normalized away, exactly as an older write normalized them.
 * @return Zero, -EINVAL for an unsavable record, -ENOSPC at the record limit,
 *         or -E2BIG when the image would exceed capacity.
 */
static int sidecar_emit_record(uint8_t *out, uint32_t capacity, uint32_t *pos,
                               uint32_t *count, const struct sidecar_record *record)
{
    uint32_t name_length = (uint32_t)storage_strlen(record->name);
    uint32_t prefix = record->has_mode ? SIDECAR_POSIX_PREFIX_BYTES
                                       : SIDECAR_RECORD_PREFIX_BYTES;
    uint32_t payload = prefix + name_length + record->ace_count * SIDECAR_ACE_BYTES;
    uint8_t *body;
    if (!name_length || name_length >= LEONOS_FS_NAME_LEN ||
        record->ace_count > LEONOS_FS_ACL_MAX_ACE) {
        return -LEONOS_EINVAL;
    }
    if (*count >= SIDECAR_MAX_RECORDS) {
        return -LEONOS_ENOSPC;
    }
    if (*pos + SIDECAR_TLV_BYTES + payload > capacity) {
        return -LEONOS_E2BIG;
    }
    sidecar_put_u16(out + *pos, (uint16_t)(record->has_mode ? SIDECAR_TLV_POSIX
                                                           : SIDECAR_TLV_RECORD));
    sidecar_put_u16(out + *pos + 2u, (uint16_t)payload);
    *pos += SIDECAR_TLV_BYTES;
    body = out + *pos;
    sidecar_put_u16(body, (uint16_t)name_length);
    sidecar_put_u16(body + 2u, (uint16_t)record->ace_count);
    sidecar_put_u32(body + 4u, record->owner_uid);
    sidecar_put_u32(body + 8u, record->flags);
    if (record->has_mode) {
        sidecar_put_u32(body + 12u, record->gid);
        sidecar_put_u32(body + 16u, record->mode);
    }
    *pos += prefix;
    for (uint32_t i = 0; i < name_length; ++i) {
        out[*pos + i] = (uint8_t)record->name[i];
    }
    *pos += name_length;
    for (uint32_t i = 0; i < record->ace_count; ++i) {
        uint8_t *ace = out + *pos + i * SIDECAR_ACE_BYTES;
        sidecar_put_u32(ace, record->aces[i].principal);
        sidecar_put_u32(ace + 4u, 0);
        sidecar_put_u32(ace + 8u, record->aces[i].permissions & LEONOS_FS_PERM_FULL);
    }
    *pos += record->ace_count * SIDECAR_ACE_BYTES;
    ++(*count);
    return 0;
}

/** @brief Map LEONOS_FS_PERM_* bits onto one POSIX rwx triplet. */
static uint32_t sidecar_permissions_to_rwx(uint32_t bits)
{
    return ((bits & LEONOS_FS_PERM_READ) << 2) |
           (bits & LEONOS_FS_PERM_WRITE) |
           ((bits & LEONOS_FS_PERM_EXEC) >> 2);
}

/**
 * @brief Recall POSIX metadata stored as a version-1 access-control record.
 * @param record Version-1 record holding only access-control entries.
 * @param out Output metadata; `gid` repeats the recorded owner, as it did when
 *            these records were first written.
 */
static void sidecar_record_permissions(const struct sidecar_record *record,
                                       struct leonos_permissions *out)
{
    uint32_t owner_bits = 0;
    uint32_t other_bits = 0;
    uint32_t other;
    for (uint32_t i = 0; i < record->ace_count; ++i) {
        const struct leonos_fs_acl_ace *ace = &record->aces[i];
        if (ace->principal == LEONOS_FS_ACL_PRINCIPAL_OWNER ||
            (!record->owner_uid && ace->principal == LEONOS_FS_ACL_PRINCIPAL_SYSTEM)) {
            owner_bits |= ace->permissions;
        }
        if (ace->principal == LEONOS_FS_ACL_PRINCIPAL_USERS ||
            ace->principal == LEONOS_FS_ACL_PRINCIPAL_EVERYONE) {
            other_bits |= ace->permissions;
        }
    }
    other = sidecar_permissions_to_rwx(other_bits);
    out->mode = (sidecar_permissions_to_rwx(owner_bits | other_bits) << 6) |
                (other << 3) | other;
    out->uid = record->owner_uid;
    out->gid = record->owner_uid;
}

/**
 * @brief Produce metadata for a sidecar member that has no explicit record.
 * @param path Absolute path being queried.
 * @param out Output metadata; root-owned, with the group copied from the owner
 *            because a sidecar volume has no separate group authority.
 */
static void sidecar_default_permissions(const char *path, struct leonos_permissions *out)
{
    uint32_t other_bits = 0;
    uint32_t other;
    if (storage_text_eq(path, "/etc/shadow") || storage_text_eq(path, "/etc/gshadow")) {
        out->mode = 0600;
        out->uid = 0;
        out->gid = 0;
        return;
    }
    if (storage_text_eq(path, "/tmp") || sidecar_path_under(path, "/tmp") ||
        storage_text_eq(path, "/var/tmp") || sidecar_path_under(path, "/var/tmp")) {
        other_bits = LEONOS_FS_PERM_FULL;
    } else if (storage_text_eq(path, LEONOS_PATH_DISPLAY_CONF) ||
               storage_text_eq(path, LEONOS_PATH_LOCALE_CONF) ||
               storage_text_eq(path, LEONOS_PATH_OOBE_DONE)) {
        other_bits = LEONOS_FS_PERM_READ | LEONOS_FS_PERM_WRITE;
    } else if (sidecar_path_is_system_tree(path)) {
        other_bits = LEONOS_FS_PERM_READ | LEONOS_FS_PERM_EXEC;
    }
    /* The owner triplet is always rwx: the synthetic owner is the system
     * principal, which holds every permission bit. */
    other = sidecar_permissions_to_rwx(other_bits);
    out->mode = (7u << 6) | (other << 3) | other;
    if (storage_text_eq(path, "/tmp") || storage_text_eq(path, "/var/tmp")) {
        out->mode = 01777;
    }
    out->uid = 0;
    out->gid = 0;
}

/**
 * @brief Read one sidecar member's metadata, or its default when unstored.
 * @param path Absolute path to query.
 * @param out Output metadata.
 * @return Zero, -EINVAL for a malformed path, or -EIO when the directory's
 *         sidecar is damaged (a default is never substituted for it).
 */
static int sidecar_fetch(const char *path, struct leonos_permissions *out)
{
    char parent[LEONOS_FS_PATH_LEN];
    char name[LEONOS_FS_NAME_LEN];
    const uint8_t *image = 0;
    uint32_t length = 0;
    uint32_t cursor = SIDECAR_HEADER_BYTES;
    struct sidecar_record record;
    int ret = sidecar_parent_name(path, parent, sizeof(parent), name, sizeof(name));
    if (ret < 0) {
        return ret;
    }
    ret = sidecar_load(parent, &image, &length);
    if (ret < 0) {
        return ret;
    }
    if (sidecar_is_corrupt(image, length)) {
        sidecar_unload(image, length);
        return -LEONOS_EIO;
    }
    while (image) {
        ret = sidecar_next_record(image, length, &cursor, &record);
        if (ret < 0) {
            sidecar_unload(image, length);
            return -LEONOS_EIO;
        }
        if (ret == 0) {
            break;
        }
        if (sidecar_names_match(record.name, name)) {
            sidecar_unload(image, length);
            if (record.has_mode) {
                out->mode = record.mode;
                out->uid = record.owner_uid;
                out->gid = record.gid;
            } else {
                sidecar_record_permissions(&record, out);
            }
            return 0;
        }
    }
    sidecar_unload(image, length);
    sidecar_default_permissions(path, out);
    return 0;
}

/**
 * @brief Apply one edit to a directory's sidecar records and store the result.
 * @param directory Absolute directory path owning the sidecar.
 * @param name Record the edit targets, matched case-insensitively.
 * @param edit How to touch the targeted record.
 * @param value Metadata for SIDECAR_EDIT_SET; ignored otherwise.
 * @param new_name Final component for SIDECAR_EDIT_RENAME; ignored otherwise.
 * @return Zero or a negative errno, and zero without any write when the edit
 *         has nothing to change. A damaged sidecar is left untouched by REMOVE
 *         and RENAME, because repairing it would need the information that is
 *         corrupt; SET reports -EIO instead of overwriting it.
 *
 * Records unrelated to the edit are decoded and re-encoded rather than copied,
 * so a version-1 record's denial bits normalize the same way an older write
 * normalized them.
 */
static int sidecar_edit(const char *directory, const char *name, enum sidecar_edit edit,
                        const struct leonos_permissions *value, const char *new_name)
{
    const uint8_t *image = 0;
    uint32_t length = 0;
    uint32_t cursor;
    uint32_t position = SIDECAR_HEADER_BYTES;
    uint32_t count = 0;
    uint64_t phys;
    uint8_t *updated;
    char path[LEONOS_FS_PATH_LEN];
    struct sidecar_record record;
    struct sidecar_record candidate;
    int replaced = 0;
    int changed = 0;
    int corrupt = 0;
    int ret;

    if (edit == SIDECAR_EDIT_SET && !value) {
        return -LEONOS_EINVAL;
    }
    if (edit == SIDECAR_EDIT_RENAME && (!new_name || !new_name[0])) {
        return -LEONOS_EINVAL;
    }
    if (edit == SIDECAR_EDIT_RENAME && sidecar_names_match(name, new_name)) {
        return 0;
    }
    ret = sidecar_load(directory, &image, &length);
    if (ret < 0) {
        return ret;
    }
    if (sidecar_is_corrupt(image, length)) {
        sidecar_unload(image, length);
        return edit == SIDECAR_EDIT_SET ? -LEONOS_EIO : 0;
    }
    if (!image && edit != SIDECAR_EDIT_SET) {
        /* Nothing is tracked in this directory, so there is nothing to drop
         * or re-key, and creating a file would be a new on-disk fact. */
        return 0;
    }
    if (edit == SIDECAR_EDIT_SET) {
        candidate = (struct sidecar_record){0};
        storage_copy_text(candidate.name, sizeof(candidate.name), name);
        candidate.owner_uid = value->uid;
        candidate.gid = value->gid;
        candidate.mode = value->mode & 0177777u;
        candidate.has_mode = 1;
    }
    phys = mm_alloc_pages(SIDECAR_MAX_PAGES);
    if (!phys) {
        sidecar_unload(image, length);
        return -LEONOS_ENOMEM;
    }
    updated = (uint8_t *)(uintptr_t)phys;
    storage_memzero(updated, SIDECAR_MAX_BYTES);
    sidecar_put_u32(updated, SIDECAR_MAGIC);
    sidecar_put_u32(updated + 4u, SIDECAR_DISK_VERSION);
    cursor = SIDECAR_HEADER_BYTES;
    ret = 0;
    while (image) {
        const struct sidecar_record *emit = &record;
        int step = sidecar_next_record(image, length, &cursor, &record);
        if (step < 0) {
            corrupt = 1;
            break;
        }
        if (step == 0) {
            break;
        }
        if (edit == SIDECAR_EDIT_RENAME && sidecar_names_match(record.name, new_name)) {
            /* The member this record describes is being overwritten, so its
             * metadata must not survive under the new name. */
            changed = 1;
            continue;
        }
        if (sidecar_names_match(record.name, name)) {
            if (edit == SIDECAR_EDIT_REMOVE) {
                changed = 1;
                continue;
            }
            if (!replaced) {
                replaced = 1;
                changed = 1;
                if (edit == SIDECAR_EDIT_SET) {
                    emit = &candidate;
                } else {
                    storage_copy_text(record.name, sizeof(record.name), new_name);
                }
            }
        }
        ret = sidecar_emit_record(updated, SIDECAR_MAX_BYTES, &position, &count, emit);
        if (ret < 0) {
            break;
        }
    }
    if (corrupt) {
        mm_free_pages(phys, SIDECAR_MAX_PAGES);
        sidecar_unload(image, length);
        return edit == SIDECAR_EDIT_SET ? -LEONOS_EIO : 0;
    }
    if (ret == 0 && edit == SIDECAR_EDIT_SET && !replaced) {
        ret = sidecar_emit_record(updated, SIDECAR_MAX_BYTES, &position, &count,
                                  &candidate);
        if (ret == 0) {
            changed = 1;
        }
    }
    if (ret == 0 && !changed) {
        mm_free_pages(phys, SIDECAR_MAX_PAGES);
        sidecar_unload(image, length);
        return 0;
    }
    if (ret == 0) {
        sidecar_put_u32(updated + 8u, count);
        sidecar_put_u32(updated + 12u, sidecar_checksum(updated, position));
        ret = sidecar_metadata_path(directory, path, sizeof(path));
        if (ret == 0) {
            ret = storage_write_file(path, updated, position);
        }
    }
    mm_free_pages(phys, SIDECAR_MAX_PAGES);
    sidecar_unload(image, length);
    return ret;
}

int storage_sidecar_permissions(const char *path, struct leonos_permissions *value,
                                bool write)
{
    char parent[LEONOS_FS_PATH_LEN];
    char name[LEONOS_FS_NAME_LEN];
    int ret;
    if (!path || !value) {
        return -LEONOS_EINVAL;
    }
    if (!write) {
        return sidecar_fetch(path, value);
    }
    ret = sidecar_parent_name(path, parent, sizeof(parent), name, sizeof(name));
    if (ret < 0) {
        return ret;
    }
    return sidecar_edit(parent, name, SIDECAR_EDIT_SET, value, 0);
}

int storage_sidecar_note_deleted(const char *path)
{
    char parent[LEONOS_FS_PATH_LEN];
    char name[LEONOS_FS_NAME_LEN];
    int ret;
    if (!path || !sidecar_volume_owns(path)) {
        return 0;
    }
    ret = sidecar_parent_name(path, parent, sizeof(parent), name, sizeof(name));
    if (ret < 0) {
        return ret;
    }
    return sidecar_edit(parent, name, SIDECAR_EDIT_REMOVE, 0, 0);
}

int storage_sidecar_note_renamed(const char *path, const char *new_path)
{
    char old_parent[LEONOS_FS_PATH_LEN];
    char new_parent[LEONOS_FS_PATH_LEN];
    char old_name[LEONOS_FS_NAME_LEN];
    char new_name[LEONOS_FS_NAME_LEN];
    int ret;
    if (!path || !new_path || !sidecar_volume_owns(new_path)) {
        return 0;
    }
    ret = sidecar_parent_name(path, old_parent, sizeof(old_parent),
                              old_name, sizeof(old_name));
    if (ret < 0) {
        return ret;
    }
    ret = sidecar_parent_name(new_path, new_parent, sizeof(new_parent),
                              new_name, sizeof(new_name));
    if (ret < 0) {
        return ret;
    }
    if (!storage_text_eq_ci(old_parent, new_parent)) {
        /* Moving between directories leaves the record with the old
         * directory, matching the behavior this replaces. */
        return 0;
    }
    return sidecar_edit(old_parent, old_name, SIDECAR_EDIT_RENAME, 0, new_name);
}
