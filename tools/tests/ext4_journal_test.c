/* Reuse the real raw-device/inode fixture; only transport failure injection
 * and flush observation are new. No mocked journal or allocator logic. */
#define main ext4_read_fixture_main
#define EXT4_JOURNAL_TRANSPORT_FIXTURE 1
#define storage_write_device ext4_fixture_write_device
#include "ext4_extent_test.c"
#undef storage_write_device
#undef main

static int fail_write_at = -1, device_failed;
static int fail_flush_at = -1;
static uint64_t flushes, last_flushed_writes;
static uint8_t *durable;
int storage_write_device(const struct storage_volume *v, uint64_t lba,
                         uint32_t n, const void *in)
{
    if (device_failed || (fail_write_at >= 0 && writes == (uint64_t)fail_write_at)) {
        device_failed = 1; return -RELIEFOS_EIO;
    }
    const uint8_t *raw = in;
    if (n && raw[0]==0xc0 && raw[1]==0x3b && raw[2]==0x39 && raw[3]==0x98 &&
        raw[4]==0 && raw[5]==0 && raw[6]==0 && raw[7]==2)
        assert(last_flushed_writes == writes); /* Data barrier precedes commit. */
    return ext4_fixture_write_device(v, lba, n, in);
}
int storage_ext4_device_flush(const struct storage_volume *v)
{
    (void)v;
    if (device_failed || (fail_flush_at >= 0 && flushes == (uint64_t)fail_flush_at)) {
        device_failed=1; return -RELIEFOS_EIO;
    }
    ++flushes; last_flushed_writes=writes;
    if (durable) memcpy(durable,disk,disk_len);
    return 0;
}
static uint32_t be32(const uint8_t *p)
{ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static void b32(uint8_t *p, uint32_t n)
{ p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
static uint32_t crc_oracle(uint32_t crc, const uint8_t *p, size_t n)
{
    for (size_t i=0;i<n;i++) {
        crc ^= p[i];
        for (unsigned k=0;k<8;k++) crc=(crc>>1)^(0x82f63b78u & (0u-(crc&1)));
    }
    return crc;
}
static void load_geometry(void)
{
    struct storage_ext4_super_view sb;
    assert(storage_ext4_parse_super(disk+1024,1024,&sb)==0);
    volume.ext4.super_view=sb; volume.ext4.block_size=sb.block_size;
    volume.ext4.blocks_count=sb.blocks_count; volume.ext4.group_count=sb.group_count;
    volume.ext4.desc_size=sb.desc_size; volume.ext4.first_data_block=sb.first_data_block;
    volume.ext4.inode_size=sb.inode_size; volume.ext4.inodes_count=sb.inodes_count;
    volume.ext4.inodes_per_group=sb.inodes_per_group; volume.ext4.blocks_per_group=sb.blocks_per_group;
    volume.ext_sector_count=disk_len/512; ++volume.mount_generation;
    volume.kind=STORAGE_VOLUME_RAM; volume.ready=1; volume.filesystem=STORAGE_FILESYSTEM_EXT4;
    volume.read_only_reason=0; volume.ext4.fs_error=0;
}
static uint64_t file_block(uint64_t ino, uint64_t logical)
{
    struct ext4_inode_view in; struct storage_ext4_map_result map;
    assert(storage_ext4_read_inode(&volume,ino,&in)==0);
    assert(storage_ext4_map_block(&volume,ino,&in,logical,false,&map)==0);
    assert(!map.hole && !map.unwritten); return map.physical;
}
static void save(const char *path)
{ FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(disk,1,disk_len,f)==disk_len); assert(!fclose(f)); }
static uint32_t next_log(uint8_t *js, uint32_t pos)
{ return pos+1==be32(js+16) ? be32(js+20) : pos+1; }
static uint8_t *log_block(uint32_t pos)
{ return disk+file_block(volume.ext4.super_view.journal_inum,pos)*volume.ext4.block_size; }
static void log_header(uint8_t *raw,uint32_t type,uint32_t sequence)
{ memset(raw,0,volume.ext4.block_size); b32(raw,0xc03b3998); b32(raw+4,type); b32(raw+8,sequence); }
static void descriptor_crc(uint8_t *raw,uint8_t *js,unsigned profile)
{
    if (!(profile&24)) return;
    uint32_t bs=volume.ext4.block_size; b32(raw+bs-4,0);
    b32(raw+bs-4,crc_oracle(crc_oracle(~0u,js+48,16),raw,bs));
}
static void commit_crc(uint8_t *raw,uint8_t *js,unsigned profile)
{
    if (profile==1) { raw[12]=1; raw[13]=4; b32(raw+16,~0u); }
    if (profile&24) { b32(raw+16,0); b32(raw+16,crc_oracle(crc_oracle(~0u,js+48,16),raw,volume.ext4.block_size)); }
}
int main(int argc,char **argv)
{
    assert(argc==5);
    disk=load(argv[1],&disk_len); load_geometry();
    uint64_t payload_ino=strtoull(argv[2],NULL,10);
    uint64_t target=file_block(payload_ino,0);
    uint64_t jb=file_block(volume.ext4.super_view.journal_inum,0);
    uint32_t bs=volume.ext4.block_size;
    uint8_t *js=disk+jb*bs;
    unsigned profile=strtoul(argv[3],NULL,0);
    b32(js+36,profile==1 ? 1 : 0); b32(js+40,profile==1 ? 1 : profile|1);
    /* Force both physical ring wrap and transaction sequence wrap. */
    b32(js+88,be32(js+16)-2); b32(js+24,UINT32_MAX);
    js[80]=(profile & 24) ? 4 : 0;
    if (profile&24) { b32(js+252,0); b32(js+252,crc_oracle(~0u,js,1024)); }
    invalidate();
    uint8_t *initial=malloc(disk_len); assert(initial); memcpy(initial,disk,disk_len);
    durable=malloc(disk_len); assert(durable); memcpy(durable,initial,disk_len);
    uint8_t *old=malloc(bs), *next=malloc(bs); assert(old&&next);
    memcpy(old,disk+target*bs,bs); memset(next,0x63,bs); b32(next,0xc03b3998);
    assert(storage_ext4_journal_open(&volume)==0);
    struct storage_ext4_handle handle;
    assert(storage_ext4_journal_start(&volume,2,&handle)==0);
    uint8_t *cache;
    assert(storage_ext4_cache_get(&volume,target,&cache,true)==0);
    memcpy(cache,next,bs);
    assert(storage_ext4_cache_mark_dirty(&volume,target)==0);
    assert(storage_ext4_journal_dirty(&handle,target)==0);
    assert(storage_ext4_cache_flush(&volume)==0);
    assert(memcmp(disk+target*bs,old,bs)==0); /* No home write before commit. */
    assert(storage_ext4_journal_stop(&handle)==0);
    assert(storage_ext4_journal_commit(&volume,true)==0);
    assert(flushes>=3 && memcmp(disk+target*bs,old,bs)==0);
    assert(ext4_get_le32(disk+1024+96)&4); /* RECOVER is durable. */
    save(argv[4]); /* e2fsck independently replays this exact committed log. */
    storage_ext4_journal_close(&volume); invalidate(); load_geometry();
    assert(storage_ext4_journal_open(&volume)==0);
    assert(storage_ext4_journal_replay(&volume)==0);
    assert(memcmp(disk+target*bs,next,bs)==0);
    uint64_t before=writes;
    assert(storage_ext4_journal_replay(&volume)==0 && writes==before);
    storage_ext4_journal_close(&volume); invalidate();
    uint64_t saved_len;
    uint8_t *committed_image=load(argv[4],&saved_len); assert(saved_len==disk_len);
    /* A committed checksum error must fail without a single replay write. */
    if (profile) {
        memcpy(disk,committed_image,disk_len); load_geometry();
        js=disk+jb*bs; uint32_t data_pos=next_log(js,be32(js+28));
        log_block(data_pos)[bs-1]^=1; invalidate();
        assert(storage_ext4_journal_open(&volume)==0); before=writes;
        assert(storage_ext4_journal_replay(&volume)==-RELIEFOS_EIO && writes==before);
        assert(volume.read_only_reason==STORAGE_EXT4_READ_ONLY_JOURNAL_CORRUPT);
        storage_ext4_journal_close(&volume); invalidate();
    }
    /* Later committed revokes suppress earlier writes. More than 512 entries
     * forces bounded-table overflow and its streaming fallback. */
    memcpy(disk,committed_image,disk_len); load_geometry(); js=disk+jb*bs;
    uint32_t pos=be32(js+28), sequence=be32(js+24)+1;
    for (unsigned i=0;i<3;i++) pos=next_log(js,pos); /* after first commit */
    unsigned revoked=0, record_size=(profile&2)?8:4;
    while (revoked<600) {
        uint8_t *raw=log_block(pos); log_header(raw,5,sequence);
        uint32_t at=16, end=bs-((profile&24)?4:0);
        while (at+record_size<=end && revoked<600) {
            uint64_t blk=revoked==599 ? target : volume.ext4.blocks_count-1024+revoked;
            if (record_size==8) { b32(raw+at,blk>>32); b32(raw+at+4,blk); }
            else b32(raw+at,blk);
            at+=record_size; revoked++;
        }
        b32(raw+12,at); descriptor_crc(raw,js,profile); pos=next_log(js,pos);
    }
    uint8_t *commit=log_block(pos); log_header(commit,2,sequence); commit_crc(commit,js,profile);
    invalidate(); assert(storage_ext4_journal_open(&volume)==0);
    assert(storage_ext4_journal_replay(&volume)==0);
    assert(memcmp(disk+target*bs,old,bs)==0 && volume.ext4.journal_revoke_hits);
    storage_ext4_journal_close(&volume); invalidate();
    /* Each write failure is a power loss window; reboot from on-disk bytes. */
    for (unsigned cut=0;cut<20;cut++) {
        memcpy(disk,initial,disk_len); load_geometry(); writes=0; device_failed=0;
        memcpy(durable,initial,disk_len); flushes=0; last_flushed_writes=0;
        fail_write_at=-1; fail_flush_at=-1;
        assert(storage_ext4_journal_open(&volume)==0);
        assert(storage_ext4_journal_start(&volume,2,&handle)==0);
        assert(storage_ext4_cache_get(&volume,target,&cache,true)==0); memcpy(cache,next,bs);
        assert(storage_ext4_cache_mark_dirty(&volume,target)==0);
        assert(storage_ext4_journal_stop(&handle)==0);
        if (cut<12) fail_write_at=(int)cut;
        else fail_flush_at=(int)cut-12;
        int committed=storage_ext4_journal_commit(&volume,true);
        int checkpoint=committed ? committed : storage_ext4_journal_checkpoint(&volume);
        (void)checkpoint;
        storage_ext4_journal_close(&volume); invalidate();
        memcpy(disk,durable,disk_len); /* Reboot loses all unflushed writes. */
        fail_write_at=-1; fail_flush_at=-1; device_failed=0; load_geometry();
        assert(storage_ext4_journal_open(&volume)==0);
        assert(storage_ext4_journal_replay(&volume)==0);
        assert(!memcmp(disk+target*bs,old,bs)||!memcmp(disk+target*bs,next,bs));
        if (!committed) assert(!memcmp(disk+target*bs,next,bs));
        storage_ext4_journal_close(&volume); invalidate();
    }
    memcpy(disk,initial,disk_len); load_geometry();
    assert(storage_ext4_journal_open(&volume)==0);
    assert(storage_ext4_journal_start(&volume,1,&handle)==0);
    assert(storage_ext4_cache_get(&volume,target,&cache,true)==0); memcpy(cache,next,bs);
    assert(storage_ext4_cache_mark_dirty(&volume,target)==0);
    assert(storage_ext4_cache_get(&volume,target+1,&cache,true)==-RELIEFOS_ENOSPC);
    assert(storage_ext4_journal_stop(&handle)==-RELIEFOS_ENOSPC);
    assert(storage_ext4_cache_read(&volume,target,next)==0 && !memcmp(old,next,bs));
    storage_ext4_journal_close(&volume); invalidate();
    /* Allocator bitmap/GDT/superblock are one transaction, including abort. */
    memcpy(disk,initial,disk_len); load_geometry();
    assert(storage_ext4_journal_open(&volume)==0);
    uint64_t free_before=volume.ext4.super_view.free_blocks_count, allocated_first;
    uint32_t allocated_count;
    assert(storage_ext4_journal_start(&volume,32,&handle)==0);
    assert(storage_ext4_alloc_blocks(&volume,target+16,3,&allocated_first,&allocated_count)==0);
    assert(allocated_count==3 && volume.ext4.super_view.free_blocks_count==free_before-3);
    storage_ext4_journal_abort(&handle,-RELIEFOS_EIO);
    assert(storage_ext4_journal_stop(&handle)==-RELIEFOS_EIO);
    assert(volume.ext4.super_view.free_blocks_count==free_before && !memcmp(disk,initial,disk_len));
    assert(storage_ext4_alloc_blocks(&volume,target+16,3,&allocated_first,&allocated_count)==0);
    assert(storage_ext4_free_blocks(&volume,allocated_first,allocated_count)==0);
    assert(storage_ext4_journal_commit(&volume,true)==0);
    assert(storage_ext4_journal_checkpoint(&volume)==0);
    assert(volume.ext4.super_view.free_blocks_count==free_before);
    char clean[4096]; assert(snprintf(clean,sizeof(clean),"%s.clean",argv[4])<(int)sizeof(clean));
    save(clean);
    storage_ext4_journal_close(&volume); invalidate();
    printf("PASS journal profile=%u block_size=%u commit/replay/repeat/credits/wrap/revoke-overflow/corruption/allocator/20 crash windows\n",profile,bs);
    free(durable); free(committed_image); free(initial); free(old); free(next); free(disk);
    return 0;
}
