/* Full storage facade: an ext4 inode handle must read through native ops. */
#define main ext2_fixture_main
#include "storage_mkdir_mount_test.c"
#undef main

int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long bytes = ftell(f); assert(bytes > 0); rewind(f);
    struct storage_volume *v = &g_volumes[0];
    *v = (struct storage_volume){.kind = STORAGE_VOLUME_RAM, .ram_bytes = bytes,
                                 .ext_sector_count = bytes / 512};
    v->ram_base = malloc(bytes); assert(v->ram_base);
    assert(fread(v->ram_base, 1, bytes, f) == (size_t)bytes); fclose(f);
    assert(storage_ext4_mount(v) == 0); v->ready = true; g_active_volume = v;
    struct storage_node node = {.volume_id = 0, .first_cluster = strtoul(argv[2], NULL, 10),
                                .type = RELIEFOS_FS_TYPE_FILE, .flags = STORAGE_NODE_FLAG_EXT2};
    struct storage_inode_ref *ref = NULL;
    assert(storage_inode_get(&node, &ref) == 0 && ref);
    assert(storage_inode_refresh(&node) == 0 && node.size);
    f = fopen(argv[3], "rb"); assert(f);
    uint8_t *got = malloc(131072), *want = malloc(131072); assert(got && want);
    for (uint64_t pos = 0; pos < node.size;) {
        uint32_t n = 0; size_t expected = fread(want, 1, 131072, f);
        assert(storage_read_node(&node, pos, got, 131072, &n) == 0);
        assert(n == expected && n && memcmp(got, want, n) == 0); pos += n;
    }
    fclose(f); assert(storage_inode_put(ref) == 0);
    storage_ext4_journal_close(v);
    storage_ext4_cache_invalidate(v); free(v->ram_base); free(got); free(want);
    puts("PASS ext4 VFS inode hold/refresh/read/release");
}
