#define main permissions_fixture_main
#include "linux_permissions_test.c"
#undef main

int main(void)
{
    struct task task = {.cap_effective = UINT64_MAX};
    strcpy(task.root_dir, "/data");
    strcpy(task.cwd, "/data/dir");
    char path[LEONOS_FS_PATH_LEN];
    assert(fs_permissions_resolve(&task, task.cwd, "/file", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    assert(fs_permissions_resolve(&task, task.cwd, "../../../../file", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    find("/data/link")->target = "/file";
    assert(fs_permissions_resolve(&task, task.cwd, "/link", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    find("/data/link")->target = "../../file";
    assert(fs_permissions_resolve(&task, task.cwd, "/link", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    assert(fs_permissions_resolve_flags(&task, task.cwd, "/link", path, sizeof(path), false, 0) == 0);
    assert(!strcmp(path, "/data/link"));
    assert(fs_permissions_resolve_flags(&task, task.cwd, "/../new", path, sizeof(path), false, FS_LOOKUP_PARENT) == 0);
    assert(!strcmp(path, "/data/new"));
    /* Like Linux, chroot does not revoke preexisting outside directory FDs. */
    assert(fs_permissions_resolve(&task, "/", "data/file", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    struct task_fs_state shared = {0};
    strcpy(shared.root_dir, "/data");
    task.root_dir[0] = 0;
    task.shared_fs = &shared;
    assert(fs_permissions_resolve(&task, "/", "/file", path, sizeof(path), false) == 0);
    assert(!strcmp(path, "/data/file"));
    puts("PASS rooted paths: absolute, relative, dotdot, links, dirfd and shared FS");
}
