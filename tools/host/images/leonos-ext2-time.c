/* Normalize imported inode timestamps using the upstream ext2fs library. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ext2fs/ext2fs.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: leonos-ext2-time IMAGE EPOCH\n");
        return 2;
    }
    char *end;
    errno = 0;
    unsigned long epoch = strtoul(argv[2], &end, 10);
    if (errno || !*argv[2] || *end || epoch > UINT32_MAX)
        return 2;
    ext2_filsys fs = NULL;
    errcode_t error =
        ext2fs_open(argv[1], EXT2_FLAG_RW, 0, 0, unix_io_manager, &fs);
    if (error) {
        fprintf(stderr, "ext2 open: %ld\n", (long)error);
        return 1;
    }
    error = ext2fs_read_inode_bitmap(fs);
    for (uint64_t index = 1; !error && index <= fs->super->s_inodes_count;
         ++index) {
        ext2_ino_t number = (ext2_ino_t)index;
        if (!ext2fs_test_inode_bitmap2(fs->inode_map, number))
            continue;
        struct ext2_inode inode;
        error = ext2fs_read_inode(fs, number, &inode);
        if (error)
            break;
        if (!inode.i_mode)
            continue; /* Reserved, unused inode. */
        inode.i_atime = (uint32_t)epoch;
        inode.i_mtime = (uint32_t)epoch;
        inode.i_ctime = (uint32_t)epoch;
        error = ext2fs_write_inode(fs, number, &inode);
    }
    if (error) {
        fprintf(stderr, "ext2 inode update: %ld\n", (long)error);
        ext2fs_free(fs);
        return 1;
    }
    fs->super->s_wtime = (uint32_t)epoch;
    fs->now = (time_t)epoch;
    error = ext2fs_close(fs);
    if (error)
        fprintf(stderr, "ext2 close: %ld\n", (long)error);
    return error ? 1 : 0;
}
