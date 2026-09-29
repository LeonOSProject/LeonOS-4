#define main legacy_mkdir_fixture_main
#include "storage_mkdir_mount_test.c"
#undef main
int main(int argc,char **argv)
{
    assert(argc==3); load_volume(argv[1],0,"/"); load_volume(argv[1],1,"/target");
    g_active_volume=&g_volumes[0];
    assert(storage_mkdir("/a")==0); assert(storage_mkdir("/b")==0);
    assert(storage_mkdir("/target/t")==0);
    uint8_t data[8192],out[8192]; memset(data,0xa3,sizeof(data));
    assert(storage_write_file("/a/file",data,sizeof(data))==0);
    struct storage_node node; assert(storage_lookup_path("/a/file",&node)==0);
    struct storage_inode_ref *held; assert(storage_inode_get(&node,&held)==0);
    assert(storage_link("/a/file","/b/hard")==0);
    assert(storage_unlink("/a/file")==0); assert(storage_unlink("/b/hard")==0);
    uint32_t got; assert(storage_read_node(&node,0,out,sizeof(out),&got)==0 && got==sizeof(out));
    assert(!memcmp(data,out,sizeof(out)));
    assert(storage_write_held_node(&node,13,"held",4,&got)==0 && got==4);
    assert(storage_truncate_held_node(&node,51)==0);
    assert(storage_inode_put(held)==0);
    struct storage_node n1,n2; struct storage_inode_ref *h1,*h2;
    assert(storage_write_file("/a/open1","one",3)==0 && storage_write_file("/a/open2","two",3)==0);
    assert(storage_lookup_path("/a/open1",&n1)==0 && storage_lookup_path("/a/open2",&n2)==0);
    assert(storage_inode_get(&n1,&h1)==0 && storage_inode_get(&n2,&h2)==0);
    assert(storage_unlink("/a/open1")==0 && storage_unlink("/a/open2")==0);
    assert(storage_inode_put(h1)==0); /* Remove a non-head orphan. */
    assert(storage_read_node(&n2,0,out,3,&got)==0 && got==3 && !memcmp(out,"two",3));
    assert(storage_inode_put(h2)==0);
    assert(storage_write_file("/a/reused","reuse",5)==0);
    assert(storage_inode_refresh(&n1)==-116);
    assert(storage_unlink("/a/reused")==0);
    assert(storage_write_file("/a/one",data,999)==0);
    assert(storage_rename("/a/one","/b/two")==0);
    assert(storage_lookup_path("/b/two",&node)==0);
    assert(storage_rename("/b/two","/target/t/two")==-18);
    assert(storage_symlink("/b/two","/a/link")==0);
    char link[128]; assert(storage_readlink("/a/link",link,sizeof(link),&got)==0);
    assert(got==6 && !memcmp(link,"/b/two",6));
    struct reliefos_permissions p={.uid=70001,.gid=90002,.mode=0600};
    assert(storage_inode_permissions(&node,&p,true)==0);
    struct linux_stat_abi st; assert(storage_inode_stat(&node,&st)==0);
    assert(st.st_uid==70001 && st.st_gid==90002 && st.st_size==999);
    assert(storage_sync_all()==0);
    g_active_volume=&g_volumes[0];
    struct ext4_inode_view native; struct storage_ext4_map_result mapping;
    assert(storage_ext4_read_inode(&g_storage,node.first_cluster,&native)==0);
    assert(storage_ext4_map_block(&g_storage,node.first_cluster,&native,0,false,&mapping)==0);
    assert(storage_read_node(&node,0,out,512,&got)==0 && got==512);
    struct storage_volume alias=g_storage; out[0]=0x77;
    assert(storage_write_device(&alias,mapping.physical*(g_storage.ext4.block_size/512),1,out)==0);
    assert(storage_read_node(&node,0,out,1,&got)==0 && got==1 && out[0]==0x77);
    g_active_volume=&g_volumes[0]; assert(ext2_mount()==0);
    assert(storage_inode_refresh(&node)==-116);
    FILE *f=fopen(argv[2],"wb"); assert(f);
    assert(fwrite(g_volumes[0].ram_base,1,g_volumes[0].ram_bytes,f)==g_volumes[0].ram_bytes); fclose(f);
    for (unsigned i=0;i<2;i++) { storage_ext4_journal_close(&g_volumes[i]); free(g_volumes[i].ram_base); }
    puts("PASS VFS ext-family namespace, held unlink/write/truncate, permissions, stale generation");
    return 0;
}
