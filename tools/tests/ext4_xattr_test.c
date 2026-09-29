#define EXT4_WRITE_FIXTURE_ONLY
#include "ext4_write_test.c"
int main(int argc,char **argv)
{
    assert(argc==4); init_volume(argv[1]); uint64_t ino=strtoull(argv[2],NULL,10);
    uint32_t len,bs=volume.ext4.block_size; char small[128]; uint8_t *big=malloc(bs),*out=malloc(bs);
    assert(big && out); for (unsigned i=0;i<bs;i++) big[i]=(i*19+23)%251;
    check(storage_ext4_getxattr(&volume,ino,"user.linux",small,sizeof(small),&len));
    assert(len==8 && !memcmp(small,"original",8));
    check(storage_ext4_setxattr(&volume,ino,"user.small","native",6,1));
    assert(storage_ext4_setxattr(&volume,ino,"user.small","x",1,1)==-RELIEFOS_EEXIST);
    assert(storage_ext4_setxattr(&volume,ino,"user.missing","x",1,2)==-61);
    assert(storage_ext4_setxattr(&volume,ino,"security.test","x",1,0)==-RELIEFOS_EOPNOTSUPP);
    check(storage_ext4_setxattr(&volume,ino,"user.big",big,bs-128,0));
    assert(storage_ext4_getxattr(&volume,ino,"user.big",small,sizeof(small),&len)==-34);
    check(storage_ext4_getxattr(&volume,ino,"user.big",NULL,0,&len)); assert(len==bs-128);
    sync_image(); invalidate();
    check(storage_ext4_getxattr(&volume,ino,"user.big",out,bs,&len)); assert(len==bs-128 && !memcmp(out,big,len));
    /* Linux can share one EA block. Construct that valid state, then force
     * copy-on-write and verify unlink releases only the replacement block. */
    uint64_t free_before=volume.ext4.super_view.free_blocks_count,other;
    check(storage_ext4_create(&volume,2,"other",0100644,0,0,NULL,&other));
    struct storage_ext4_handle h; struct ext4_inode_view first,second; uint8_t *raw;
    check(storage_ext4_journal_start(&volume,32,&h));
    check(storage_ext4_read_inode(&volume,ino,&first)); check(storage_ext4_read_inode(&volume,other,&second));
    assert(first.file_acl); second.file_acl=first.file_acl; second.blocks=bs/512;
    check(storage_ext4_write_inode(&volume,other,&second));
    check(storage_ext4_cache_get(&volume,first.file_acl,&raw,true)); ext4_put_le32(raw+4,2);
    if (volume.ext4.super_view.feature_ro_compat&0x400) {
        uint8_t word[8]; ext4_put_le64(word,first.file_acl); ext4_put_le32(raw+16,0);
        uint32_t crc=storage_ext4_crc32c(storage_ext4_super_csum_seed(&volume.ext4.super_view),word,8);
        ext4_put_le32(raw+16,storage_ext4_crc32c(crc,raw,bs));
    }
    check(storage_ext4_cache_mark_dirty(&volume,first.file_acl)); check(storage_ext4_journal_stop(&h));
    memset(out,0x73,bs-128); check(storage_ext4_setxattr(&volume,other,"user.big",out,bs-128,2));
    check(storage_ext4_getxattr(&volume,ino,"user.big",out,bs,&len)); assert(!memcmp(out,big,len));
    check(storage_ext4_unlink(&volume,2,"other",false));
    assert(volume.ext4.super_view.free_blocks_count==free_before);
    check(storage_ext4_removexattr(&volume,ino,"user.small"));
    assert(storage_ext4_getxattr(&volume,ino,"user.small",small,sizeof(small),&len)==-61);
    check(storage_ext4_setxattr(&volume,ino,"user.linux","replaced",8,2));
    volume.read_only_reason=1;
    assert(storage_ext4_setxattr(&volume,ino,"user.ro","x",1,0)==-RELIEFOS_EROFS);
    volume.read_only_reason=0; sync_image();
    if (volume.ext4.super_view.feature_ro_compat&0x400) {
        check(storage_ext4_read_inode(&volume,ino,&first));
        check(storage_ext4_cache_get(&volume,first.file_acl,&raw,false)); raw[bs-1]^=1;
        assert(storage_ext4_getxattr(&volume,ino,"user.big",out,bs,&len)==-RELIEFOS_EIO);
        assert(volume.ext4.fs_error); invalidate(); volume.ext4.fs_error=0;
    }
    FILE *f=fopen(argv[3],"wb"); assert(f); assert(fwrite(disk,1,disk_len,f)==disk_len); fclose(f);
    storage_ext4_journal_close(&volume); invalidate(); free(disk); free(big); free(out);
    puts("PASS Linux/native inline/external user xattrs, create/replace/remove/errors/reload"); return 0;
}
