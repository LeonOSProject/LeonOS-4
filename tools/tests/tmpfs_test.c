#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ntclks/tmpfs.h>
#include <ntclks/time.h>
#include <linux/mount.h>
static struct {
    uint64_t phys;
    unsigned refs;
} pages[64];
uint64_t mm_alloc_page(void)
{
    for (unsigned i = 0; i < 64; ++i)
        if (!pages[i].refs) {
            void *p = aligned_alloc(4096, 4096);
            assert(p);
            pages[i].phys = (uintptr_t)p;
            pages[i].refs = 1;
            return pages[i].phys;
        }
    return 0;
}
void mm_retain_page(uint64_t phys)
{
    for (unsigned i = 0; i < 64; ++i)
        if (pages[i].phys == phys && pages[i].refs) {
            ++pages[i].refs;
            return;
        }
    abort();
}
void mm_free_page(uint64_t phys)
{
    for (unsigned i = 0; i < 64; ++i)
        if (pages[i].phys == phys && pages[i].refs) {
            if (!--pages[i].refs)
                free((void *)(uintptr_t)phys);
            return;
        }
    abort();
}
static unsigned invalidations;
void sched_truncate_file_mappings(const struct storage_node *node, uint64_t length)
{
    assert(node->flags & STORAGE_NODE_FLAG_TMPFS);
    (void)length;
    ++invalidations;
}
void *kernel_malloc(size_t size) { return malloc(size); }
void kernel_free(void *p) { free(p); }
void page_cache_invalidate_node(const struct storage_node *node)
{
    assert(node->flags & STORAGE_NODE_FLAG_TMPFS);
}
uint64_t mm_total_memory_kib(void) { return 65536; }
int time_wall_clock(struct leonos_time_info *out)
{
    memset(out, 0, sizeof(*out));
    out->unix_seconds = 1234;
    return 0;
}
int main(void)
{
    struct tmpfs_super *fs = NULL;
    struct storage_node a, b;
    struct linux_stat_abi st;
    struct linux_statfs_abi space;
    char data[16384], got[16384];
    memset(data, 0xab, sizeof(data));
    uint32_t done;
    assert(tmpfs_new(3, 0, "size=8k,nr_inodes=8,mode=1770,uid=42,gid=43", 0, 0, &fs) == 0);
    assert(tmpfs_lookup(fs, "/", &a) == 0 && tmpfs_stat(fs, a.first_cluster, &st) == 0);
    assert(st.st_uid == 42 && st.st_gid == 43 && st.st_mode == 041770 && st.st_nlink == 2);
    assert(tmpfs_create(fs, "/a", 0100640, NULL, &a) == 0);
    assert(tmpfs_write(fs, a.first_cluster, 0, data, sizeof(data), &done) == -28 && done == 8192);
    tmpfs_statfs(fs, &space);
    assert(space.f_type == 0x01021994 && space.f_blocks == 2 && space.f_bfree == 0);
    assert(tmpfs_truncate(fs, a.first_cluster, 17) == 0);
    assert(tmpfs_truncate(fs, a.first_cluster, 8192) == 0);
    assert(tmpfs_read(fs, a.first_cluster, 0, got, sizeof(got), &done) == 0 && done == 8192);
    assert(!memcmp(got, data, 17));
    for (unsigned i = 17; i < done; ++i)
        assert(got[i] == 0);
    assert(tmpfs_create(fs, "/d", 0040700, NULL, &b) == 0);
    assert(tmpfs_hold(fs, a.first_cluster) == 0);
    assert(tmpfs_link(fs, "/a", "/d/link") == 0);
    assert(tmpfs_rename(fs, "/a", "/d/moved") == 0);
    assert(tmpfs_lookup(fs, "/a", &b) == -2);
    assert(tmpfs_unlink(fs, "/d", true) == -39);
    assert(tmpfs_unlink(fs, "/d/link", false) == 0);
    assert(tmpfs_unlink(fs, "/d/moved", false) == 0);
    assert(tmpfs_stat(fs, a.first_cluster, &st) == 0 && st.st_nlink == 0 && st.st_size == 8192);
    assert(tmpfs_read(fs, a.first_cluster, 0, got, 17, &done) == 0 && done == 17 && !memcmp(data, got, 17));
    tmpfs_drop(fs, a.first_cluster);
    assert(tmpfs_stat(fs, a.first_cluster, &st) == -2);
    assert(tmpfs_create(fs, "/d/sub", 0040755, NULL, &a) == 0);
    assert(tmpfs_rename(fs, "/d", "/d/sub/cycle") == -22);
    assert(tmpfs_create(fs, "/symlink", 0120777, "d/sub", &a) == 0);
    assert(tmpfs_readlink(fs, a.first_cluster, got, 3, &done) == 0 && done == 3 && !memcmp(got, "d/s", 3));
    struct leonos_permissions p = {0601, 1000, 1001};
    assert(tmpfs_permissions(fs, a.first_cluster, &p, true) == 0);
    assert(tmpfs_stat(fs, a.first_cluster, &st) == 0 && st.st_uid == 1000 && st.st_mode == 0120601);
    tmpfs_set_flags(fs, MS_RDONLY);
    assert(tmpfs_create(fs, "/no", 0100600, NULL, &b) == -30);
    assert(tmpfs_unlink(fs, "/symlink", false) == -30);
    assert(tmpfs_permissions(fs, a.first_cluster, &p, true) == -30);
    tmpfs_destroy(fs);
    assert(tmpfs_new(3, 0, "size=1k,unknown=yes", 0, 0, &fs) == -22 && !fs);
    assert(tmpfs_new(3, 0, "size=18446744073709551615G", 0, 0, &fs) == -22 && !fs);
    assert(tmpfs_new(3, 0, "nr_inodes=2", 0, 0, &fs) == 0);
    assert(tmpfs_create(fs, "/a", 0100600, NULL, &a) == 0);
    assert(tmpfs_create(fs, "/b", 0100600, NULL, &b) == -28);
    assert(tmpfs_link(fs, "/a", "/hardlink") == -28);
    tmpfs_destroy(fs);
    assert(tmpfs_new(3, 0, "size=4k", 0, 0, &fs) == 0);
    assert(tmpfs_create(fs, "/shared", 0100600, NULL, &a) == 0);
    assert(tmpfs_truncate(fs, a.first_cluster, 8192) == 0);
    uint64_t first, second;
    assert(tmpfs_get_page(fs, a.first_cluster, 0, &first) == 0);
    assert(tmpfs_get_page(fs, a.first_cluster, 0, &second) == 0 && first == second);
    assert(tmpfs_get_page(fs, a.first_cluster, 4096, &second) == -28);
    assert(tmpfs_get_page(fs, a.first_cluster, 8192, &second) == -22);
    strcpy((char *)(uintptr_t)first, "shared");
    assert(tmpfs_read(fs, a.first_cluster, 0, got, 7, &done) == 0 && !strcmp(got, "shared"));
    assert(tmpfs_write(fs, a.first_cluster, 0, "direct", 7, &done) == 0);
    assert(!strcmp((char *)(uintptr_t)first, "direct"));
    unsigned before = invalidations;
    assert(tmpfs_truncate(fs, a.first_cluster, 0) == 0 && invalidations == before + 1);
    tmpfs_statfs(fs, &space);
    assert(space.f_bfree == 1);
    /* Mapping references outlive removal of the inode's ownership. */
    assert(!strcmp((char *)(uintptr_t)first, "direct"));
    mm_free_page(first);
    mm_free_page(first);
    tmpfs_destroy(fs);
    for (unsigned i = 0; i < 64; ++i)
        assert(!pages[i].refs);
    puts("PASS tmpfs sparse pages, limits, metadata, links, rename cycles, held unlink and read-only");
}
