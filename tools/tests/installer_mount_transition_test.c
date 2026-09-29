#define _GNU_SOURCE
#include <assert.h>
/* glibc declares these as enum members after including linux/mount.h. */
#include <linux/mount.h>
#undef MNT_FORCE
#undef MNT_DETACH
#undef MNT_EXPIRE
#undef UMOUNT_NOFOLLOW
#define main installer_application_main
#define mkdir fake_mkdir
#define lstat fake_lstat
#define mount fake_mount
#define umount2 fake_umount2
#define posix_spawnp fake_posix_spawnp
#define waitpid fake_waitpid
#include "../../userland/apps/installer/main.c"
#undef main
#undef posix_spawnp
#undef waitpid

static int root_mounted, esp_mounted, busy_root, busy_esp, mutations;

int fake_posix_spawnp(pid_t *pid, const char *file,
                      const posix_spawn_file_actions_t *actions,
                      const posix_spawnattr_t *attr,
                      char *const argv[], char *const envp[])
{
    (void)file;
    (void)actions;
    (void)attr;
    (void)argv;
    (void)envp;
    *pid = 1;
    return 0;
}

pid_t fake_waitpid(pid_t pid, int *status, int options)
{
    (void)options;
    if (status) *status = 0;
    return pid;
}

int fake_mkdir(const char *path, mode_t mode) { return 0; }
int fake_lstat(const char *path, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFDIR | 0755;
    return 0;
}
int fake_umount2(const char *path, int flags)
{
    int esp = !strcmp(path, INSTALL_ESP_MOUNT);
    int *mounted = esp ? &esp_mounted : &root_mounted;
    if ((esp ? busy_esp : busy_root) || (!esp && esp_mounted)) {
        errno = EBUSY;
        return -1;
    }
    if (!*mounted) { errno = EINVAL; return -1; }
    *mounted = 0;
    return 0;
}
int fake_mount(const char *source, const char *target, const char *type,
               unsigned long flags, const void *data)
{
    int *mounted = !strcmp(target, INSTALL_ESP_MOUNT) ? &esp_mounted : &root_mounted;
    assert(!*mounted);
    *mounted = 1;
    return 0;
}
int reliefos_block_get_info(const char *path, struct reliefos_block_disk_info *out)
{
    out->sector_size = 512;
    out->sector_count = 8 * 1024 * 1024;
    return 0;
}
int reliefos_block_gpt_initialize(const char *path, int force)
{
    ++mutations;
    /* GPT rereads and filesystem formatting require all target mounts gone. */
    return root_mounted || esp_mounted ? -EBUSY : 0;
}
int reliefos_block_gpt_create(const char *path, uint32_t fs, uint32_t size,
                            const char *name, uint32_t *index)
{ *index = fs == RELIEFOS_BLOCK_FILESYSTEM_FAT32 ? 0 : 1; return 0; }
int reliefos_block_gpt_set_type(const char *path, uint32_t index, uint32_t type) { return 0; }
int reliefos_block_partition_path(const char *path, uint32_t index, char *out, uint32_t cap)
{ snprintf(out, cap, "%sp%u", path, index + 1); return 0; }
int reliefos_block_format(const char *path, uint32_t fs, const char *label)
{ assert(!root_mounted && !esp_mounted); return 0; }
int reliefos_block_partition_uuid(const char *path, uint32_t index, char uuid[37])
{ strcpy(uuid, "00000000-0000-0000-0000-000000000001"); return 0; }
int reliefos_block_list_partitions(const char *path, struct reliefos_block_partition *parts,
                                 uint32_t cap, uint32_t *count)
{
    assert(cap >= 2);
    memset(parts, 0, 2 * sizeof(*parts));
    parts[0].filesystem = RELIEFOS_BLOCK_FILESYSTEM_FAT32;
    parts[1].filesystem = RELIEFOS_BLOCK_FILESYSTEM_EXT4;
    strcpy(parts[0].path, "/dev/sda1");
    strcpy(parts[1].path, "/dev/sda2");
    *count = 2;
    return 0;
}

int main(void)
{
    assert(installer_mount_targets("/dev/sda", 0) == 0);
    assert(root_mounted && esp_mounted);
    assert(installer_mount_targets("/dev/sda", 1) == 0);
    assert(root_mounted && esp_mounted && mutations == 1);
    assert(installer_mount_targets("/dev/sda", 0) == 0);
    busy_esp = 1;
    assert(installer_mount_targets("/dev/sda", 1) == -EBUSY);
    assert(root_mounted && esp_mounted && mutations == 1);
    busy_esp = 0;
    busy_root = 1;
    assert(installer_mount_targets("/dev/sda", 1) == -EBUSY);
    assert(root_mounted && !esp_mounted && mutations == 1);
    busy_root = 0;
    assert(installer_mount_targets("/dev/sda", 1) == 0);
    assert(root_mounted && esp_mounted && mutations == 2);
    puts("PASS installer update/fresh mounts, busy abort and retry");
    return 0;
}
