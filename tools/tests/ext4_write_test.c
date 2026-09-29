/* Actual backend mutations; Python compares every saved image with e2fsprogs. */
#define main extent_read_fixture_main
#include "ext4_extent_test.c"
#undef main

static void check(int ret) { if (ret < 0) fprintf(stderr, "backend error %d\n", ret); assert(ret == 0); }
static void sync_image(void)
{
    check(storage_ext4_journal_commit(&volume, true));
    check(storage_ext4_journal_checkpoint(&volume));
    check(storage_ext4_cache_flush(&volume));
}
static void init_volume(const char *path)
{
    disk = load(path, &disk_len);
    struct storage_ext4_super_view s;
    check(storage_ext4_parse_super(disk+1024, 1024, &s));
    volume.ext4.super_view = s; volume.ext4.block_size = s.block_size;
    volume.ext4.blocks_count = s.blocks_count; volume.ext4.group_count = s.group_count;
    volume.ext4.desc_size = s.desc_size; volume.ext4.first_data_block = s.first_data_block;
    volume.ext4.inode_size = s.inode_size; volume.ext4.inodes_count = s.inodes_count;
    volume.ext4.inodes_per_group = s.inodes_per_group; volume.ext4.blocks_per_group = s.blocks_per_group;
    volume.ext_sector_count = disk_len/512; volume.mount_generation = 1;
    check(storage_ext4_journal_open(&volume));
}
#ifndef EXT4_WRITE_FIXTURE_ONLY
int main(int argc, char **argv)
{
    assert(argc == 5); init_volume(argv[1]);
    uint64_t ino = strtoull(argv[2], NULL, 10), bs = volume.ext4.block_size;
    size_t size = bs*800; uint8_t *expected = calloc(1, size), *out = malloc(size);
    assert(expected && out); uint32_t done; struct ext4_inode_view in;
    uint64_t before_free=volume.ext4.super_view.free_blocks_count,offset=0;
    memset(out,0x37,bs*32);
    for (;;) {
        int ret=storage_ext4_write_file_range(&volume,ino,offset,out,bs*32,&done);
        offset+=done;
        if (ret<0) { fprintf(stderr,"fill ret=%d offset=%llu free=%llu\n",ret,(unsigned long long)offset,
                            (unsigned long long)volume.ext4.super_view.free_blocks_count);
                     assert(ret==-RELIEFOS_ENOSPC); break; }
        assert(done==bs*32 && offset<disk_len);
    }
    assert(offset>0); check(storage_ext4_truncate(&volume,ino,0)); sync_image();
    assert(volume.ext4.super_view.free_blocks_count==before_free);
    /* Fragmented sparse writes force the inline root and a full leaf to split. */
    for (unsigned i=0; i<350; i++) {
        uint64_t pos = i*2*bs+17; uint8_t payload[79]; memset(payload, i%251+1, sizeof(payload));
        check(storage_ext4_write_file_range(&volume, ino, pos, payload, sizeof(payload), &done));
        assert(done == sizeof(payload)); memcpy(expected+pos, payload, done);
    }
    check(storage_ext4_fallocate(&volume, ino, 1, bs*710, bs*20));
    check(storage_ext4_read_inode(&volume, ino, &in)); assert(in.size == 698*bs+96);
    struct storage_ext4_fiemap_extent preallocated[32]; uint32_t nr;
    check(storage_ext4_fiemap(&volume,ino,710*bs,20*bs,preallocated,32,&nr));
    uint64_t mapped=0; for (unsigned i=0;i<nr;i++) mapped+=preallocated[i].length;
    assert(nr && mapped==20*bs);
    if (in.flags&EXT4_EXTENTS_FL) for (unsigned i=0;i<nr;i++) assert(preallocated[i].flags&EXT4_FIEMAP_UNWRITTEN);
    memset(out, 0xa7, bs*9+121);
    check(storage_ext4_write_file_range(&volume, ino, bs*713+31, out, bs*9+121, &done));
    memcpy(expected+bs*713+31, out, done);
    check(storage_ext4_truncate(&volume, ino, size));
    check(storage_ext4_fallocate(&volume, ino, 3, bs*4+19, bs*23+71));
    memset(expected+bs*4+19, 0, bs*23+71);
    check(storage_ext4_fallocate(&volume, ino, 16, bs*713+41, bs*7+53));
    memset(expected+bs*713+41, 0, bs*7+53);
    assert(storage_ext4_fallocate(&volume, ino, 8, 0, bs) == -RELIEFOS_EOPNOTSUPP);
    check(storage_ext4_truncate(&volume, ino, bs*517+39));
    memset(expected+bs*517+39, 0, size-bs*517-39);
    check(storage_ext4_truncate(&volume, ino, size));
    sync_image(); invalidate();
    check(storage_ext4_read_inode(&volume, ino, &in));
    check(storage_ext4_read_file_range(&volume, ino, &in, 0, out, size, &done));
    assert(done==size && !memcmp(out,expected,size));
    FILE *f=fopen(argv[3],"wb"); assert(f); assert(fwrite(disk,1,disk_len,f)==disk_len); fclose(f);
    f=fopen(argv[4],"wb"); assert(f); assert(fwrite(expected,1,size,f)==size); fclose(f);
    storage_ext4_journal_close(&volume); invalidate(); free(disk); free(expected); free(out);
    puts("PASS native ENOSPC reclaim, file write, split, unwritten, punch, zero, truncate, reload");
    return 0;
}
#endif
