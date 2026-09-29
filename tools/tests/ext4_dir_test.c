#define EXT4_WRITE_FIXTURE_ONLY
#include "ext4_write_test.c"
extern long ext2fs_dirhash(int,const char *,int,const uint32_t *,uint32_t *,uint32_t *);

int main(int argc,char **argv)
{
    assert(argc==4); init_volume(argv[1]); unsigned count=strtoul(argv[3],NULL,10);
    uint64_t parent,other,ino,found; uint8_t type; char name[256];
    uint32_t seed[4]; for (unsigned i=0;i<4;i++) seed[i]=ext4_get_le32(disk+1024+0xec+i*4);
    for (unsigned ver=0;ver<6;ver++) for (unsigned len=1;len<=255;len++) {
        for (unsigned i=0;i<len;i++) name[i]=(char)(1+(i*57+len)%255);
        name[len]=0; uint32_t actual,expected,minor;
        unsigned oracle_ver=ver;
        if (oracle_ver<3 && (ext4_get_le32(disk+1024+0x160)&2)) oracle_ver+=3;
        assert(ext2fs_dirhash(oracle_ver,name,len,seed,&expected,&minor)==0);
        check(storage_ext4_dir_hash(&volume,name,ver,&actual)); assert(actual==expected);
    }
    check(storage_ext4_create(&volume,2,"native",0040755,0,0,NULL,&parent));
    check(storage_ext4_create(&volume,2,"other",0040755,0,0,NULL,&other));
    /* Find a real major-hash collision with the independent Linux library. */
    struct collision_slot { uint32_t hash,index; };
    struct collision_slot *table=calloc(1u<<19,sizeof(*table)); assert(table);
    char collision[2][64]; bool collided=false;
    for (unsigned i=1;i<400000;i++) {
        snprintf(name,sizeof(name),"collision-%u",i); uint32_t h,minor;
        assert(ext2fs_dirhash(1,name,strlen(name),seed,&h,&minor)==0);
        unsigned slot=(h>>1)&((1u<<19)-1);
        while (table[slot].index && table[slot].hash!=h) slot=(slot+1)&((1u<<19)-1);
        if (table[slot].index) {
            snprintf(collision[0],64,"collision-%u",table[slot].index);
            snprintf(collision[1],64,"collision-%u",i); collided=true; break;
        }
        table[slot]=(struct collision_slot){h,i};
    }
    assert(collided); free(table);
    for (unsigned i=0;i<2;i++) check(storage_ext4_create(&volume,parent,collision[i],0100644,0,0,NULL,&ino));
    for (unsigned i=0;i<count;i++) {
        snprintf(name,sizeof(name),"native-file-%05u",i);
        check(storage_ext4_create(&volume,parent,name,0100644,70001,90002,NULL,&ino));
        check(storage_ext4_dir_lookup(&volume,parent,name,&found,&type)); assert(found==ino && type==1);
    }
    for (unsigned i=0;i<count;i+=37) {
        snprintf(name,sizeof(name),"native-file-%05u",i);
        check(storage_ext4_dir_lookup(&volume,parent,name,&ino,&type));
        struct ext4_inode_view in; check(storage_ext4_read_inode(&volume,ino,&in));
        assert(in.uid==70001 && in.gid==90002 && in.links_count==1);
    }
    check(storage_ext4_dir_lookup(&volume,parent,"native-file-00000",&ino,&type));
    check(storage_ext4_link(&volume,other,"hard",ino));
    check(storage_ext4_rename(&volume,parent,"native-file-00001",other,"moved"));
    check(storage_ext4_rename(&volume,parent,"native-file-00002",other,"moved"));
    check(storage_ext4_unlink(&volume,parent,"native-file-00000",false));
    check(storage_ext4_dir_lookup(&volume,other,"hard",&found,&type)); assert(found==ino);
    assert(storage_ext4_unlink(&volume,2,"native",true)==-RELIEFOS_ENOTEMPTY);
    check(storage_ext4_create(&volume,parent,"child",0040700,0,0,NULL,&ino));
    check(storage_ext4_rename(&volume,parent,"child",other,"child"));
    check(storage_ext4_dir_lookup(&volume,ino,"..",&found,&type)); assert(found==other);
    assert(storage_ext4_rename(&volume,2,"other",ino,"cycle")==-RELIEFOS_EINVAL);
    check(storage_ext4_unlink(&volume,other,"child",true));
    memset(name,'z',255); name[255]=0;
    check(storage_ext4_create(&volume,parent,name,0100600,0,0,NULL,&ino));
    check(storage_ext4_unlink(&volume,parent,name,false));
    check(storage_ext4_create(&volume,parent,"symlink",0120777,0,0,"../other/hard",&ino));
    check(storage_ext4_create(&volume,parent,"remove-symlink",0120777,0,0,"../other/hard",&ino));
    check(storage_ext4_unlink(&volume,parent,"remove-symlink",false));
    sync_image(); invalidate();
    struct storage_ext4_dirent entry; uint64_t cursor=0; unsigned entries=0;
    int iter;
    while ((iter=storage_ext4_dir_iterate(&volume,parent,&cursor,&entry))==0) entries++;
    fprintf(stderr,"iterate entries=%u expected=%u ret=%d cursor=%llu\n",entries,count,iter,(unsigned long long)cursor);
    assert(entries==count+2); /* -3 removed + symlink + dot/dotdot + two collisions */
    for (unsigned i=0;i<2;i++) check(storage_ext4_dir_lookup(&volume,parent,collision[i],&found,NULL));
    check(storage_ext4_dir_lookup(&volume,2,"linux",&ino,&type));
    for (unsigned i=0;i<1000;i+=7) {
        snprintf(name,sizeof(name),"linux-file-%05u",i);
        check(storage_ext4_dir_lookup(&volume,ino,name,&found,&type));
    }
    check(storage_ext4_create(&volume,ino,"new-in-linux-htree",0100644,0,0,NULL,&found));
    sync_image();
    if (volume.ext4.super_view.feature_ro_compat&0x400) {
        struct ext4_inode_view in; struct storage_ext4_map_result map; uint8_t *raw;
        check(storage_ext4_read_inode(&volume,parent,&in));
        check(storage_ext4_map_block(&volume,parent,&in,0,false,&map));
        check(storage_ext4_cache_get(&volume,map.physical,&raw,false));
        unsigned tail=32+8*ext4_get_le16(raw+32)+4; raw[tail]^=1;
        check(storage_ext4_dir_lookup(&volume,parent,"native-file-00037",&found,NULL));
        assert(volume.ext4.fs_error);
        assert(storage_ext4_create(&volume,parent,"must-not-write",0100644,0,0,NULL,&found)==-RELIEFOS_EROFS);
        invalidate(); volume.ext4.fs_error=0;
    }
    sync_image(); FILE *f=fopen(argv[2],"wb"); assert(f);
    assert(fwrite(disk,1,disk_len,f)==disk_len); fclose(f);
    storage_ext4_journal_close(&volume); invalidate(); free(disk);
    printf("PASS %u native directory entries, link/rename/rmdir/longname/Linux HTREE\n",count);
    return 0;
}
