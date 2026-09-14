#include <ntclks/tmpfs.h>
#include <ntclks/heap.h>
#include <ntclks/mm.h>
#include <ntclks/time.h>
#include <ntclks/page_cache.h>
#include <ntclks/sched.h>
#include <linux/mount.h>

#define TMPFS_PAGE 4096u
#define TMPFS_HASH 64u
struct tmpfs_page {
    struct tmpfs_page *next;
    uint64_t index;
    uint64_t phys;
};
struct tmpfs_entry {
    struct tmpfs_entry *next;
    struct tmpfs_inode *inode;
    uint64_t cookie;
    char name[LEONOS_FS_NAME_LEN];
};
struct tmpfs_inode {
    struct tmpfs_inode *next, *parent;
    struct linux_stat_abi stat;
    struct tmpfs_page *pages[TMPFS_HASH];
    struct tmpfs_entry *entries;
    uint64_t next_cookie, refs;
    char *target;
};
struct tmpfs_super {
    struct tmpfs_inode *inodes[TMPFS_HASH], *root;
    uint64_t max_pages, used_pages, max_inodes, used_inodes, flags;
    uint32_t volume, next_ino;
};

static int64_t tmpfs_now(void)
{
    struct leonos_time_info now;
    return time_wall_clock(&now) == 0 ? (int64_t)now.unix_seconds : 0;
}
static void tmpfs_changed(struct tmpfs_inode *inode, bool data)
{
    inode->stat.ctime_sec = tmpfs_now();
    if (data)
        inode->stat.mtime_sec = inode->stat.ctime_sec;
}
static bool tmpfs_dir(const struct tmpfs_inode *inode)
{
    return (inode->stat.st_mode & LINUX_S_IFMT) == LINUX_S_IFDIR;
}
static struct tmpfs_inode *tmpfs_inode(struct tmpfs_super *fs, uint32_t ino)
{
    for (struct tmpfs_inode *n = fs->inodes[ino % TMPFS_HASH]; n; n = n->next)
        if (n->stat.st_ino == ino)
            return n;
    return NULL;
}
static void tmpfs_node(struct tmpfs_super *fs, struct tmpfs_inode *n, struct storage_node *out)
{
    if (!out)
        return;
    uint32_t mode = n->stat.st_mode & LINUX_S_IFMT;
    *out =
        (struct storage_node){.volume_id = fs->volume,
                              .first_cluster = n->stat.st_ino,
                              .size = n->stat.st_size,
                              .flags = STORAGE_NODE_FLAG_TMPFS | (n == fs->root ? STORAGE_NODE_FLAG_ROOT : 0),
                              .type = mode == LINUX_S_IFDIR    ? LEONOS_FS_TYPE_DIR
                                      : mode == LINUX_S_IFLNK  ? LEONOS_FS_TYPE_SYMLINK
                                      : mode == LINUX_S_IFSOCK ? LEONOS_FS_TYPE_SOCKET
                                                               : LEONOS_FS_TYPE_FILE};
}
static void tmpfs_free_inode(struct tmpfs_super *fs, struct tmpfs_inode *n)
{
    struct storage_node node;
    tmpfs_node(fs, n, &node);
    page_cache_invalidate_node(&node);
    for (unsigned i = 0; i < TMPFS_HASH; ++i) {
        struct tmpfs_page *p = n->pages[i];
        while (p) {
            struct tmpfs_page *next = p->next;
            mm_free_page(p->phys);
            kernel_free(p);
            p = next;
            --fs->used_pages;
        }
    }
    struct tmpfs_entry *e = n->entries;
    while (e) {
        struct tmpfs_entry *next = e->next;
        kernel_free(e);
        e = next;
    }
    struct tmpfs_inode **link = &fs->inodes[n->stat.st_ino % TMPFS_HASH];
    while (*link != n)
        link = &(*link)->next;
    *link = n->next;
    kernel_free(n->target);
    kernel_free(n);
    --fs->used_inodes;
}
void tmpfs_destroy(struct tmpfs_super *fs)
{
    if (!fs)
        return;
    for (unsigned i = 0; i < TMPFS_HASH; ++i)
        while (fs->inodes[i])
            tmpfs_free_inode(fs, fs->inodes[i]);
    kernel_free(fs);
}
static int tmpfs_alloc_inode(struct tmpfs_super *fs, uint32_t mode, struct tmpfs_inode **out)
{
    if ((fs->max_inodes && fs->used_inodes >= fs->max_inodes) || !fs->next_ino)
        return -28;
    struct tmpfs_inode *n = kernel_malloc(sizeof(*n));
    if (!n)
        return -12;
    __builtin_memset(n, 0, sizeof(*n));
    n->stat = (struct linux_stat_abi){.st_ino = fs->next_ino++,
                                      .st_dev = fs->volume + 1,
                                      .st_mode = mode,
                                      .st_nlink = (mode & LINUX_S_IFMT) == LINUX_S_IFDIR ? 2 : 1,
                                      .st_blksize = TMPFS_PAGE,
                                      .atime_sec = tmpfs_now()};
    n->stat.mtime_sec = n->stat.ctime_sec = n->stat.atime_sec;
    if (tmpfs_dir(n))
        n->stat.st_size = 40;
    n->next_cookie = 2;
    unsigned hash = n->stat.st_ino % TMPFS_HASH;
    n->next = fs->inodes[hash];
    fs->inodes[hash] = n;
    ++fs->used_inodes;
    *out = n;
    return 0;
}
static int tmpfs_number(const char *text, uint64_t *out, unsigned base, bool suffix, bool percent)
{
    uint64_t value = 0;
    unsigned digits = 0;
    while (*text >= '0' && *text <= '9') {
        unsigned digit = *text++ - '0';
        if (digit >= base || value > (UINT64_MAX - digit) / base)
            return -22;
        value = value * base + digit;
        ++digits;
    }
    if (!digits)
        return -22;
    if (suffix && *text) {
        unsigned shift = 0;
        switch (*text) {
        case 'k':
        case 'K':
            shift = 10;
            break;
        case 'm':
        case 'M':
            shift = 20;
            break;
        case 'g':
        case 'G':
            shift = 30;
            break;
        case 't':
        case 'T':
            shift = 40;
            break;
        case 'p':
        case 'P':
            shift = 50;
            break;
        case 'e':
        case 'E':
            shift = 60;
            break;
        }
        if (shift) {
            if (value > (UINT64_MAX >> shift))
                return -22;
            value <<= shift;
            ++text;
        }
    }
    if (percent && *text == '%') {
        uint64_t ram = mm_total_memory_kib() * 1024;
        if (value && ram > UINT64_MAX / value)
            return -22;
        value = value * ram / 100;
        ++text;
    }
    if (*text)
        return -22;
    *out = value;
    return 0;
}
int tmpfs_new(uint32_t volume, uint64_t flags, const char *options, uint32_t uid, uint32_t gid,
              struct tmpfs_super **out)
{
    if (!out)
        return -22;
    *out = NULL;
    struct tmpfs_super *fs = kernel_malloc(sizeof(*fs));
    if (!fs)
        return -12;
    __builtin_memset(fs, 0, sizeof(*fs));
    fs->volume = volume;
    fs->next_ino = 1;
    fs->flags = flags;
    fs->max_pages = fs->max_inodes = mm_total_memory_kib() / 8; /* Half RAM, in pages. */
    uint32_t mode = 01777;
    const char *p = options ? options : "";
    int ret = -22;
    while (*p) {
        char option[128];
        unsigned length = 0;
        while (*p && *p != ',') {
            if (length + 1 == sizeof(option))
                goto fail;
            option[length++] = *p++;
        }
        if (*p)
            ++p;
        option[length] = 0;
        if (!length)
            continue;
        char *value = option;
        while (*value && *value != '=')
            ++value;
        if (!*value)
            goto fail;
        *value++ = 0;
        uint64_t number;
        bool is_mode = !__builtin_strcmp(option, "mode");
        bool is_size = !__builtin_strcmp(option, "size");
        bool is_blocks = !__builtin_strcmp(option, "nr_blocks");
        bool is_inodes = !__builtin_strcmp(option, "nr_inodes");
        if (tmpfs_number(value, &number, is_mode ? 8 : 10, is_size || is_blocks || is_inodes, is_size))
            goto fail;
        if (is_size) {
            if (number > UINT64_MAX - 4095)
                goto fail;
            fs->max_pages = (number + 4095) / 4096;
        } else if (is_blocks) {
            if (number > INT64_MAX)
                goto fail;
            fs->max_pages = number;
        } else if (is_inodes) {
            if (number > UINT32_MAX)
                goto fail;
            fs->max_inodes = number;
        } else if (is_mode) {
            if (number > UINT32_MAX)
                goto fail;
            mode = number & 07777;
        } else if (!__builtin_strcmp(option, "uid")) {
            if (number >= UINT32_MAX)
                goto fail;
            uid = number;
        } else if (!__builtin_strcmp(option, "gid")) {
            if (number >= UINT32_MAX)
                goto fail;
            gid = number;
        } else
            goto fail;
    }
    ret = tmpfs_alloc_inode(fs, LINUX_S_IFDIR | mode, &fs->root);
    if (ret)
        goto fail;
    fs->root->parent = fs->root;
    fs->root->stat.st_uid = uid;
    fs->root->stat.st_gid = gid;
    *out = fs;
    return 0;
fail:
    tmpfs_destroy(fs);
    return ret;
}
static struct tmpfs_entry **tmpfs_find(struct tmpfs_inode *dir, const char *name)
{
    struct tmpfs_entry **e = &dir->entries;
    while (*e && __builtin_strcmp((*e)->name, name))
        e = &(*e)->next;
    return e;
}
static int tmpfs_walk(struct tmpfs_super *fs, const char *path, bool parent, struct tmpfs_inode **out,
                      char *last)
{
    if (!fs || !path || *path != '/')
        return -22;
    struct tmpfs_inode *n = fs->root;
    const char *p = path;
    while (*p) {
        while (*p == '/')
            ++p;
        if (!*p)
            break;
        if (!tmpfs_dir(n))
            return -20;
        char name[LEONOS_FS_NAME_LEN];
        unsigned length = 0;
        while (*p && *p != '/') {
            if (length + 1 == sizeof(name))
                return -36;
            name[length++] = *p++;
        }
        name[length] = 0;
        const char *tail = p;
        while (*tail == '/')
            ++tail;
        if (parent && !*tail) {
            if (!__builtin_strcmp(name, ".") || !__builtin_strcmp(name, ".."))
                return -22;
            __builtin_memcpy(last, name, length + 1);
            *out = n;
            return 0;
        }
        if (!__builtin_strcmp(name, "."))
            continue;
        if (!__builtin_strcmp(name, "..")) {
            n = n->parent;
            continue;
        }
        struct tmpfs_entry *e = *tmpfs_find(n, name);
        if (!e)
            return -2;
        n = e->inode;
        /* Global namei resolves links before issuing backend operations. */
        if (*p == '/' && !tmpfs_dir(n))
            return -20;
    }
    if (parent)
        return -16;
    *out = n;
    return 0;
}
int tmpfs_lookup(struct tmpfs_super *fs, const char *path, struct storage_node *out)
{
    struct tmpfs_inode *n;
    int ret = tmpfs_walk(fs, path, false, &n, NULL);
    if (!ret)
        tmpfs_node(fs, n, out);
    return ret;
}
static struct tmpfs_entry *tmpfs_entry_new(struct tmpfs_inode *parent, const char *name,
                                           struct tmpfs_inode *n)
{
    if (parent->next_cookie == UINT64_MAX)
        return NULL;
    struct tmpfs_entry *e = kernel_malloc(sizeof(*e));
    if (!e)
        return NULL;
    *e = (struct tmpfs_entry){.inode = n, .cookie = parent->next_cookie++};
    __builtin_memcpy(e->name, name, __builtin_strlen(name) + 1);
    return e;
}
static void tmpfs_attach(struct tmpfs_inode *parent, struct tmpfs_entry *e)
{
    struct tmpfs_entry **link = &parent->entries;
    while (*link)
        link = &(*link)->next;
    *link = e;
    e->next = NULL;
    parent->stat.st_size += 20;
    if (tmpfs_dir(e->inode)) {
        ++parent->stat.st_nlink;
        e->inode->parent = parent;
    }
    tmpfs_changed(parent, true);
}
int tmpfs_create(struct tmpfs_super *fs, const char *path, uint32_t mode, const char *target,
                 struct storage_node *out)
{
    if (fs->flags & MS_RDONLY)
        return -30;
    struct tmpfs_inode *parent, *n;
    char name[LEONOS_FS_NAME_LEN];
    int ret = tmpfs_walk(fs, path, true, &parent, name);
    if (ret)
        return ret;
    if (!parent->stat.st_nlink)
        return -2;
    if (*tmpfs_find(parent, name))
        return -17;
    uint32_t type = mode & LINUX_S_IFMT;
    if (type != LINUX_S_IFDIR && type != LINUX_S_IFREG && type != LINUX_S_IFLNK && type != LINUX_S_IFSOCK)
        return -95;
    if (type == LINUX_S_IFLNK && (!target || !*target))
        return -2;
    ret = tmpfs_alloc_inode(fs, mode, &n);
    if (ret)
        return ret;
    n->parent = parent;
    if (target) {
        size_t length = __builtin_strlen(target);
        if (length >= LEONOS_FS_PATH_LEN) {
            tmpfs_free_inode(fs, n);
            return -36;
        }
        n->target = kernel_malloc(length + 1);
        if (!n->target) {
            tmpfs_free_inode(fs, n);
            return -12;
        }
        __builtin_memcpy(n->target, target, length + 1);
        n->stat.st_size = length;
    }
    struct tmpfs_entry *e = tmpfs_entry_new(parent, name, n);
    if (!e) {
        tmpfs_free_inode(fs, n);
        return -12;
    }
    tmpfs_attach(parent, e);
    tmpfs_node(fs, n, out);
    return 0;
}
static struct tmpfs_page *tmpfs_page_find(struct tmpfs_inode *n, uint64_t index)
{
    for (struct tmpfs_page *p = n->pages[index % TMPFS_HASH]; p; p = p->next)
        if (p->index == index)
            return p;
    return NULL;
}
static int tmpfs_page_get(struct tmpfs_super *fs, struct tmpfs_inode *n, uint64_t index,
                          struct tmpfs_page **out)
{
    struct tmpfs_page *p = tmpfs_page_find(n, index);
    if (!p) {
        if (fs->max_pages && fs->used_pages >= fs->max_pages)
            return -28;
        p = kernel_malloc(sizeof(*p));
        if (!p)
            return -12;
        p->phys = mm_alloc_page();
        if (!p->phys) {
            kernel_free(p);
            return -12;
        }
        __builtin_memset((void *)(uintptr_t)p->phys, 0, TMPFS_PAGE);
        p->index = index;
        p->next = n->pages[index % TMPFS_HASH];
        n->pages[index % TMPFS_HASH] = p;
        ++fs->used_pages;
        n->stat.st_blocks += TMPFS_PAGE / 512;
    }
    *out = p;
    return 0;
}
int tmpfs_get_page(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, uint64_t *phys)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if (!phys || offset % TMPFS_PAGE || offset >= (uint64_t)n->stat.st_size ||
        (n->stat.st_mode & LINUX_S_IFMT) != LINUX_S_IFREG)
        return -22;
    struct tmpfs_page *p;
    int ret = tmpfs_page_get(fs, n, offset / TMPFS_PAGE, &p);
    if (ret)
        return ret;
    mm_retain_page(p->phys);
    *phys = p->phys;
    return 0;
}
int tmpfs_read(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, void *buffer, uint32_t length,
               uint32_t *done)
{
    uint32_t total = 0;
    if (done)
        *done = 0;
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if ((n->stat.st_mode & LINUX_S_IFMT) != LINUX_S_IFREG)
        return tmpfs_dir(n) ? -21 : -22;
    if (!buffer && length)
        return -22;
    if (offset >= (uint64_t)n->stat.st_size)
        return 0;
    if (length > (uint64_t)n->stat.st_size - offset)
        length = n->stat.st_size - offset;
    while (total < length) {
        uint64_t pos = offset + total;
        unsigned skip = pos % TMPFS_PAGE, take = TMPFS_PAGE - skip;
        if (take > length - total)
            take = length - total;
        struct tmpfs_page *p = tmpfs_page_find(n, pos / TMPFS_PAGE);
        if (p)
            __builtin_memcpy((uint8_t *)buffer + total, (uint8_t *)(uintptr_t)p->phys + skip, take);
        else
            __builtin_memset((uint8_t *)buffer + total, 0, take);
        total += take;
    }
    if (length && !(fs->flags & (MS_NOATIME | MS_RDONLY))) {
        int64_t now = tmpfs_now();
        if (!(fs->flags & MS_RELATIME) || n->stat.atime_sec <= n->stat.mtime_sec ||
            n->stat.atime_sec <= n->stat.ctime_sec || now - n->stat.atime_sec >= 86400)
            n->stat.atime_sec = now;
    }
    if (done)
        *done = total;
    return 0;
}
int tmpfs_write(struct tmpfs_super *fs, uint32_t ino, uint64_t offset, const void *buffer, uint32_t length,
                uint32_t *done)
{
    uint32_t total = 0;
    if (done)
        *done = 0;
    if (fs->flags & MS_RDONLY)
        return -30;
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if ((n->stat.st_mode & LINUX_S_IFMT) != LINUX_S_IFREG)
        return tmpfs_dir(n) ? -21 : -22;
    if (!buffer && length)
        return -22;
    if (offset > INT64_MAX || length > (uint64_t)INT64_MAX - offset)
        return -27;
    int ret = 0;
    while (total < length) {
        uint64_t pos = offset + total, index = pos / TMPFS_PAGE;
        unsigned skip = pos % TMPFS_PAGE, take = TMPFS_PAGE - skip;
        if (take > length - total)
            take = length - total;
        struct tmpfs_page *p;
        ret = tmpfs_page_get(fs, n, index, &p);
        if (ret)
            break;
        __builtin_memcpy((uint8_t *)(uintptr_t)p->phys + skip, (const uint8_t *)buffer + total, take);
        total += take;
    }
    if (total) {
        if (offset + total > (uint64_t)n->stat.st_size)
            n->stat.st_size = offset + total;
        tmpfs_changed(n, true);
    }
    if (done)
        *done = total;
    return ret;
}
int tmpfs_truncate(struct tmpfs_super *fs, uint32_t ino, uint64_t length)
{
    if (fs->flags & MS_RDONLY)
        return -30;
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if ((n->stat.st_mode & LINUX_S_IFMT) != LINUX_S_IFREG)
        return tmpfs_dir(n) ? -21 : -22;
    if (length > INT64_MAX)
        return -27;
    if (length <= (uint64_t)n->stat.st_size) {
        struct storage_node node;
        tmpfs_node(fs, n, &node);
        n->stat.st_size = length;
        /* Revoke PTEs, including COW copies, before dropping file-page refs. */
        sched_truncate_file_mappings(&node, length);
        for (unsigned i = 0; i < TMPFS_HASH; ++i) {
            struct tmpfs_page **link = &n->pages[i];
            while (*link) {
                struct tmpfs_page *p = *link;
                if (p->index >= (length + TMPFS_PAGE - 1) / TMPFS_PAGE) {
                    *link = p->next;
                    mm_free_page(p->phys);
                    kernel_free(p);
                    --fs->used_pages;
                    n->stat.st_blocks -= TMPFS_PAGE / 512;
                } else {
                    if (p->index == length / TMPFS_PAGE && length % TMPFS_PAGE)
                        __builtin_memset((uint8_t *)(uintptr_t)p->phys + length % TMPFS_PAGE, 0,
                                         TMPFS_PAGE - length % TMPFS_PAGE);
                    link = &p->next;
                }
            }
        }
    }
    n->stat.st_size = length;
    tmpfs_changed(n, true);
    return 0;
}
int tmpfs_hold(struct tmpfs_super *fs, uint32_t ino)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    ++n->refs;
    return 0;
}
void tmpfs_drop(struct tmpfs_super *fs, uint32_t ino)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n || !n->refs)
        return;
    --n->refs;
    if (!n->refs && !n->stat.st_nlink)
        tmpfs_free_inode(fs, n);
}
static void tmpfs_remove(struct tmpfs_super *fs, struct tmpfs_inode *parent, struct tmpfs_entry **link)
{
    struct tmpfs_entry *e = *link;
    struct tmpfs_inode *n = e->inode;
    *link = e->next;
    kernel_free(e);
    parent->stat.st_size -= 20;
    if (tmpfs_dir(n)) {
        --parent->stat.st_nlink;
        n->stat.st_nlink = 0;
    } else {
        if (n->stat.st_nlink > 1)
            --fs->used_inodes;
        --n->stat.st_nlink;
    }
    tmpfs_changed(parent, true);
    tmpfs_changed(n, false);
    if (!n->refs && !n->stat.st_nlink)
        tmpfs_free_inode(fs, n);
}
int tmpfs_unlink(struct tmpfs_super *fs, const char *path, bool directory)
{
    if (fs->flags & MS_RDONLY)
        return -30;
    char name[LEONOS_FS_NAME_LEN];
    struct tmpfs_inode *parent;
    int ret = tmpfs_walk(fs, path, true, &parent, name);
    if (ret)
        return ret;
    struct tmpfs_entry **link = tmpfs_find(parent, name);
    if (!*link)
        return -2;
    struct tmpfs_inode *n = (*link)->inode;
    if (tmpfs_dir(n) != directory)
        return directory ? -20 : -21;
    if (directory && n->entries)
        return -39;
    tmpfs_remove(fs, parent, link);
    return 0;
}
int tmpfs_link(struct tmpfs_super *fs, const char *old_path, const char *new_path)
{
    if (fs->flags & MS_RDONLY)
        return -30;
    char name[LEONOS_FS_NAME_LEN];
    struct tmpfs_inode *n, *parent;
    int ret = tmpfs_walk(fs, old_path, false, &n, NULL);
    if (ret)
        return ret;
    if (tmpfs_dir(n))
        return -1;
    ret = tmpfs_walk(fs, new_path, true, &parent, name);
    if (ret)
        return ret;
    if (*tmpfs_find(parent, name))
        return -17;
    if (n->stat.st_nlink >= UINT32_MAX)
        return -31;
    /* tmpfs pins dentries: Linux charges additional hardlinks to nr_inodes. */
    if (fs->max_inodes && fs->used_inodes >= fs->max_inodes)
        return -28;
    struct tmpfs_entry *e = tmpfs_entry_new(parent, name, n);
    if (!e)
        return -12;
    ++fs->used_inodes;
    ++n->stat.st_nlink;
    tmpfs_attach(parent, e);
    tmpfs_changed(n, false);
    return 0;
}
int tmpfs_rename(struct tmpfs_super *fs, const char *old_path, const char *new_path)
{
    if (fs->flags & MS_RDONLY)
        return -30;
    char old_name[LEONOS_FS_NAME_LEN], new_name[LEONOS_FS_NAME_LEN];
    struct tmpfs_inode *old_parent, *new_parent;
    int ret = tmpfs_walk(fs, old_path, true, &old_parent, old_name);
    if (ret)
        return ret;
    ret = tmpfs_walk(fs, new_path, true, &new_parent, new_name);
    if (ret)
        return ret;
    struct tmpfs_entry **old = tmpfs_find(old_parent, old_name), **dest = tmpfs_find(new_parent, new_name);
    if (!*old)
        return -2;
    struct tmpfs_inode *n = (*old)->inode;
    if (*dest && (*dest)->inode == n)
        return 0;
    if (tmpfs_dir(n)) {
        for (struct tmpfs_inode *p = new_parent;; p = p->parent) {
            if (p == n)
                return -22;
            if (p == fs->root)
                break;
        }
    }
    if (*dest) {
        if (tmpfs_dir((*dest)->inode) != tmpfs_dir(n))
            return tmpfs_dir(n) ? -20 : -21;
        if (tmpfs_dir(n) && (*dest)->inode->entries)
            return -39;
    }
    /* Allocate before altering either directory, so ENOMEM is atomic. */
    struct tmpfs_entry *replacement = tmpfs_entry_new(new_parent, new_name, n);
    if (!replacement)
        return -12;
    if (*dest)
        tmpfs_remove(fs, new_parent, dest);
    old = tmpfs_find(old_parent, old_name);
    struct tmpfs_entry *e = *old;
    *old = e->next;
    kernel_free(e);
    old_parent->stat.st_size -= 20;
    if (tmpfs_dir(n))
        --old_parent->stat.st_nlink;
    tmpfs_attach(new_parent, replacement);
    tmpfs_changed(old_parent, true);
    tmpfs_changed(n, false);
    return 0;
}
int tmpfs_readdir(struct tmpfs_super *fs, uint32_t ino, uint64_t *offset, struct leonos_dir_entry *out)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if (!tmpfs_dir(n))
        return -20;
    if (!n->stat.st_nlink)
        return -2;
    struct tmpfs_inode *item;
    const char *name;
    if (*offset < 2) {
        name = *offset ? ".." : ".";
        item = *offset ? n->parent : n;
        ++*offset;
    } else {
        struct tmpfs_entry *e = n->entries;
        while (e && e->cookie < *offset)
            e = e->next;
        if (!e)
            return 0;
        item = e->inode;
        name = e->name;
        *offset = e->cookie + 1;
    }
    struct storage_node node;
    tmpfs_node(fs, item, &node);
    __builtin_memset(out, 0, sizeof(*out));
    out->type = node.type;
    __builtin_memcpy(out->name, name, __builtin_strlen(name) + 1);
    return 1;
}
int tmpfs_readlink(struct tmpfs_super *fs, uint32_t ino, void *buffer, uint32_t length, uint32_t *done)
{
    if (done)
        *done = 0;
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if ((n->stat.st_mode & LINUX_S_IFMT) != LINUX_S_IFLNK)
        return -22;
    if (length > (uint64_t)n->stat.st_size)
        length = n->stat.st_size;
    if (length)
        __builtin_memcpy(buffer, n->target, length);
    if (done)
        *done = length;
    return 0;
}
int tmpfs_stat(struct tmpfs_super *fs, uint32_t ino, struct linux_stat_abi *out)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    *out = n->stat;
    return 0;
}
int tmpfs_permissions(struct tmpfs_super *fs, uint32_t ino, struct leonos_permissions *value, bool write)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if (write) {
        if (fs->flags & MS_RDONLY)
            return -30;
        n->stat.st_mode = (n->stat.st_mode & LINUX_S_IFMT) | (value->mode & 07777);
        n->stat.st_uid = value->uid;
        n->stat.st_gid = value->gid;
        tmpfs_changed(n, false);
    } else
        *value = (struct leonos_permissions){n->stat.st_mode & 07777, n->stat.st_uid, n->stat.st_gid};
    return 0;
}
int tmpfs_utimens(struct tmpfs_super *fs, uint32_t ino, int64_t atime, int64_t mtime, bool set_atime,
                  bool set_mtime)
{
    struct tmpfs_inode *n = tmpfs_inode(fs, ino);
    if (!n)
        return -2;
    if (fs->flags & MS_RDONLY)
        return -30;
    if (set_atime)
        n->stat.atime_sec = atime;
    if (set_mtime)
        n->stat.mtime_sec = mtime;
    if (set_atime || set_mtime)
        tmpfs_changed(n, false);
    return 0;
}
void tmpfs_statfs(struct tmpfs_super *fs, struct linux_statfs_abi *out)
{
    *out = (struct linux_statfs_abi){
        .f_type = 0x01021994,
        .f_bsize = TMPFS_PAGE,
        .f_frsize = TMPFS_PAGE,
        .f_blocks = fs->max_pages,
        .f_bfree = fs->max_pages ? fs->max_pages - fs->used_pages : 0,
        .f_bavail = fs->max_pages ? fs->max_pages - fs->used_pages : 0,
        .f_files = fs->max_inodes,
        .f_ffree = fs->max_inodes ? fs->max_inodes - fs->used_inodes : 0,
        .f_namelen = LEONOS_FS_NAME_LEN - 1,
        .f_flags = LINUX_ST_VALID |
                   (fs->flags & (MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME | MS_NODIRATIME)) |
                   ((fs->flags & MS_RELATIME) ? LINUX_ST_RELATIME : 0)};
    out->f_fsid[0] = fs->volume + 1;
}
void tmpfs_set_flags(struct tmpfs_super *fs, uint64_t flags) { fs->flags = flags; }
